/**
 * @file ast_format.h
 * @brief AST 节点与运算符的共享格式化工具（文本树 / DOT 共用）
 */

#ifndef AST_FORMAT_H
#define AST_FORMAT_H

#include "ast.h"
#include <string>

/** @brief NodeKind → 可读英文节点名 */
const char* astKindName(NodeKind k);

/** @brief 运算符 token 码 → 可读字符串（含复合运算符） */
std::string astOpName(int op);

/** @brief Call 的 callee 简短描述（标识符、*ptr、下标、成员访问等） */
std::string astCalleeSummary(ASTNode* node);

#endif
