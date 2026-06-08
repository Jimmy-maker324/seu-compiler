/**
 * @file token_names.cpp
 * @brief 终结符显示名（与 common_defs.h TokenType 枚举一致）
 */

#include "token_names.h"
#include "common_defs.h"
#include <cstdio>

const char* tokenDisplayName(int token) {
    static char buf[16];
    switch (token) {
        case T_EOF: return "EOF";
        case T_BREAK: return "break";
        case T_CASE: return "case";
        case T_CHAR: return "char";
        case T_CONTINUE: return "continue";
        case T_DEFAULT: return "default";
        case T_DOUBLE: return "double";
        case T_ELSE: return "else";
        case T_FLOAT: return "float";
        case T_FOR: return "for";
        case T_IF: return "if";
        case T_INT: return "int";
        case T_RETURN: return "return";
        case T_STRUCT: return "struct";
        case T_SWITCH: return "switch";
        case T_UNION: return "union";
        case T_VOID: return "void";
        case T_WHILE: return "while";
        case T_IDENTIFIER: return "IDENTIFIER";
        case T_CONSTANT: return "CONSTANT";
        case T_FLOAT_CONSTANT: return "FLOAT";
        case T_STRING_LITERAL: return "STRING";
        case T_PTR_OP: return "->";
        case T_AND_OP: return "&&";
        case T_OR_OP: return "||";
        case T_LE_OP: return "<=";
        case T_GE_OP: return ">=";
        case T_EQ_OP: return "==";
        case T_NE_OP: return "!=";
        case T_ERROR: return "error";
        default:
            if (token >= 32 && token <= 126) {
                snprintf(buf, sizeof(buf), "'%c'", (char)token);
                return buf;
            }
            snprintf(buf, sizeof(buf), "TOKEN(%d)", token);
            return buf;
    }
}
