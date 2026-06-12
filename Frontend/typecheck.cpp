/**
 * @file typecheck.cpp
 * @brief 语义分析（类型检查）器实现
 *
 * 【算法】单遍自顶向下遍历 AST + 作用域栈符号表
 *
 * 【数据结构】
 *   Scope 链：parent 指针 + unordered_map<name, Symbol*>
 *   Symbol：name, irName, Type*, isFunction
 *
 * 【visitStmt 流程（声明/语句）】
 *   VarDecl：查重 → addSymbol
 *   FuncDef：登记函数类型 → enterScope → 处理形参 → 检查 compound → leaveScope
 *   CompoundStmt：enterScope → 子节点 → leaveScope（函数体最外层块与形参同 scope）
 *   If/While/Return：递归检查条件与分支类型；Return 与当前函数返回类型比对
 *
 * 【visit 表达式规则】
 *   二元运算：数值类型提升
 *   赋值：左值 visitLvalue，类型与右值一致
 *   Call：查符号 isFunction，实参个数与 paramTypes 匹配
 *   下标：数组/指针 base，下标须 int
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#include "typecheck.h"
#include "symbol.h"
#include "type.h"
#include "ast_walk.h"
#include "common_defs.h"
#include <iostream>
#include <iomanip>
#include <set>

/**
 * @brief 结构等价地比较两个类型是否相同
 * @note 递归比较指针、数组、函数的组成类型
 */
static bool type_equal(Type* a, Type* b) {
    if (!a || !b) return false;
    if (a == b) return true;
    if ((a->kind == TypeKind::Int && b->kind == TypeKind::Char) ||
        (a->kind == TypeKind::Char && b->kind == TypeKind::Int))
        return true;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
        case TypeKind::Void:
        case TypeKind::Int:
        case TypeKind::Char:
        case TypeKind::Float:
        case TypeKind::Double:
            return true;
        case TypeKind::Pointer:
            return type_equal(((PointerType*)a)->base, ((PointerType*)b)->base);
        case TypeKind::Array:
            return (((ArrayType*)a)->size == ((ArrayType*)b)->size) &&
                   type_equal(((ArrayType*)a)->base, ((ArrayType*)b)->base);
        case TypeKind::Function: {
            FunctionType* fa = (FunctionType*)a;
            FunctionType* fb = (FunctionType*)b;
            if (!type_equal(fa->returnType, fb->returnType)) return false;
            if (fa->paramTypes.size() != fb->paramTypes.size()) return false;
            for (size_t i = 0; i < fa->paramTypes.size(); i++)
                if (!type_equal(fa->paramTypes[i], fb->paramTypes[i])) return false;
            return true;
        }
        case TypeKind::Struct:
            return ((StructType*)a)->name == ((StructType*)b)->name;
        case TypeKind::Union:
            return ((UnionType*)a)->name == ((UnionType*)b)->name;
        default: return false;
    }
}

static Type* promoteNumeric(Type* a, Type* b) {
    if (a->kind == TypeKind::Double || b->kind == TypeKind::Double)
        return BasicType::Double;
    if (a->kind == TypeKind::Float || b->kind == TypeKind::Float)
        return BasicType::Float;
    return BasicType::Int;
}

static bool assignCompatible(Type* lhs, Type* rhs) {
    if (type_equal(lhs, rhs)) return true;
    if (lhs->kind == TypeKind::Pointer) {
        if (rhs->kind == TypeKind::Pointer)
            return type_equal(static_cast<PointerType*>(lhs)->base,
                              static_cast<PointerType*>(rhs)->base);
        if (rhs->kind == TypeKind::Function)
            return type_equal(static_cast<PointerType*>(lhs)->base, rhs);
        return false;
    }
    if (!lhs->isNumeric() || !rhs->isNumeric()) return false;
    if (lhs->kind == TypeKind::Double) return true;
    if (lhs->kind == TypeKind::Float)
        return rhs->kind == TypeKind::Int || rhs->kind == TypeKind::Char ||
               rhs->kind == TypeKind::Float || rhs->kind == TypeKind::Double;
    return rhs->kind == TypeKind::Int || rhs->kind == TypeKind::Char;
}

static bool isComparisonOp(int op) {
    return op == '<' || op == T_LE_OP || op == '>' || op == T_GE_OP ||
           op == T_EQ_OP || op == T_NE_OP;
}

static bool isLogicalOp(int op) {
    return op == T_AND_OP || op == T_OR_OP;
}

static bool isScalarCondition(Type* t) {
    return t && (t->isNumeric() || t->kind == TypeKind::Pointer);
}

static bool stmtGuaranteedReturn(ASTNode* stmt) {
    if (!stmt) return false;
    switch (stmt->kind) {
        case NodeKind::ReturnStmt:
            return true;
        case NodeKind::CompoundStmt: {
            auto* block = static_cast<MultiNode*>(stmt);
            if (block->children.empty()) return false;
            return stmtGuaranteedReturn(block->children.back().get());
        }
        case NodeKind::IfStmt: {
            auto* ifNode = static_cast<MultiNode*>(stmt);
            if (ifNode->children.size() < 2) return false;
            if (ifNode->children.size() >= 3)
                return stmtGuaranteedReturn(ifNode->children[1].get())
                    && stmtGuaranteedReturn(ifNode->children[2].get());
            return false;
        }
        default:
            return false;
    }
}

/** @brief clone 并解析类型树（不修改 AST 上的 Type*） */
Type* TypeChecker::resolveTypeTree(Type* ftype, int line, bool reportUnknownTag) {
    if (ftype && ftype->kind == TypeKind::Struct && reportUnknownTag) {
        StructType* reg = lookupStructType(static_cast<StructType*>(ftype)->name);
        if (!reg || reg->members.empty())
            error("unknown struct type: " + static_cast<StructType*>(ftype)->name, line);
    }
    if (ftype && ftype->kind == TypeKind::Union && reportUnknownTag) {
        UnionType* reg = lookupUnionType(static_cast<UnionType*>(ftype)->name);
        if (!reg || reg->members.empty())
            error("unknown union type: " + static_cast<UnionType*>(ftype)->name, line);
    }
    return resolveDeclaredType(ftype);
}

void TypeChecker::checkCondition(ASTNode* cond, int line) {
    if (!cond) return;
    Type* t = visit(cond);
    if (!isScalarCondition(t))
        error("control condition requires scalar type", line);
}

void TypeChecker::enterScopeLogged() {
    enterScope();
    scopeDepth_++;
    if (report_) {
        symbolLog_ << "  [L" << scopeDepth_ << "] 进入作用域\n";
    }
}

void TypeChecker::leaveScopeLogged() {
    if (report_) {
        symbolLog_ << "  [L" << scopeDepth_ << "] 离开作用域\n";
    }
    leaveScope();
    scopeDepth_--;
}

void TypeChecker::addSymbolLogged(const std::string& name, Type* type, bool isFunc, int line) {
    if (currentScope && currentScope->lookupLocal(name)) {
        error("redefinition of '" + name + "'", line);
        return;
    }
    addSymbol(name, type, isFunc);
    if (report_) {
        Symbol* sym = getSymbol(name);
        symbolLog_ << "  [L" << scopeDepth_ << "] + "
                   << std::setw(16) << std::left << name << std::right
                   << " : " << (type ? type->toString() : "?")
                   << "  (" << (isFunc ? "函数" : "变量")
                   << ", IR:" << (sym ? sym->irName : name)
                   << ", 行 " << line << ")\n";
    }
}

void TypeChecker::logTypeCheck(const std::string& msg) {
    if (report_) {
        typeLog_ << "  " << msg << "\n";
    }
}

void TypeChecker::flushReport() {
    if (!report_) return;
    *report_ << "========== 符号表构建 ==========\n";
    *report_ << symbolLog_.str();
    *report_ << "\n========== 类型检查 ==========\n";
    *report_ << typeLog_.str();
    if (errorCount_ == 0) {
        *report_ << "  类型检查通过，未发现错误。\n\n";
    } else {
        *report_ << "  共发现 " << errorCount_ << " 个类型错误。\n\n";
    }
}

/** @brief 从 Program 根节点开始，在全局作用域中检查所有顶层声明与语句 */
void TypeChecker::check(ASTNode* root, std::ostream* report) {
    report_ = report;
    symbolLog_.str("");
    symbolLog_.clear();
    typeLog_.str("");
    typeLog_.clear();
    scopeDepth_ = 0;
    errorCount_ = 0;
    loopDepth_ = 0;
    switchDepth_ = 0;
    skipCompoundScope_ = false;
    currentReturnType_ = nullptr;

    resetSymbolTable();

    if (!root || root->kind != NodeKind::Program) {
        flushReport();
        return;
    }

    if (report_) {
        typeLog_ << "  开始语义分析（单遍遍历 AST）\n";
    }

    auto* prog = static_cast<MultiNode*>(root);
    enterScopeLogged();
    for (auto& child : prog->children) {
        if (child) visitStmt(child.get());
    }
    leaveScopeLogged();
    flushReport();
}

/** @brief 表达式访问分发：根据节点种类调用对应 visit 例程 */
Type* TypeChecker::visit(ASTNode* node) {
    if (!node) return BasicType::Void;
    switch (node->kind) {
        case NodeKind::BinaryOp: return visitBinaryOp((BinaryOpNode*)node);
        case NodeKind::UnaryOp:  return visitUnaryOp((UnaryOpNode*)node);
        case NodeKind::AssignOp: return visitAssignOp((AssignOpNode*)node);
        case NodeKind::Identifier: return visitIdentifier((IdentifierNode*)node);
        case NodeKind::Integer:  return visitInteger((IntegerNode*)node);
        case NodeKind::Float:    return visitFloat((FloatNode*)node);
        case NodeKind::String:   return visitString((StringNode*)node);
        case NodeKind::Call:     return visitCall((CallNode*)node);
        case NodeKind::ArraySubscript: return visitSubscript((ArraySubscriptNode*)node);
        case NodeKind::MemberAccess: return visitMemberAccess((MemberAccessNode*)node);
        default: return BasicType::Void;
    }
}

/** @brief 二元算术要求数值操作数并提升类型；比较/逻辑运算结果为 int */
Type* TypeChecker::visitBinaryOp(BinaryOpNode* bin) {
    Type* left = visit(bin->left.get());
    Type* right = visit(bin->right.get());
    if (isLogicalOp(bin->op)) {
        if (!left->isNumeric() || !right->isNumeric()) {
            error("logical operator requires numeric operands", bin->line);
        }
        return BasicType::Int;
    }
    if (isComparisonOp(bin->op)) {
        if (!left->isNumeric() || !right->isNumeric()) {
            error("comparison requires numeric operands", bin->line);
        }
        return BasicType::Int;
    }
    if (!left->isNumeric() || !right->isNumeric()) {
        error("binary operator requires numeric operands", bin->line);
        return BasicType::Int;
    }
    Type* result = promoteNumeric(left, right);
    logTypeCheck("行 " + std::to_string(bin->line) + ": 二元运算 ("
                 + left->toString() + " op " + right->toString()
                 + ") -> " + result->toString());
    return result;
}

/** @brief 一元负号支持数值类型；逻辑非要求 int */
Type* TypeChecker::visitUnaryOp(UnaryOpNode* un) {
    Type* operand = visit(un->operand.get());
    if (un->op == '-') {
        if (!operand->isNumeric()) {
            error("unary minus requires numeric operand", un->line);
            return BasicType::Int;
        }
        if (operand->kind == TypeKind::Double) return BasicType::Double;
        if (operand->kind == TypeKind::Float) return BasicType::Float;
        return BasicType::Int;
    }
    if (un->op == '&') {
        Type* lt = visitLvalue(un->operand.get());
        if (lt->kind == TypeKind::Void) {
            error("cannot take address of this operand", un->line);
            return BasicType::Int;
        }
        return new PointerType(lt);
    }
    if (un->op == '*') {
        if (operand->kind != TypeKind::Pointer) {
            error("dereference of non-pointer", un->line);
            return BasicType::Int;
        }
        return ((PointerType*)operand)->base;
    }
    if (un->op == '!' && !isScalarCondition(operand)) {
        error("logical not requires scalar operand", un->line);
    }
    return BasicType::Int;
}

/** @brief 左值访问：标识符与下标表达式可作为赋值左端 */
Type* TypeChecker::visitLvalue(ASTNode* node) {
    if (!node) return BasicType::Void;
    switch (node->kind) {
        case NodeKind::Identifier:
            return visitIdentifier((IdentifierNode*)node);
        case NodeKind::ArraySubscript:
            return visitSubscript((ArraySubscriptNode*)node);
        case NodeKind::MemberAccess:
            return visitMemberAccess((MemberAccessNode*)node);
        case NodeKind::UnaryOp: {
            auto* un = (UnaryOpNode*)node;
            if (un->op == '*')
                return visitUnaryOp(un);
            error("invalid lvalue in assignment", un->line);
            return BasicType::Int;
        }
        default:
            return visit(node);
    }
}

/** @brief 赋值左值类型须与右值类型结构等价 */
Type* TypeChecker::visitAssignOp(AssignOpNode* assign) {
    if (!assign->right) {
        error("assignment missing right-hand side", assign->line);
        return BasicType::Int;
    }
    Type* left = visitLvalue(assign->left.get());
    Type* right = visit(assign->right.get());
    if (!assignCompatible(left, right)) {
        error("assignment type mismatch: cannot convert "
              + right->toString() + " to " + left->toString(), assign->line);
    } else {
        logTypeCheck("行 " + std::to_string(assign->line) + ": 赋值类型匹配 ("
                     + left->toString() + " = " + right->toString() + ")");
    }
    return left;
}

/** @brief 查找符号表；未定义则报错并返回 int 作为容错类型 */
Type* TypeChecker::visitIdentifier(IdentifierNode* id) {
    Symbol* sym = getSymbol(id->name);
    if (!sym) {
        error("undefined variable: " + id->name, id->line);
        return BasicType::Int;
    }
    return sym->type;
}

/** @brief 整型字面量恒为 int */
Type* TypeChecker::visitInteger(IntegerNode* num) {
    return BasicType::Int;
}

/** @brief 浮点字面量类型为 double */
Type* TypeChecker::visitFloat(FloatNode* num) {
    return BasicType::Double;
}

/** @brief 字符串字面量类型为 char* */
Type* TypeChecker::visitString(StringNode* str) {
    (void)str;
    return BasicType::CharPtr;
}

/** @brief 验证函数/函数指针调用 */
Type* TypeChecker::visitCall(CallNode* call) {
    FunctionType* ft = nullptr;
    std::string callLabel;

    if (!call->callee) {
        error("invalid call expression", call->line);
        return BasicType::Int;
    }

    if (call->callee->kind == NodeKind::Identifier) {
        auto* id = (IdentifierNode*)call->callee.get();
        callLabel = id->name;
        Symbol* sym = getSymbol(id->name);
        if (sym && sym->isFunction)
            ft = (FunctionType*)sym->type;
        else if (sym && sym->type->kind == TypeKind::Pointer &&
                 ((PointerType*)sym->type)->base->kind == TypeKind::Function)
            ft = (FunctionType*)((PointerType*)sym->type)->base;
    } else {
        Type* ct = visit(call->callee.get());
        if (ct->kind == TypeKind::Pointer &&
            ((PointerType*)ct)->base->kind == TypeKind::Function)
            ft = (FunctionType*)((PointerType*)ct)->base;
        else if (ct->kind == TypeKind::Function)
            ft = (FunctionType*)ct;
        callLabel = "<indirect>";
    }

    if (!ft) {
        error("function not defined: " + callLabel, call->line);
        return BasicType::Int;
    }
    if (call->args.size() != ft->paramTypes.size()) {
        error("wrong number of arguments", call->line);
    }
    bool argOk = true;
    for (size_t i = 0; i < call->args.size(); ++i) {
        if (i < ft->paramTypes.size() &&
            !assignCompatible(ft->paramTypes[i], visit(call->args[i].get()))) {
            error("argument type mismatch", call->line);
            argOk = false;
        }
    }
    if (argOk && call->args.size() == ft->paramTypes.size()) {
        logTypeCheck("行 " + std::to_string(call->line) + ": 调用 "
                     + callLabel + "() -> " + ft->returnType->toString());
    }
    return ft->returnType;
}

/** @brief 下标须为 int；被下标对象须为数组或指针，结果为元素/基类型 */
Type* TypeChecker::visitSubscript(ArraySubscriptNode* sub) {
    Type* arr = visit(sub->array.get());
    Type* idx = visit(sub->index.get());
    if (!idx->isInt()) {
        error("array index must be int", sub->line);
    }
    if (arr->kind != TypeKind::Array && arr->kind != TypeKind::Pointer) {
        error("subscript on non-array/pointer", sub->line);
        return BasicType::Int;
    }
    if (arr->kind == TypeKind::Array) {
        return ((ArrayType*)arr)->base;
    } else {
        return ((PointerType*)arr)->base;
    }
}

/** @brief 成员访问：. 用于 struct/union 对象，-> 用于指向 struct/union 的指针 */
Type* TypeChecker::visitMemberAccess(MemberAccessNode* ma) {
    Type* base = visit(ma->object.get());
    if (ma->throughPointer) {
        if (base->kind != TypeKind::Pointer) {
            error("-> requires pointer operand", ma->line);
            return BasicType::Int;
        }
        base = ((PointerType*)base)->base;
    }
    Type* mt = nullptr;
    if (base->kind == TypeKind::Struct) {
        StructType* st = (StructType*)base;
        mt = st->getMemberType(ma->member);
        if (!mt)
            error("struct " + st->name + " has no member: " + ma->member, ma->line);
    } else if (base->kind == TypeKind::Union) {
        UnionType* ut = (UnionType*)base;
        mt = ut->getMemberType(ma->member);
        if (!mt)
            error("union " + ut->name + " has no member: " + ma->member, ma->line);
    } else {
        error(ma->throughPointer ? "-> on non-struct/union pointer"
                                 : "member access on non-struct/union", ma->line);
    }
    return mt ? mt : BasicType::Int;
}

/** @brief 递归解析变量类型（clone 后解析，不修改 AST 上的 Type*） */
Type* TypeChecker::resolveVarType(Type* varType, int line) {
    return resolveTypeTree(varType, line, true);
}

/** @brief 语句访问分发：处理声明、复合语句、控制流与函数定义 */
void TypeChecker::visitStmt(ASTNode* stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case NodeKind::CompoundStmt: {
            auto* block = static_cast<MultiNode*>(stmt);
            astwalk::walkCompound(
                block, skipCompoundScope_,
                [this]() { enterScopeLogged(); },
                [this]() { leaveScopeLogged(); },
                [this](ASTNode* child) { visitStmt(child); });
            break;
        }
        case NodeKind::VarDecl: {
            astwalk::VarDeclLayout layout;
            if (!astwalk::parseVarDeclLayout(static_cast<MultiNode*>(stmt), layout))
                break;
            Type* varType = resolveVarType(layout.typeNode->type, stmt->line);
            addSymbolLogged(layout.id->name, varType, false, layout.id->line);
            if (layout.init) {
                Type* init = visit(layout.init);
                if (!assignCompatible(varType, init)) {
                    error("initializer type mismatch: cannot convert "
                          + init->toString() + " to " + varType->toString(),
                          stmt->line);
                }
            }
            break;
        }
        case NodeKind::StructDef: {
            auto* def = (MultiNode*)stmt;
            if (def->children.empty()) break;
            auto* tag = (IdentifierNode*)def->children[0].get();
            if (lookupStructType(tag->name) || lookupUnionType(tag->name)) {
                error("struct redefinition or tag conflict: " + tag->name, def->line);
                break;
            }
            StructType* st = defineStructType(tag->name);
            for (size_t i = 1; i < def->children.size(); ++i) {
                auto* field = (MultiNode*)def->children[i].get();
                if (field->children.size() < 2) continue;
                auto* fid = (IdentifierNode*)field->children[1].get();
                auto* ftn = (TypeNode*)field->children[0].get();
                if (st->hasMember(fid->name)) {
                    error("duplicate struct member: " + fid->name, field->line);
                    continue;
                }
                st->addMember(fid->name, resolveTypeTree(ftn->type, field->line, false));
            }
            logTypeCheck("行 " + std::to_string(def->line) + ": 定义结构体 "
                         + tag->name + " (" + std::to_string(st->members.size()) + " 成员)");
            break;
        }
        case NodeKind::UnionDef: {
            auto* def = (MultiNode*)stmt;
            if (def->children.empty()) break;
            auto* tag = (IdentifierNode*)def->children[0].get();
            if (lookupUnionType(tag->name) || lookupStructType(tag->name)) {
                error("union redefinition or tag conflict: " + tag->name, def->line);
                break;
            }
            UnionType* ut = defineUnionType(tag->name);
            for (size_t i = 1; i < def->children.size(); ++i) {
                auto* field = (MultiNode*)def->children[i].get();
                if (field->children.size() < 2) continue;
                auto* fid = (IdentifierNode*)field->children[1].get();
                auto* ftn = (TypeNode*)field->children[0].get();
                if (ut->hasMember(fid->name)) {
                    error("duplicate union member: " + fid->name, field->line);
                    continue;
                }
                ut->addMember(fid->name, resolveTypeTree(ftn->type, field->line, false));
            }
            logTypeCheck("行 " + std::to_string(def->line) + ": 定义联合体 "
                         + tag->name + " (" + std::to_string(ut->members.size()) + " 成员)");
            break;
        }
        case NodeKind::ExprStmt:
            if (auto* exprStmt = (MultiNode*)stmt) {
                if (!exprStmt->children.empty())
                    visit(exprStmt->children[0].get());
            }
            break;
        case NodeKind::IfStmt: {
            auto* ifNode = (MultiNode*)stmt;
            if (ifNode->children.size() >= 1)
                checkCondition(ifNode->children[0].get(), ifNode->line);
            if (ifNode->children.size() >= 2)
                visitStmt(ifNode->children[1].get());
            if (ifNode->children.size() >= 3)
                visitStmt(ifNode->children[2].get());
            break;
        }
        case NodeKind::WhileStmt: {
            auto* whileNode = static_cast<MultiNode*>(stmt);
            if (whileNode->children.size() >= 1)
                checkCondition(whileNode->children[0].get(), whileNode->line);
            ++loopDepth_;
            if (whileNode->children.size() >= 2)
                visitStmt(whileNode->children[1].get());
            --loopDepth_;
            break;
        }
        case NodeKind::ForStmt: {
            auto* forNode = static_cast<MultiNode*>(stmt);
            if (forNode->children.size() >= 1 && forNode->children[0])
                visitStmt(forNode->children[0].get());
            ++loopDepth_;
            if (forNode->children.size() >= 2 && forNode->children[1])
                checkCondition(forNode->children[1].get(), forNode->line);
            if (forNode->children.size() >= 3 && forNode->children[2])
                visitStmt(forNode->children[2].get());
            if (forNode->children.size() >= 4 && forNode->children[3])
                visitStmt(forNode->children[3].get());
            --loopDepth_;
            break;
        }
        case NodeKind::SwitchStmt: {
            auto* sw = static_cast<MultiNode*>(stmt);
            Type* swType = visit(sw->children[0].get());
            if (!swType->isInt()) {
                error("switch expression must be int", sw->line);
            }
            std::set<int> caseLabels;
            ++switchDepth_;
            for (size_t i = 1; i < sw->children.size(); ++i) {
                auto* clause = static_cast<MultiNode*>(sw->children[i].get());
                if (clause->kind == NodeKind::CaseStmt) {
                    int val = static_cast<IntegerNode*>(clause->children[0].get())->value;
                    if (!caseLabels.insert(val).second) {
                        error("duplicate case value: " + std::to_string(val), clause->line);
                    }
                }
            }
            astwalk::walkSwitchClauseStmts(sw, [this](ASTNode* clauseStmt) {
                visitStmt(clauseStmt);
            });
            --switchDepth_;
            break;
        }
        case NodeKind::BreakStmt:
            if (loopDepth_ == 0 && switchDepth_ == 0)
                error("'break' outside loop or switch", stmt->line);
            break;
        case NodeKind::ContinueStmt:
            if (loopDepth_ == 0)
                error("'continue' outside loop", stmt->line);
            break;
        case NodeKind::ReturnStmt: {
            auto* retNode = static_cast<MultiNode*>(stmt);
            if (!currentReturnType_) {
                error("'return' outside function", retNode->line);
                break;
            }
            if (currentReturnType_->kind == TypeKind::Void) {
                if (!retNode->children.empty())
                    error("void function must not return a value", retNode->line);
            } else if (retNode->children.empty()) {
                error("non-void function must return a value", retNode->line);
            } else {
                Type* valType = visit(retNode->children[0].get());
                if (!assignCompatible(currentReturnType_, valType)) {
                    error("return type mismatch: expected "
                          + currentReturnType_->toString() + ", got "
                          + valType->toString(), retNode->line);
                }
            }
            break;
        }
        case NodeKind::FuncDef: {
            astwalk::FuncDefLayout layout;
            if (!astwalk::parseFuncDefLayout(static_cast<MultiNode*>(stmt), layout))
                break;
            Type* fnRetType = resolveVarType(layout.retType->type, layout.id->line);
            auto* ft = new FunctionType(fnRetType);
            astwalk::walkParams(layout.paramList, [&](TypeNode* tn, IdentifierNode*) {
                ft->addParam(resolveVarType(tn->type, layout.id->line));
            });
            addSymbolLogged(layout.id->name, ft, true, layout.id->line);
            logTypeCheck("行 " + std::to_string(layout.id->line) + ": 检查函数 "
                         + layout.id->name + " 函数体");
            Type* savedReturnType = currentReturnType_;
            currentReturnType_ = fnRetType;
            enterScopeLogged();
            astwalk::walkParams(layout.paramList, [&](TypeNode* tn, IdentifierNode* pid) {
                Type* ptype = resolveVarType(tn->type, pid->line);
                addSymbolLogged(pid->name, ptype, false, pid->line);
            });
            if (layout.body) {
                skipCompoundScope_ = true;
                visitStmt(layout.body);
                if (!fnRetType->isVoid()
                    && !stmtGuaranteedReturn(layout.body)) {
                    error("non-void function '" + layout.id->name
                          + "' may fail to return a value", layout.id->line);
                }
            }
            leaveScopeLogged();
            currentReturnType_ = savedReturnType;
            break;
        }
        case NodeKind::StmtList:
        case NodeKind::ArgList:
        case NodeKind::ParamList:
            // 语法归约临时容器，语义阶段不单独处理
            break;
        default:
            break;
    }
}

/** @brief 向 stderr 与详情报告输出带行号的类型错误信息 */
void TypeChecker::error(const std::string& msg, int line) {
    ++errorCount_;
    std::cerr << "Type error at line " << line << ": " << msg << std::endl;
    if (report_) {
        typeLog_ << "  [错误] 行 " << line << ": " << msg << "\n";
    }
}
