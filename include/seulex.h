/*
 * seulex.h — SeuLex 词法分析器对外接口
 *
 * 语法分析器（yacc.y / yyparse.cpp）与前端通过本头文件调用 yylex、
 * set_input，并访问 yytext、yylval、yylineno。
 */
#ifndef SEULEX_H
#define SEULEX_H

#include "common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 绑定源字符串并重置行号（分析前调用） */
void set_input(const char* str);

/** @brief 读取下一个记号，0 表示 EOF */
int yylex(void);

#ifdef __cplusplus
}
#endif

extern int yylineno;
extern char* yytext;
extern YYSTYPE yylval;

#endif
