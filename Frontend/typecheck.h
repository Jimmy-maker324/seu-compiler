/**
 * @file typecheck.h
 * @brief 语义分析（类型检查）器
 *
 * 【设计】访问者模式：visit 处理表达式，visitStmt 处理声明/语句
 * 【符号表】配合 symbol.h 的 Scope 链，FuncDef/CompoundStmt 进入新作用域
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef TYPECHECK_H
#define TYPECHECK_H

#include "ast.h"
#include <iosfwd>
#include <sstream>
#include <string>

/**
 * @class TypeChecker
 * @brief AST 访问者，执行静态类型检查并报告语义错误
 */
class TypeChecker {
public:
    /** @brief 对 Program 根节点启动类型检查；report 非空时写入符号表与类型检查详情 */
    void check(ASTNode* root, std::ostream* report = nullptr);
    /** @brief 类型错误个数（check 之后有效） */
    int errorCount() const { return errorCount_; }
    bool hasErrors() const { return errorCount_ > 0; }
private:
    std::ostream* report_ = nullptr;
    std::ostringstream symbolLog_;
    std::ostringstream typeLog_;
    int scopeDepth_ = 0;
    int errorCount_ = 0;
    int loopDepth_ = 0;
    int switchDepth_ = 0;
    /** @brief 函数体最外层 CompoundStmt 与形参共享作用域，不再嵌套开 scope */
    bool skipCompoundScope_ = false;

    void enterScopeLogged();
    void leaveScopeLogged();
    void addSymbolLogged(const std::string& name, Type* type, bool isFunc, int line);
    void logTypeCheck(const std::string& msg);
    /** @brief 访问表达式节点，返回其类型 */
    Type* visit(ASTNode* node);
    /** @brief 检查二元运算：数值类型提升，结果为 int 或浮点 */
    Type* visitBinaryOp(BinaryOpNode* bin);
    /** @brief 检查一元运算（如逻辑非） */
    Type* visitUnaryOp(UnaryOpNode* un);
    /** @brief 检查赋值：左值与右值类型须一致 */
    Type* visitAssignOp(AssignOpNode* assign);
    /** @brief 区分左值（标识符、下标）与普通表达式 */
    Type* visitLvalue(ASTNode* node);
    /** @brief 查找标识符符号并返回其类型 */
    Type* visitIdentifier(IdentifierNode* id);
    /** @brief 整型字面量类型为 int */
    Type* visitInteger(IntegerNode* num);
    /** @brief 浮点字面量类型为 double */
    Type* visitFloat(FloatNode* num);
    /** @brief 字符串字面量类型简化为 char */
    Type* visitString(StringNode* str);
    /** @brief 检查函数调用的存在性、实参个数与类型 */
    Type* visitCall(CallNode* call);
    /** @brief 检查数组/指针下标访问 */
    Type* visitSubscript(ArraySubscriptNode* sub);
    /** @brief 检查结构体成员访问 */
    Type* visitMemberAccess(MemberAccessNode* ma);
    Type* resolveVarType(Type* varType, int line);
    /** @brief clone 并解析类型树；reportUnknownTag 为 false 时不报未定义 tag */
    Type* resolveTypeTree(Type* src, int line, bool reportUnknownTag);
    /** @brief 访问语句节点（声明、控制流、函数定义等） */
    void visitStmt(ASTNode* stmt);
    /** @brief 输出类型错误到 stderr 与详情报告 */
    void error(const std::string& msg, int line);
    void flushReport();
};

#endif
