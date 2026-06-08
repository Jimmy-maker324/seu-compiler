/*
 * common_defs.h — Seu 编译器前后端共享定义
 *
 * 【TokenType】258+ 为多字符记号；单字符运算符用 ASCII 值（如 '+'）
 * 【YYSTYPE】ival | sval | fval | ptr，ptr 在语法分析中存 ASTNode*
 * 【NON_TERM_BASE=1000】非终结符编号分区，与终结符 ID 不重叠
 */
#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

typedef enum {
    T_EOF = 0,

    /* MiniC 关键字（与 grammar/yacc.y 一致） */
    T_BREAK = 258,
    T_CASE = 259,
    T_CHAR = 260,
    T_CONTINUE = 261,
    T_DEFAULT = 262,
    T_DOUBLE = 263,
    T_ELSE = 264,
    T_FLOAT = 265,
    T_FOR = 266,
    T_IF = 267,
    T_INT = 268,
    T_RETURN = 269,
    T_STRUCT = 270,
    T_SWITCH = 271,
    T_UNION = 272,
    T_VOID = 273,
    T_WHILE = 274,

    /* 标识符与字面量 */
    T_IDENTIFIER = 275,
    T_CONSTANT = 276,
    T_STRING_LITERAL = 277,
    T_FLOAT_CONSTANT = 278,

    /* 文法使用的多字符运算符 */
    T_PTR_OP = 279,   /* -> */
    T_AND_OP = 280,   /* && */
    T_OR_OP = 281,    /* || */
    T_LE_OP = 282,    /* <= */
    T_GE_OP = 283,    /* >= */
    T_EQ_OP = 284,    /* == */
    T_NE_OP = 285,    /* != */

    T_ERROR = 286,    /* Yacc 错误恢复（词法器不返回） */

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
