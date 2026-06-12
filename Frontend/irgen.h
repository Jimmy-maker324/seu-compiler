/**
 * @file irgen.h
 * @brief 中间代码（四元式）生成器接口
 *
 * 【数据结构】vector<Quad> code — 顺序四元式列表
 * 【翻译模式】语法制导、后序表达式 + 回填式控制流（if/goto/label）
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef IRGEN_H
#define IRGEN_H

#include "ast.h"
#include "ast_walk.h"
#include "ir.h"
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class IRGenerator
 * @brief 将 AST 翻译为四元式中间表示并输出到文件
 */
class IRGenerator {
public:
    /** @brief 指定 IR 输出文件路径 */
    IRGenerator(const std::string& outFile);
    /** @brief 从 Program 根节点生成 IR（独立第二遍，测试/兼容用） */
    void generate(ASTNode* root);
    /** @brief 单遍模式：符号已由 TypeChecker 注册，只补 IR */
    void setCombinedPass(bool enabled) { combinedPass_ = enabled; }
    /** @brief 单遍开始前清空 IR 状态 */
    void beginCombinedPass();
    void endCombinedPass() { combinedPass_ = false; }
    /** @brief 单遍模式下函数体检查完成后生成 func/param 四元式 */
    void emitFunctionHead(const astwalk::FuncDefLayout& layout);
    void emitFunctionTail(const std::string& savedFunc);
    /** @brief 当前 IR 生成上下文中的函数名（单遍模式保存/恢复用） */
    std::string currentFuncName() const { return currentFunc; }
    /** @brief 访问语句节点（TypeChecker 单遍驱动时调用） */
    void visitStmt(ASTNode* stmt);
    /** @brief 将四元式序列写入文件 */
    void dump();
    /** @brief 写入指定路径（用于保存优化前 IR） */
    void dumpTo(const std::string& path);
    /** @brief 获取四元式序列（供优化 pass 使用） */
    std::vector<IRQuad>& getCode() { return code; }
    const std::vector<IRQuad>& getCode() const { return code; }
private:
    std::string outFile;  ///< 输出文件路径
    /** @brief 追加一条四元式 (op, arg1, arg2, result) */
    void emit(const std::string& op, const std::string& a1,
              const std::string& a2, const std::string& r);
    /** @brief 访问表达式，返回存放结果的变量名或常量字符串 */
    std::string visit(ASTNode* node);
    /** @brief 生成二元运算四元式 */
    std::string visitBinaryOp(BinaryOpNode* bin);
    /** @brief 生成一元运算四元式 */
    std::string visitUnaryOp(UnaryOpNode* un);
    /** @brief 生成赋值四元式（含数组元素赋值 []=） */
    std::string visitAssignOp(AssignOpNode* assign);
    /** @brief 标识符解析为符号表中的 IR 名（Symbol::irName） */
    std::string visitIdentifier(IdentifierNode* id);
    /** @brief 整型字面量转为十进制字符串 */
    std::string visitInteger(IntegerNode* num);
    /** @brief 浮点字面量转为字符串 */
    std::string visitFloat(FloatNode* num);
    /** @brief 字符串字面量：生成 str 四元式并返回符号名 strN */
    std::string visitString(StringNode* str);
    /** @brief 生成函数调用四元式 */
    std::string visitCall(CallNode* call);
    int tempCounter;   ///< 临时变量编号计数器
    int labelCounter;  ///< 标签编号计数器（与 temp 分离）
    std::string currentFunc;  ///< 当前正在生成 IR 的函数名（用于标签说明）
    std::unordered_map<std::string, std::string> labelDesc;  ///< 标签 → 语义说明

    std::vector<IRQuad> code;  ///< 四元式序列

    std::vector<std::string> breakTargetStack;     ///< break 目标（循环/ switch 出口）
    std::vector<std::string> continueTargetStack;  ///< continue 目标（仅循环）

    bool combinedPass_ = false;      ///< true：符号表由 TypeChecker 维护
    bool skipCompoundScope_ = false; ///< 函数体最外层块不再嵌套开 scope
    int stringCounter_ = 0;          ///< 字符串常量 strN 编号

    /** @brief 分配新的临时变量名 t0, t1, ... */
    std::string newTemp();
    /** @brief 分配新的标签名 L0, L1, ... */
    std::string newLabel();
    /** @brief 为标签生成带函数上下文的说明前缀 */
    std::string labelContext(const std::string& meaning) const;
    /** @brief 登记标签语义并输出 label 四元式 */
    void emitLabel(const std::string& label, const std::string& desc);
    /** @brief 条件为真跳 trueLabel；为假且 falseLabel 非空则 goto falseLabel，否则顺序执行下一行 */
    void emitTrueCondJump(ASTNode* cond, const std::string& trueLabel,
                          const std::string& falseLabel = "");
    /** @brief 二元比较运算符 → IR 操作符，非比较则返回空串 */
    static std::string relOpFromBinary(int tokenOp);
};

#endif
