/**
 * @file token_names.h
 * @brief 终结符编号 → 可读名称（词法 dump、调试输出共用）
 */

#ifndef TOKEN_NAMES_H
#define TOKEN_NAMES_H

/** @brief 将 token 码转为可读名称；单字符返回 'x' 形式，未知返回 "UNKNOWN" */
const char* tokenDisplayName(int token);

#endif
