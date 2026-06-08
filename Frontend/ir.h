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
bool irIsConstant(const std::string& s);
bool irIsTemp(const std::string& s);

#endif
