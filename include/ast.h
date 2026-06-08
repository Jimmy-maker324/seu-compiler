/**
 * @file ast.h
 * @brief 抽象语法树（AST）节点定义
 *
 * 【整体设计】多叉树 + 专用表达式节点
 *   - MultiNode：语句/声明容器，children 有序列表
 *   - BinaryOp/UnaryOp/AssignOp/Call/ArraySubscript：表达式专用结构
 *   - Identifier/Integer/String：叶子
 *
 * 【构建时机】yyparse 归约时 yacc.y 语义动作 new 节点并挂接
 * 【遍历模式】
 *   - TypeChecker：visit / visitStmt 递归 + 符号表
 *   - IRGenerator：表达式后序生成四元式，语句先序控制流
 *   - ASTPrinter/ASTDotExporter：先序打印/导出
 *
 * 【内存管理】unique_ptr 持有子树；归约槽 void* 经 adopt/grab 转移所有权
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>

/**
 * @enum NodeKind
 * @brief AST 节点种类枚举
 *
 * 涵盖程序结构、声明、语句、表达式及语法分析阶段的临时容器节点。
 */
enum class NodeKind {
    Program, VarDecl, StructDef, UnionDef, FieldDecl, FuncDef, ParamDecl,
    CompoundStmt, IfStmt, WhileStmt, ForStmt, SwitchStmt, CaseStmt, DefaultStmt,
    ReturnStmt, BreakStmt, ContinueStmt,
    ExprStmt, NullStmt, Identifier, Integer, Float, String,
    BinaryOp, UnaryOp, AssignOp, Call, ArraySubscript, MemberAccess, TypeNode,
    ParamList,
    StmtList,  // stmt_list / local_decls 语法归约时的临时容器
    CaseList,  // case_list 语法归约时的临时容器
    FieldList, // struct_field_list 语法归约时的临时容器
    ArgList    // arg_list 语法归约时的临时容器
};

/**
 * @class ASTNode
 * @brief AST 基类，所有节点均继承自此
 *
 * 保存节点种类与源代码行号，便于错误报告与调试。
 */
class ASTNode {
public:
    NodeKind kind;  ///< 节点种类
    int line;       ///< 对应源代码行号（0 表示未知）
    /** @brief 构造指定种类与行号的 AST 节点 */
    explicit ASTNode(NodeKind k, int l = 0) : kind(k), line(l) {}
    virtual ~ASTNode() = default;
};

/**
 * @class MultiNode
 * @brief 多叉树节点，用于语句块、程序、参数列表等含多个子节点的结构
 */
class MultiNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> children;  ///< 子节点列表
    /** @brief 构造多叉节点 */
    MultiNode(NodeKind k, int l = 0) : ASTNode(k, l) {}
    /** @brief 追加一个子节点（转移所有权） */
    void addChild(std::unique_ptr<ASTNode> child) {
        children.push_back(std::move(child));
    }
};

/** @class IdentifierNode @brief 标识符叶子节点（变量名、函数名等） */
class IdentifierNode : public ASTNode {
public:
    std::string name;  ///< 标识符文本
    /** @brief 构造标识符节点 */
    IdentifierNode(const std::string& n, int l = 0) : ASTNode(NodeKind::Identifier, l), name(n) {}
};

/** @class IntegerNode @brief 整型字面量叶子节点 */
class IntegerNode : public ASTNode {
public:
    int value;  ///< 整数值
    /** @brief 构造整型字面量节点 */
    IntegerNode(int v, int l = 0) : ASTNode(NodeKind::Integer, l), value(v) {}
};

/** @class FloatNode @brief 浮点字面量叶子节点 */
class FloatNode : public ASTNode {
public:
    double value;  ///< 浮点数值
    FloatNode(double v, int l = 0) : ASTNode(NodeKind::Float, l), value(v) {}
};

/** @class StringNode @brief 字符串字面量叶子节点 */
class StringNode : public ASTNode {
public:
    std::string value;  ///< 字符串内容（不含引号）
    /** @brief 构造字符串字面量节点 */
    StringNode(const std::string& v, int l = 0) : ASTNode(NodeKind::String, l), value(v) {}
};

/** @class BinaryOpNode @brief 二元运算表达式节点 */
class BinaryOpNode : public ASTNode {
public:
    int op;  ///< 运算符 token 码（见 common_defs.h）
    std::unique_ptr<ASTNode> left, right;  ///< 左、右操作数
    /** @brief 构造二元运算节点 */
    BinaryOpNode(int opcode, std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs, int l = 0)
        : ASTNode(NodeKind::BinaryOp, l), op(opcode), left(std::move(lhs)), right(std::move(rhs)) {}
};

/** @class UnaryOpNode @brief 一元运算表达式节点 */
class UnaryOpNode : public ASTNode {
public:
    int op;  ///< 运算符 token 码
    std::unique_ptr<ASTNode> operand;  ///< 操作数
    /** @brief 构造一元运算节点 */
    UnaryOpNode(int opcode, std::unique_ptr<ASTNode> expr, int l = 0)
        : ASTNode(NodeKind::UnaryOp, l), op(opcode), operand(std::move(expr)) {}
};

/** @class AssignOpNode @brief 赋值表达式节点（左值 = 右值） */
class AssignOpNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> left, right;  ///< 左值、右值子树
    /** @brief 构造赋值节点 */
    AssignOpNode(std::unique_ptr<ASTNode> lhs, std::unique_ptr<ASTNode> rhs, int l = 0)
        : ASTNode(NodeKind::AssignOp, l), left(std::move(lhs)), right(std::move(rhs)) {}
};

/** @class CallNode @brief 函数调用（标识符或函数指针表达式） */
class CallNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> callee;  ///< 被调表达式（标识符/解引用等）
    std::vector<std::unique_ptr<ASTNode>> args;
    explicit CallNode(std::unique_ptr<ASTNode> fn, int l = 0)
        : ASTNode(NodeKind::Call, l), callee(std::move(fn)) {}
    void addArg(std::unique_ptr<ASTNode> arg) { args.push_back(std::move(arg)); }
};

/** @class ArraySubscriptNode @brief 数组/指针下标访问表达式节点 */
class ArraySubscriptNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> array, index;  ///< 数组/指针表达式、下标表达式
    /** @brief 构造下标访问节点 */
    ArraySubscriptNode(std::unique_ptr<ASTNode> arr, std::unique_ptr<ASTNode> idx, int l = 0)
        : ASTNode(NodeKind::ArraySubscript, l), array(std::move(arr)), index(std::move(idx)) {}
};

/** @class MemberAccessNode @brief 结构体/联合体成员访问（obj.member 或 ptr->member） */
class MemberAccessNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> object;
    std::string member;
    bool throughPointer;  ///< true 表示 -> 访问，false 表示 . 访问
    MemberAccessNode(std::unique_ptr<ASTNode> obj, const std::string& mem,
                     bool ptr = false, int l = 0)
        : ASTNode(NodeKind::MemberAccess, l), object(std::move(obj)),
          member(mem), throughPointer(ptr) {}
};

struct Type;  ///< 前向声明，完整定义见 type.h

/**
 * @class TypeNode
 * @brief 类型节点，在声明与函数签名中携带语义类型信息
 */
class TypeNode : public ASTNode {
public:
    Type* type;  ///< 指向类型系统的 Type 对象
    /** @brief 构造类型节点 */
    TypeNode(Type* t, int l = 0) : ASTNode(NodeKind::TypeNode, l), type(t) {}
};

#endif
