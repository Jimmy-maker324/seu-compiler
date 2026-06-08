/*
 * common_defs.h — Seu 编译器前后端共享定义
 *
 * 【TokenType】258+ 为多字符记号；单字符运算符用 ASCII 值（如 '+'）
 * 多字符终结符枚举见 include/tokens.def（单点维护）
 */
#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

typedef enum {
    T_EOF = 0,

#define TOKEN(name, id, disp) name = id,
#include "tokens.def"
#undef TOKEN

    /* 单字符：枚举值即 ASCII，词法器可直接 return '+' 等 */
    T_SEMICOLON = ';',
    T_COMMA = ',',
    T_COLON = ':',
    T_LPAREN = '(',
    T_RPAREN = ')',
    T_LBRACKET = '[',
    T_RBRACKET = ']',
    T_LBRACE = '{',
    T_RBRACE = '}',
    T_DOT = '.',
    T_AMP = '&',
    T_BANG = '!',
    T_MINUS = '-',
    T_PLUS = '+',
    T_STAR = '*',
    T_SLASH = '/',
    T_LESS = '<',
    T_GREATER = '>',
    T_ASSIGN = '='

} TokenType;

typedef union {
    int ival;
    double fval;
    char* sval;
    void* ptr;
} YYSTYPE;

#define NON_TERM_BASE 1000
#define STACK_SIZE 2048

#define T_MULT  '*'
#define T_DIV   '/'
#define T_OR    T_OR_OP
#define T_AND   T_AND_OP
#define T_EQ    T_EQ_OP
#define T_NE    T_NE_OP
#define T_LT    '<'
#define T_LE    T_LE_OP
#define T_GT    '>'
#define T_GE    T_GE_OP
#define T_NOT   '!'

#endif
