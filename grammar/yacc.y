/*
 * ============================================================================
 * yacc.y — MiniC 文法与 AST 语义动作（SeuYacc 输入）
 * ============================================================================
 *
 * 【文法层次】program → global_decl* → var_decl | func_def
 *              func_def → type id (params) compound_stmt
 *              stmt → if | while | return | break | continue | expr_stmt | block
 *              expr → assign → logic_or → ... → primary（运算符优先级由 %left 声明）
 *
 * 【AST 构建约定】
 *   adopt(void*& p) — 从归约槽接管 unique_ptr，清空槽防 double-free
 *   grab(void*& p)  — 取走裸指针供 BinaryOp 等构造，多槽时先 grab 高下标
 *   $$ 与 $1 同槽（val_stack[sp]）；赋值等须 adopt($3) 后再 adopt($1)
 *   MultiNode 容器 — Program/CompoundStmt/StmtList 等用 children 列表
 *
 * 【悬空指针】yacc.y 中 FuncDef 子节点顺序：Type, Identifier, ParamList?, CompoundStmt
 * ============================================================================
 */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_defs.h"
#include "seulex.h"
#include "ast.h"
#include "type.h"
#include <memory>

extern int yylineno;   /* 当前行号（词法器维护） */
extern char* yytext;   /* 当前记号文本 */
void yyerror(const char* s);

/* 全局 AST 根节点，分析成功后由 program 规则赋值 */
ASTNode* astRoot = nullptr;

/** 语法错误计数（含 error 恢复后继续分析时的报错） */
int parseErrorCount = 0;

/** 移进时已构造 IdentifierNode；取走名字并释放临时节点 */
static std::string takeIdentName(void*& p) {
    auto* id = static_cast<IdentifierNode*>(p);
    std::string name = id->name;
    delete id;
    p = nullptr;
    return name;
}

/* 从 void* 槽接管 AST 所有权，并清空槽位防止重复释放 */
static std::unique_ptr<ASTNode> adopt(void*& p) {
    ASTNode* n = static_cast<ASTNode*>(p);
    p = nullptr;
    return std::unique_ptr<ASTNode>(n);
}

// 从栈槽取走 AST 指针（先保存再清空；多槽归约时须先 grab 较大下标，避免与 $$ 重叠）
static ASTNode* grab(void*& p) {
    ASTNode* n = static_cast<ASTNode*>(p);
    p = nullptr;
    return n;
}

static void grabPair(void*& p1, void*& p3, ASTNode*& outL, ASTNode*& outR) {
    outR = grab(p3);
    outL = grab(p1);
}

/** 三目归约：先 grab 高下标槽，避免 $$ 与 $1 同槽时覆盖 */
static void grabTriple(void*& p1, void*& p3, void*& p5, ASTNode*& a, ASTNode*& b, ASTNode*& c) {
    c = grab(p5);
    b = grab(p3);
    a = grab(p1);
}

static void grabQuad(void*& p1, void*& p3, void*& p5, void*& p7,
                     ASTNode*& a, ASTNode*& b, ASTNode*& c, ASTNode*& d) {
    d = grab(p7);
    c = grab(p5);
    b = grab(p3);
    a = grab(p1);
}

/* 结构体成员声明符：标识符 + 可选指针/数组修饰 */
struct FieldDeclarator {
    std::string name;
    bool isPointer = false;
    int arraySize = -1;
};

enum DeclModKind { DECL_PTR, DECL_ARRAY, DECL_FUNC };

struct DeclMod {
    DeclModKind kind;
    int arraySize = -1;
    MultiNode* paramList = nullptr;
    DeclMod() : kind(DECL_PTR), arraySize(-1), paramList(nullptr) {}
    DeclMod(DeclModKind k, int sz, MultiNode* pl) : kind(k), arraySize(sz), paramList(pl) {}
};

struct DeclaratorInfo {
    std::string name;
    std::vector<DeclMod> mods;

    void prependPtr() { mods.insert(mods.begin(), DeclMod(DECL_PTR, -1, nullptr)); }
    void appendArray(int sz) { mods.push_back(DeclMod(DECL_ARRAY, sz, nullptr)); }
    void appendFunc(MultiNode* plist) { mods.push_back(DeclMod(DECL_FUNC, -1, plist)); }

    static void fillFunctionParams(FunctionType* ft, MultiNode* plist) {
        if (!plist) return;
        for (auto& pch : plist->children) {
            auto* p = (MultiNode*)pch.get();
            if (p->children.size() >= 1)
                ft->addParam(((TypeNode*)p->children[0].get())->type);
        }
    }

    Type* buildType(Type* base) const {
        Type* t = base;
        for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
            switch (it->kind) {
            case DECL_FUNC: {
                FunctionType* ft = new FunctionType(base);
                fillFunctionParams(ft, it->paramList);
                t = ft;
                break;
            }
            case DECL_PTR:
                t = new PointerType(t);
                break;
            case DECL_ARRAY:
                t = new ArrayType(t, it->arraySize);
                break;
            }
        }
        return t;
    }
};

static Type* buildFieldType(Type* base, FieldDeclarator* fd) {
    Type* t = base;
    if (fd->isPointer) t = new PointerType(t);
    if (fd->arraySize >= 0) t = new ArrayType(t, fd->arraySize);
    return t;
}

/** 构造 VarDecl：children 为 Type、Identifier，可选初值表达式 */
static MultiNode* makeVarDeclNode(Type* type, const std::string& name, int line,
                                  ASTNode* initExpr = nullptr) {
    auto* node = new MultiNode(NodeKind::VarDecl, line);
    node->addChild(std::unique_ptr<ASTNode>(new TypeNode(type, line)));
    node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, line)));
    if (initExpr)
        node->addChild(std::unique_ptr<ASTNode>(initExpr));
    return node;
}

%}

/* 语义值联合体：与 common_defs.h 中 YYSTYPE 字段对应 */
%union {
    int ival;      /* 整型常量等 */
    double fval;   /* 浮点常量 */
    char *sval;    /* 标识符、字符串（需 free） */
    void *ptr;     /* ASTNode* 或 Type* */
}

/* ---------- 终结符声明（MiniC 子集，与 lex.l / common_defs.h 一致） ---------- */
%token T_EOF
%token T_BREAK T_CASE T_CHAR T_CONTINUE T_DEFAULT T_DOUBLE T_ELSE T_FLOAT T_FOR
%token T_IF T_INT T_RETURN T_STRUCT T_SWITCH T_UNION T_VOID T_WHILE

%token <sval> T_IDENTIFIER
%token <ival> T_CONSTANT
%token <fval> T_FLOAT_CONSTANT
%token <sval> T_STRING_LITERAL

%token T_PTR_OP T_AND_OP T_OR_OP T_LE_OP T_GE_OP T_EQ_OP T_NE_OP

%token '+' '-' '*' '/' '=' '<' '>' '!' '(' ')' '{' '}' '[' ']' ';' ',' '&' '.'

#define T_PLUS '+'
#define T_MINUS '-'
#define T_MULT '*'
#define T_DIV '/'
#define T_ASSIGN '='
#define T_LT '<'
#define T_GT '>'
#define T_NOT '!'
#define T_OR T_OR_OP
#define T_AND T_AND_OP
#define T_EQ T_EQ_OP
#define T_NE T_NE_OP
#define T_LE T_LE_OP
#define T_GE T_GE_OP

/* 携带 AST/类型指针的非终结符 */
%type <ptr> program global_decl var_decl struct_def union_def struct_field_list field_decl field_declarator func_def type_spec
%type <ptr> param_list param_decl param_decl_list declarator direct_declarator
%type <ptr> compound_stmt block_items block_item stmt matched_stmt unmatched_stmt
%type <ptr> expr_stmt while_stmt for_stmt for_init for_cond for_step switch_stmt switch_cases case_block default_block return_stmt break_stmt continue_stmt
%type <ptr> expression assign_expr logic_or_expr logic_and_expr equality_expr
%type <ptr> relational_expr additive_expr multiplicative_expr unary_expr postfix_expr primary_expr
%type <ptr> stmt_list arg_list

/* ---------- 运算符优先级与结合性 ---------- */
%nonassoc LOWER_THAN_ELSE   /* matched 语句在 else 前归约优先级更低 */
%nonassoc T_ELSE
%right T_ASSIGN
%left T_OR
%left T_AND
%left T_EQ T_NE
%left T_LT T_LE T_GT T_GE
%left T_PLUS T_MINUS
%left T_MULT T_DIV
%right T_NOT UMINUS
%left '.' T_PTR_OP

%start program

%%

/* ==================== 程序结构：翻译单元 ==================== */
program:
    global_decl {
        auto* prog = new MultiNode(NodeKind::Program, yylineno);
        prog->addChild(adopt($1.ptr));
        $$.ptr = prog;
        astRoot = prog;
    }
    | program global_decl {
        MultiNode* prog = (MultiNode*)$1.ptr;
        prog->addChild(adopt($2.ptr));
        $$.ptr = prog;
        astRoot = prog;
    }
    ;

/* ==================== 顶层声明：变量、结构体或函数 ==================== */
global_decl:
    var_decl   { }
    | struct_def { }
    | union_def { }
    | func_def { }
    ;

/* ==================== 结构体定义 ==================== */
struct_def:
    T_STRUCT T_IDENTIFIER '{' '}' ';' {
        std::string tag = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::StructDef, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(tag, yylineno)));
        $$.ptr = node;
    }
    | T_STRUCT T_IDENTIFIER '{' struct_field_list '}' ';' {
        std::string tag = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::StructDef, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(tag, yylineno)));
        MultiNode* fields = (MultiNode*)$4.ptr;
        for (auto& ch : fields->children)
            node->addChild(std::move(ch));
        delete fields;
        $$.ptr = node;
    }
    ;

/* ==================== 联合体定义 ==================== */
union_def:
    T_UNION T_IDENTIFIER '{' '}' ';' {
        std::string tag = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::UnionDef, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(tag, yylineno)));
        $$.ptr = node;
    }
    | T_UNION T_IDENTIFIER '{' struct_field_list '}' ';' {
        std::string tag = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::UnionDef, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(tag, yylineno)));
        MultiNode* fields = (MultiNode*)$4.ptr;
        for (auto& ch : fields->children)
            node->addChild(std::move(ch));
        delete fields;
        $$.ptr = node;
    }
    ;

/* 至少一个成员，无 ε 产生式，避免与类型关键字 shift 冲突 */
struct_field_list:
    field_decl {
        auto* list = new MultiNode(NodeKind::FieldList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | struct_field_list field_decl {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($2.ptr));
        $$.ptr = list;
    }
    ;

field_decl:
    type_spec field_declarator ';' {
        FieldDeclarator* fd = (FieldDeclarator*)$2.ptr;
        Type* ftype = buildFieldType((Type*)$1.ptr, fd);
        auto* node = new MultiNode(NodeKind::FieldDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(ftype, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(fd->name, yylineno)));
        delete fd;
        $$.ptr = node;
    }
    ;

field_declarator:
    T_IDENTIFIER {
        std::string fname = takeIdentName($1.ptr);
        auto* fd = new FieldDeclarator();
        fd->name = fname;
        $$.ptr = fd;
    }
    | '*' T_IDENTIFIER {
        std::string fname = takeIdentName($2.ptr);
        auto* fd = new FieldDeclarator();
        fd->name = fname;
        fd->isPointer = true;
        $$.ptr = fd;
    }
    | T_IDENTIFIER '[' T_CONSTANT ']' {
        std::string fname = takeIdentName($1.ptr);
        auto* fd = new FieldDeclarator();
        fd->name = fname;
        fd->arraySize = $3.ival;
        $$.ptr = fd;
    }
    | '*' T_IDENTIFIER '[' T_CONSTANT ']' {
        std::string fname = takeIdentName($2.ptr);
        auto* fd = new FieldDeclarator();
        fd->name = fname;
        fd->isPointer = true;
        fd->arraySize = $4.ival;
        $$.ptr = fd;
    }
    ;

/* ==================== 类型说明符 ==================== */
type_spec:
    T_INT   { $$.ptr = (void*)BasicType::Int; }
    | T_CHAR { $$.ptr = (void*)BasicType::Char; }
    | T_FLOAT { $$.ptr = (void*)BasicType::Float; }
    | T_DOUBLE { $$.ptr = (void*)BasicType::Double; }
    | T_VOID { $$.ptr = (void*)BasicType::Void; }
    | T_STRUCT T_IDENTIFIER {
        std::string tag = takeIdentName($2.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        $$.ptr = (void*)st;
    }
    | T_UNION T_IDENTIFIER {
        std::string tag = takeIdentName($2.ptr);
        UnionType* ut = lookupUnionType(tag);
        if (!ut) ut = new UnionType(tag);
        $$.ptr = (void*)ut;
    }
    ;

/* ==================== 全局/局部变量声明 ==================== */
var_decl:
    T_INT T_IDENTIFIER ';' {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Int, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT T_IDENTIFIER '[' T_CONSTANT ']' ';' {
        std::string name = takeIdentName($2.ptr);
        auto* arrType = new ArrayType(BasicType::Int, $4.ival);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(arrType, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT '*' T_IDENTIFIER ';' {
        std::string name = takeIdentName($3.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(BasicType::Int), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT '*' T_IDENTIFIER '[' T_CONSTANT ']' ';' {
        std::string name = takeIdentName($3.ptr);
        auto* pt = new PointerType(BasicType::Int);
        auto* arrType = new ArrayType(pt, $5.ival);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(arrType, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT '(' '*' T_IDENTIFIER ')' '(' param_list ')' ';' {
        std::string name = takeIdentName($4.ptr);
        FunctionType* ft = new FunctionType(BasicType::Int);
        if ($7.ptr) DeclaratorInfo::fillFunctionParams(ft, (MultiNode*)$7.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(ft), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_FLOAT T_IDENTIFIER ';' {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Float, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_FLOAT T_IDENTIFIER '[' T_CONSTANT ']' ';' {
        std::string name = takeIdentName($2.ptr);
        auto* arrType = new ArrayType(BasicType::Float, $4.ival);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(arrType, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_FLOAT '*' T_IDENTIFIER ';' {
        std::string name = takeIdentName($3.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(BasicType::Float), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_DOUBLE T_IDENTIFIER ';' {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Double, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_DOUBLE T_IDENTIFIER '[' T_CONSTANT ']' ';' {
        std::string name = takeIdentName($2.ptr);
        auto* arrType = new ArrayType(BasicType::Double, $4.ival);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(arrType, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_CHAR T_IDENTIFIER ';' {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Char, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_CHAR '*' T_IDENTIFIER ';' {
        std::string name = takeIdentName($3.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(BasicType::Char), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_STRUCT T_IDENTIFIER T_IDENTIFIER ';' {
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($3.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(st, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_STRUCT T_IDENTIFIER '*' T_IDENTIFIER ';' {
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($4.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(st), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_STRUCT T_IDENTIFIER '*' T_IDENTIFIER '[' T_CONSTANT ']' ';' {
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($4.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        auto* pt = new PointerType(st);
        auto* arrType = new ArrayType(pt, $6.ival);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(arrType, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_UNION T_IDENTIFIER T_IDENTIFIER ';' {
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($3.ptr);
        UnionType* ut = lookupUnionType(tag);
        if (!ut) ut = new UnionType(tag);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(ut, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_UNION T_IDENTIFIER '*' T_IDENTIFIER ';' {
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($4.ptr);
        UnionType* ut = lookupUnionType(tag);
        if (!ut) ut = new UnionType(tag);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(new PointerType(ut), yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT T_IDENTIFIER '=' expression ';' {
        auto init = adopt($4.ptr);
        std::string name = takeIdentName($2.ptr);
        $$.ptr = makeVarDeclNode(BasicType::Int, name, yylineno, init.release());
    }
    | T_INT T_IDENTIFIER '[' T_CONSTANT ']' '=' expression ';' {
        auto init = adopt($7.ptr);
        std::string name = takeIdentName($2.ptr);
        auto* arrType = new ArrayType(BasicType::Int, $4.ival);
        $$.ptr = makeVarDeclNode(arrType, name, yylineno, init.release());
    }
    | T_FLOAT T_IDENTIFIER '=' expression ';' {
        auto init = adopt($4.ptr);
        std::string name = takeIdentName($2.ptr);
        $$.ptr = makeVarDeclNode(BasicType::Float, name, yylineno, init.release());
    }
    | T_DOUBLE T_IDENTIFIER '=' expression ';' {
        auto init = adopt($4.ptr);
        std::string name = takeIdentName($2.ptr);
        $$.ptr = makeVarDeclNode(BasicType::Double, name, yylineno, init.release());
    }
    | T_CHAR T_IDENTIFIER '=' expression ';' {
        auto init = adopt($4.ptr);
        std::string name = takeIdentName($2.ptr);
        $$.ptr = makeVarDeclNode(BasicType::Char, name, yylineno, init.release());
    }
    | T_INT '*' T_IDENTIFIER '=' expression ';' {
        auto init = adopt($5.ptr);
        std::string name = takeIdentName($3.ptr);
        $$.ptr = makeVarDeclNode(new PointerType(BasicType::Int), name, yylineno, init.release());
    }
    | T_FLOAT '*' T_IDENTIFIER '=' expression ';' {
        auto init = adopt($5.ptr);
        std::string name = takeIdentName($3.ptr);
        $$.ptr = makeVarDeclNode(new PointerType(BasicType::Float), name, yylineno, init.release());
    }
    | T_CHAR '*' T_IDENTIFIER '=' expression ';' {
        auto init = adopt($5.ptr);
        std::string name = takeIdentName($3.ptr);
        $$.ptr = makeVarDeclNode(new PointerType(BasicType::Char), name, yylineno, init.release());
    }
    | T_STRUCT T_IDENTIFIER T_IDENTIFIER '=' expression ';' {
        auto init = adopt($5.ptr);
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($3.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        $$.ptr = makeVarDeclNode(st, name, yylineno, init.release());
    }
    | T_STRUCT T_IDENTIFIER '*' T_IDENTIFIER '=' expression ';' {
        auto init = adopt($6.ptr);
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($4.ptr);
        StructType* st = lookupStructType(tag);
        if (!st) st = new StructType(tag);
        $$.ptr = makeVarDeclNode(new PointerType(st), name, yylineno, init.release());
    }
    | T_UNION T_IDENTIFIER T_IDENTIFIER '=' expression ';' {
        auto init = adopt($5.ptr);
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($3.ptr);
        UnionType* ut = lookupUnionType(tag);
        if (!ut) ut = new UnionType(tag);
        $$.ptr = makeVarDeclNode(ut, name, yylineno, init.release());
    }
    | T_UNION T_IDENTIFIER '*' T_IDENTIFIER '=' expression ';' {
        auto init = adopt($6.ptr);
        std::string tag = takeIdentName($2.ptr);
        std::string name = takeIdentName($4.ptr);
        UnionType* ut = lookupUnionType(tag);
        if (!ut) ut = new UnionType(tag);
        $$.ptr = makeVarDeclNode(new PointerType(ut), name, yylineno, init.release());
    }
    ;

/* ==================== 函数定义（含形参表与复合语句体） ==================== */
func_def:
    T_INT T_IDENTIFIER '(' param_list ')' compound_stmt {
        std::string fname = takeIdentName($2.ptr);
        auto* func = new MultiNode(NodeKind::FuncDef, yylineno);
        func->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Int, yylineno)));
        func->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(fname, yylineno)));
        if ($4.ptr) func->addChild(adopt($4.ptr));
        func->addChild(adopt($6.ptr));
        $$.ptr = func;
    }
    | T_FLOAT T_IDENTIFIER '(' param_list ')' compound_stmt {
        std::string fname = takeIdentName($2.ptr);
        auto* func = new MultiNode(NodeKind::FuncDef, yylineno);
        func->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Float, yylineno)));
        func->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(fname, yylineno)));
        if ($4.ptr) func->addChild(adopt($4.ptr));
        func->addChild(adopt($6.ptr));
        $$.ptr = func;
    }
    | T_DOUBLE T_IDENTIFIER '(' param_list ')' compound_stmt {
        std::string fname = takeIdentName($2.ptr);
        auto* func = new MultiNode(NodeKind::FuncDef, yylineno);
        func->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Double, yylineno)));
        func->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(fname, yylineno)));
        if ($4.ptr) func->addChild(adopt($4.ptr));
        func->addChild(adopt($6.ptr));
        $$.ptr = func;
    }
    | T_VOID T_IDENTIFIER '(' param_list ')' compound_stmt {
        std::string fname = takeIdentName($2.ptr);
        auto* func = new MultiNode(NodeKind::FuncDef, yylineno);
        func->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Void, yylineno)));
        func->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(fname, yylineno)));
        if ($4.ptr) func->addChild(adopt($4.ptr));
        func->addChild(adopt($6.ptr));
        $$.ptr = func;
    }
    ;

/* ==================== 形参列表 ==================== */
param_list:
    /* empty */ { $$.ptr = nullptr; }
    | T_VOID { $$.ptr = nullptr; }
    | param_decl_list { }
    ;

param_decl_list:
    param_decl {
        auto* list = new MultiNode(NodeKind::ParamList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | param_decl_list ',' param_decl {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($3.ptr));
        $$.ptr = list;
    }
    ;

param_decl:
    type_spec declarator {
        DeclaratorInfo* d = (DeclaratorInfo*)$2.ptr;
        Type* ptype = d->buildType((Type*)$1.ptr);
        auto* param = new MultiNode(NodeKind::ParamDecl, yylineno);
        param->addChild(std::unique_ptr<ASTNode>(new TypeNode(ptype, yylineno)));
        param->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(d->name, yylineno)));
        delete d;
        $$.ptr = param;
    }
    ;

/* ==================== 声明符（指针、数组、函数指针） ==================== */
declarator:
    direct_declarator { $$.ptr = $1.ptr; }
    | '*' declarator {
        DeclaratorInfo* d = (DeclaratorInfo*)$2.ptr;
        d->prependPtr();
        $$.ptr = d;
    }
    ;

direct_declarator:
    T_IDENTIFIER {
        std::string name = takeIdentName($1.ptr);
        auto* d = new DeclaratorInfo();
        d->name = name;
        $$.ptr = d;
    }
    | '(' declarator ')' {
        $$.ptr = $2.ptr;
    }
    | direct_declarator '[' T_CONSTANT ']' {
        DeclaratorInfo* d = (DeclaratorInfo*)$1.ptr;
        d->appendArray($3.ival);
        $$.ptr = d;
    }
    | direct_declarator '(' param_list ')' {
        DeclaratorInfo* d = (DeclaratorInfo*)$1.ptr;
        d->appendFunc($3.ptr ? (MultiNode*)$3.ptr : nullptr);
        $$.ptr = d;
    }
    ;

/* ==================== 复合语句：空块 / 非空块（block_item 无 ε 列表） ==================== */
compound_stmt:
    '{' '}' {
        $$.ptr = new MultiNode(NodeKind::CompoundStmt, yylineno);
    }
    | '{' block_items '}' {
        auto* block = new MultiNode(NodeKind::CompoundStmt, yylineno);
        MultiNode* items = (MultiNode*)$2.ptr;
        for (auto& ch : items->children)
            block->addChild(std::move(ch));
        delete items;
        $$.ptr = block;
    }
    ;

block_items:
    block_item {
        auto* list = new MultiNode(NodeKind::StmtList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | block_items block_item {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($2.ptr));
        $$.ptr = list;
    }
    ;

block_item:
    var_decl { $$.ptr = $1.ptr; }
    | stmt { $$.ptr = $1.ptr; }
    ;

/* ==================== 语句列表与各类语句 ==================== */
stmt_list:
    /* empty */ { $$.ptr = nullptr; }
    | stmt_list stmt {
        if (!$1.ptr) {
            auto* list = new MultiNode(NodeKind::StmtList, yylineno);
            list->addChild(adopt($2.ptr));
            $$.ptr = list;
        } else {
            MultiNode* list = (MultiNode*)$1.ptr;
            list->addChild(adopt($2.ptr));
            $$.ptr = list;
        }
    }
    ;

stmt:
    matched_stmt %prec LOWER_THAN_ELSE { }
    | unmatched_stmt { }
    | error ';' { yyerrok; $$.ptr = nullptr; }  /* 错误恢复：吞掉至分号 */
    ;

matched_stmt:
    expr_stmt %prec LOWER_THAN_ELSE { }
    | compound_stmt %prec LOWER_THAN_ELSE { }
    | while_stmt %prec LOWER_THAN_ELSE { }
    | for_stmt %prec LOWER_THAN_ELSE { }
    | switch_stmt %prec LOWER_THAN_ELSE { }
    | return_stmt %prec LOWER_THAN_ELSE { }
    | break_stmt %prec LOWER_THAN_ELSE { }
    | continue_stmt %prec LOWER_THAN_ELSE { }
    | T_IF '(' expression ')' matched_stmt T_ELSE matched_stmt {
        auto* ifNode = new MultiNode(NodeKind::IfStmt, yylineno);
        ASTNode *cond, *thenB, *elseB;
        grabTriple($3.ptr, $5.ptr, $7.ptr, cond, thenB, elseB);
        ifNode->addChild(std::unique_ptr<ASTNode>(cond));
        ifNode->addChild(std::unique_ptr<ASTNode>(thenB));
        ifNode->addChild(std::unique_ptr<ASTNode>(elseB));
        $$.ptr = ifNode;
    }
    ;

unmatched_stmt:
    T_IF '(' expression ')' stmt %prec LOWER_THAN_ELSE {
        auto* ifNode = new MultiNode(NodeKind::IfStmt, yylineno);
        ASTNode *cond, *thenB;
        grabPair($3.ptr, $5.ptr, cond, thenB);
        ifNode->addChild(std::unique_ptr<ASTNode>(cond));
        ifNode->addChild(std::unique_ptr<ASTNode>(thenB));
        $$.ptr = ifNode;
    }
    | T_IF '(' expression ')' matched_stmt T_ELSE unmatched_stmt {
        auto* ifNode = new MultiNode(NodeKind::IfStmt, yylineno);
        ASTNode *cond, *thenB, *elseB;
        grabTriple($3.ptr, $5.ptr, $7.ptr, cond, thenB, elseB);
        ifNode->addChild(std::unique_ptr<ASTNode>(cond));
        ifNode->addChild(std::unique_ptr<ASTNode>(thenB));
        ifNode->addChild(std::unique_ptr<ASTNode>(elseB));
        $$.ptr = ifNode;
    }
    ;

expr_stmt:
    expression ';' {
        auto* exprStmt = new MultiNode(NodeKind::ExprStmt, yylineno);
        exprStmt->addChild(adopt($1.ptr));
        $$.ptr = exprStmt;
    }
    | ';' {
        $$.ptr = new MultiNode(NodeKind::NullStmt, yylineno);
    }
    ;

while_stmt:
    T_WHILE while_paren_stmt {
        $$.ptr = adopt($2.ptr).release();
    }
    ;

while_paren_stmt:
    '(' expression ')' stmt {
        auto* whileNode = new MultiNode(NodeKind::WhileStmt, yylineno);
        ASTNode *cond, *body;
        grabPair($2.ptr, $4.ptr, cond, body);
        whileNode->addChild(std::unique_ptr<ASTNode>(cond));
        whileNode->addChild(std::unique_ptr<ASTNode>(body));
        $$.ptr = whileNode;
    }
    ;

for_stmt:
    T_FOR '(' for_init ';' for_cond ';' for_step ')' stmt {
        auto* forNode = new MultiNode(NodeKind::ForStmt, yylineno);
        ASTNode *init, *cond, *step, *body;
        grabQuad($3.ptr, $5.ptr, $7.ptr, $9.ptr, init, cond, step, body);
        forNode->addChild(std::unique_ptr<ASTNode>(init));
        forNode->addChild(std::unique_ptr<ASTNode>(cond));
        forNode->addChild(std::unique_ptr<ASTNode>(step));
        forNode->addChild(std::unique_ptr<ASTNode>(body));
        $$.ptr = forNode;
    }
    ;

for_init:
    /* empty */ {
        $$.ptr = new MultiNode(NodeKind::NullStmt, yylineno);
    }
    | expression {
        auto* init = new MultiNode(NodeKind::ExprStmt, yylineno);
        init->addChild(adopt($1.ptr));
        $$.ptr = init;
    }
    | T_INT T_IDENTIFIER {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Int, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        $$.ptr = node;
    }
    | T_INT T_IDENTIFIER '=' expression {
        std::string name = takeIdentName($2.ptr);
        auto* node = new MultiNode(NodeKind::VarDecl, yylineno);
        node->addChild(std::unique_ptr<ASTNode>(new TypeNode(BasicType::Int, yylineno)));
        node->addChild(std::unique_ptr<ASTNode>(new IdentifierNode(name, yylineno)));
        node->addChild(adopt($4.ptr));
        $$.ptr = node;
    }
    ;

for_cond:
    /* empty */ {
        $$.ptr = nullptr;
    }
    | expression { }
    ;

for_step:
    /* empty */ {
        $$.ptr = new MultiNode(NodeKind::NullStmt, yylineno);
    }
    | expression {
        auto* step = new MultiNode(NodeKind::ExprStmt, yylineno);
        step->addChild(adopt($1.ptr));
        $$.ptr = step;
    }
    ;

switch_stmt:
    T_SWITCH '(' expression ')' '{' '}' {
        auto* sw = new MultiNode(NodeKind::SwitchStmt, yylineno);
        sw->addChild(adopt($3.ptr));
        $$.ptr = sw;
    }
    | T_SWITCH '(' expression ')' '{' switch_cases '}' {
        auto* sw = new MultiNode(NodeKind::SwitchStmt, yylineno);
        sw->addChild(adopt($3.ptr));
        MultiNode* cases = (MultiNode*)$6.ptr;
        for (auto& ch : cases->children)
            sw->addChild(std::move(ch));
        delete cases;
        $$.ptr = sw;
    }
    ;

switch_cases:
    case_block {
        auto* list = new MultiNode(NodeKind::CaseList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | default_block {
        auto* list = new MultiNode(NodeKind::CaseList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | switch_cases case_block {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($2.ptr));
        $$.ptr = list;
    }
    | switch_cases default_block {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($2.ptr));
        $$.ptr = list;
    }
    ;

case_block:
    T_CASE T_CONSTANT ':' stmt_list {
        auto* caseNode = new MultiNode(NodeKind::CaseStmt, yylineno);
        caseNode->addChild(std::unique_ptr<ASTNode>(new IntegerNode($2.ival, yylineno)));
        if ($4.ptr) {
            MultiNode* stmts = (MultiNode*)$4.ptr;
            for (auto& ch : stmts->children)
                caseNode->addChild(std::move(ch));
            delete stmts;
        }
        $$.ptr = caseNode;
    }
    ;

default_block:
    T_DEFAULT ':' stmt_list {
        auto* defNode = new MultiNode(NodeKind::DefaultStmt, yylineno);
        if ($3.ptr) {
            MultiNode* stmts = (MultiNode*)$3.ptr;
            for (auto& ch : stmts->children)
                defNode->addChild(std::move(ch));
            delete stmts;
        }
        $$.ptr = defNode;
    }
    ;

return_stmt:
    T_RETURN ';' {
        $$.ptr = new MultiNode(NodeKind::ReturnStmt, yylineno);
    }
    | T_RETURN expression ';' {
        auto* retNode = new MultiNode(NodeKind::ReturnStmt, yylineno);
        retNode->addChild(adopt($2.ptr));
        $$.ptr = retNode;
    }
    ;

break_stmt:
    T_BREAK ';' {
        $$.ptr = new MultiNode(NodeKind::BreakStmt, yylineno);
    }
    ;

continue_stmt:
    T_CONTINUE ';' {
        $$.ptr = new MultiNode(NodeKind::ContinueStmt, yylineno);
    }
    ;

/* ==================== 表达式（按优先级分层） ==================== */
expression:
    assign_expr { }
    ;

assign_expr:
    logic_or_expr { }
    | postfix_expr '=' assign_expr {
        auto r = adopt($3.ptr);
        auto l = adopt($1.ptr);
        $$.ptr = new AssignOpNode(std::move(l), std::move(r), yylineno);
    }
    | '*' postfix_expr '=' assign_expr %prec UMINUS {
        auto r = adopt($4.ptr);
        auto p = adopt($2.ptr);
        auto l = std::unique_ptr<ASTNode>(new UnaryOpNode('*', std::move(p), yylineno));
        $$.ptr = new AssignOpNode(std::move(l), std::move(r), yylineno);
    }
    | postfix_expr '=' error { yyerrok; $$.ptr = nullptr; }  /* 赋值右侧错误恢复 */
    ;

logic_or_expr:
    logic_and_expr { }
    | logic_or_expr T_OR logic_and_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_OR, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

logic_and_expr:
    equality_expr { }
    | logic_and_expr T_AND equality_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_AND, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

equality_expr:
    relational_expr { }
    | equality_expr T_EQ relational_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_EQ, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | equality_expr T_NE relational_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_NE, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

relational_expr:
    additive_expr { }
    | relational_expr '<' additive_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_LT, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | relational_expr T_LE additive_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_LE, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | relational_expr '>' additive_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_GT, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | relational_expr T_GE additive_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_GE, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

additive_expr:
    multiplicative_expr { }
    | additive_expr '+' multiplicative_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_PLUS, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | additive_expr '-' multiplicative_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_MINUS, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

multiplicative_expr:
    unary_expr { }
    | multiplicative_expr '*' unary_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_MULT, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    | multiplicative_expr '/' unary_expr {
        ASTNode *l, *r;
        grabPair($1.ptr, $3.ptr, l, r);
        $$.ptr = new BinaryOpNode(T_DIV, std::unique_ptr<ASTNode>(l), std::unique_ptr<ASTNode>(r), yylineno);
    }
    ;

unary_expr:
    postfix_expr { }
    | '-' unary_expr %prec UMINUS {
        auto o = adopt($2.ptr);
        $$.ptr = new UnaryOpNode(T_MINUS, std::move(o), yylineno);
    }
    | '!' unary_expr {
        auto o = adopt($2.ptr);
        $$.ptr = new UnaryOpNode(T_NOT, std::move(o), yylineno);
    }
    | '&' unary_expr {
        auto o = adopt($2.ptr);
        $$.ptr = new UnaryOpNode('&', std::move(o), yylineno);
    }
    | '*' unary_expr %prec UMINUS {
        auto o = adopt($2.ptr);
        $$.ptr = new UnaryOpNode('*', std::move(o), yylineno);
    }
    ;

postfix_expr:
    postfix_expr '(' arg_list ')' {
        auto callee = adopt($1.ptr);
        auto* call = new CallNode(std::move(callee), yylineno);
        if ($3.ptr) {
            MultiNode* args = (MultiNode*)$3.ptr;
            for (auto& arg : args->children)
                call->addArg(std::move(arg));
            delete args;
        }
        $$.ptr = call;
    }
    | primary_expr { }
    | postfix_expr '[' expression ']' {
        auto i = adopt($3.ptr);
        auto a = adopt($1.ptr);
        $$.ptr = new ArraySubscriptNode(std::move(a), std::move(i), yylineno);
    }
    | postfix_expr '.' T_IDENTIFIER {
        auto obj = adopt($1.ptr);
        std::string member = takeIdentName($3.ptr);
        $$.ptr = new MemberAccessNode(std::move(obj), member, false, yylineno);
    }
    | postfix_expr T_PTR_OP T_IDENTIFIER {
        auto obj = adopt($1.ptr);
        std::string member = takeIdentName($3.ptr);
        $$.ptr = new MemberAccessNode(std::move(obj), member, true, yylineno);
    }
    ;

primary_expr:
    T_IDENTIFIER {
        $$.ptr = adopt($1.ptr).release();
    }
    | T_CONSTANT {
        $$.ptr = new IntegerNode($1.ival, yylineno);
    }
    | T_FLOAT_CONSTANT {
        $$.ptr = new FloatNode($1.fval, yylineno);
    }
    | T_STRING_LITERAL {
        $$.ptr = adopt($1.ptr).release();
    }
    | '(' expression ')' {
        $$.ptr = adopt($2.ptr).release();
    }
    ;

/* ==================== 函数实参列表 ==================== */
arg_list:
    /* empty */ { $$.ptr = nullptr; }
    | assign_expr {
        auto* list = new MultiNode(NodeKind::ArgList, yylineno);
        list->addChild(adopt($1.ptr));
        $$.ptr = list;
    }
    | arg_list ',' assign_expr {
        MultiNode* list = (MultiNode*)$1.ptr;
        list->addChild(adopt($3.ptr));
        $$.ptr = list;
    }
    ;

%%

/* 默认语法错误处理：输出行号与出错记号，并累计错误数 */
void yyerror(const char* s) {
    ++parseErrorCount;
    fprintf(stderr, "[Syntax error] Line:%d, Error:%s, text:\"%s\"\n",
            yylineno, s, yytext ? yytext : "(null)");
}