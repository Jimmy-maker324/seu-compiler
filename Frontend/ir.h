/**
 * @file ir.h
 * @brief 四元式中间表示（IR 生成与优化共用）
 */

#ifndef IR_H
#define IR_H

#include <string>
#include <vector>

/** @brief 四元式 (op, arg1, arg2, result) */
struct IRQuad {
    std::string op, arg1, arg2, result;
};

bool irIsIntegerLiteral(const std::string& s);
bool irIsFloatLiteral(const std::string& s);
bool irIsStringSymbol(const std::string& s);
bool irIsConstant(const std::string& s);
bool irIsTemp(const std::string& s);
bool irIsLabel(const std::string& s);
bool irIsRelationalOp(const std::string& op);
/** @brief 关系比较四元式且 result 为标签：关系成立时跳转到 result */
bool irIsCondJump(const IRQuad& q);

#endif
