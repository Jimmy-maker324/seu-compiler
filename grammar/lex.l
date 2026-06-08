%{
/*
 * lex.l — Seu 编译器内置 C 词法扫描器（Flex 规格文件）
 *
 * 本文件由 SeuLex 读取并生成 lex.yy.cpp，识别 MiniC 子集关键字、
 * 常量、字符串、运算符与标点，供语法分析器调用。
 * 词法值通过 yylval 传递，行号由 yylineno 维护。
 *
 * 注意：规则区仅可使用整行块注释，勿在 pattern 或 action 行尾加注释，
 *       否则 SeuLex 解析器会报错。
 */
#include "common_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void comment(void);
static void count(void);
static void bump_yylineno(char c);
static char* unescape_string(const char* s);

extern YYSTYPE yylval;
extern char* yytext;
extern int yylineno;
%}

D       [0-9]
L       [a-zA-Z_]
H       [a-fA-F0-9]
E       ([Ee][+-]?{D}+)
P       ([Pp][+-]?{D}+)
FS      (f|F|l|L)
IS      ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))

%%

/* ---------- 注释（不返回记号）---------- */
"/*"            { comment(); }
"//"[^\n]*      { }

/* ---------- C 关键字（MiniC 文法子集；其余作为 IDENTIFIER） ---------- */
"break"         { count(); return T_BREAK; }
"case"          { count(); return T_CASE; }
"char"          { count(); return T_CHAR; }
"continue"      { count(); return T_CONTINUE; }
"default"       { count(); return T_DEFAULT; }
"double"        { count(); return T_DOUBLE; }
"else"          { count(); return T_ELSE; }
"float"         { count(); return T_FLOAT; }
"for"           { count(); return T_FOR; }
"if"            { count(); return T_IF; }
"int"           { count(); return T_INT; }
"return"        { count(); return T_RETURN; }
"struct"        { count(); return T_STRUCT; }
"switch"        { count(); return T_SWITCH; }
"union"         { count(); return T_UNION; }
"void"          { count(); return T_VOID; }
"while"         { count(); return T_WHILE; }

/* ---------- 标识符 ---------- */
{L}({L}|{D})*   { count(); yylval.sval = strdup(yytext); return T_IDENTIFIER; }

/* ---------- 整型常量（十六进制 / 八进制 / 十进制）---------- */
0[xX]{H}+{IS}?  { count(); yylval.ival = (int)strtol(yytext, NULL, 0); return T_CONSTANT; }
0[0-7]*{IS}?    { count(); yylval.ival = (int)strtol(yytext, NULL, 0); return T_CONSTANT; }
[1-9]{D}*{IS}?  { count(); yylval.ival = (int)strtol(yytext, NULL, 0); return T_CONSTANT; }

/* ---------- 字符常量 ---------- */
L?'(\\.|[^\\'\n])+' {
    count();
    if (yytext[0] == 'L') {
        if (yytext[2] == '\\') {
            char esc = yytext[3];
            switch (esc) {
                case 'n': yylval.ival = '\n'; break;
                case 't': yylval.ival = '\t'; break;
                case 'r': yylval.ival = '\r'; break;
                default: yylval.ival = esc; break;
            }
        } else {
            yylval.ival = (unsigned char)yytext[2];
        }
    } else {
        if (yytext[1] == '\\') {
            char esc = yytext[2];
            switch (esc) {
                case 'n': yylval.ival = '\n'; break;
                case 't': yylval.ival = '\t'; break;
                case 'r': yylval.ival = '\r'; break;
                default: yylval.ival = esc; break;
            }
        } else {
            yylval.ival = (unsigned char)yytext[1];
        }
    }
    return T_CONSTANT;
}

/* ---------- 浮点常量 ---------- */
{D}+{E}{FS}?            { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }
{D}*"."{D}+{E}?{FS}?    { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }
{D}+"."{D}*{E}?{FS}?    { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }
0[xX]{H}+{P}{FS}?       { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }
0[xX]{H}*"."{H}+{P}?{FS}?   { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }
0[xX]{H}+"."{H}*{P}?{FS}?   { count(); yylval.fval = strtod(yytext, NULL); return T_FLOAT_CONSTANT; }

/* ---------- 字符串字面量 ---------- */
L?\"(\\.|[^\\"\n])*\"   { count(); yylval.sval = unescape_string(yytext); return T_STRING_LITERAL; }

/* ---------- 逻辑/比较/指针运算符 ---------- */
"->"            { count(); return T_PTR_OP; }
"&&"            { count(); return T_AND_OP; }
"||"            { count(); return T_OR_OP; }
"<="            { count(); return T_LE_OP; }
">="            { count(); return T_GE_OP; }
"=="            { count(); return T_EQ_OP; }
"!="            { count(); return T_NE_OP; }

/* ---------- 单字符标点与运算符 ---------- */
";"             { count(); return T_SEMICOLON; }
","             { count(); return T_COMMA; }
":"             { count(); return T_COLON; }
"="             { count(); return T_ASSIGN; }
"("             { count(); return T_LPAREN; }
")"             { count(); return T_RPAREN; }
"["             { count(); return T_LBRACKET; }
"]"             { count(); return T_RBRACKET; }
"{"             { count(); return T_LBRACE; }
"}"             { count(); return T_RBRACE; }
"."             { count(); return T_DOT; }
"&"             { count(); return T_AMP; }
"!"             { count(); return T_BANG; }
"-"             { count(); return T_MINUS; }
"+"             { count(); return T_PLUS; }
"*"             { count(); return T_STAR; }
"/"             { count(); return T_SLASH; }
"<"             { count(); return T_LESS; }
">"             { count(); return T_GREATER; }

/* ---------- 空白字符（不计入记号）---------- */
[ \t\v\n\f\r]   { count(); }

/* ---------- 无法识别的字符 ---------- */
.               { fprintf(stderr, "Unrecognized character '%c' at line %d\n", *yytext, yylineno); }

%%

/* 从自定义输入缓冲区读取下一字符（供 comment 使用） */
static char next_char(void) {
    if (input_ptr >= input_end) return 0;
    char c = *input_ptr;
    input_ptr++;
    bump_yylineno(c);
    return c;
}

/* 扫描 C 块注释，从块注释起始符之后直到结束符 */
static void comment(void) {
    char c, prev = 0;
    while ((c = next_char()) != 0) {
        if (c == '/' && prev == '*')
            return;
        prev = c;
    }
    fprintf(stderr, "Error: unterminated comment at line %d\n", yylineno);
}

static int column = 0;

/* 字符已被消费后更新行号：CRLF 只计一次，单独 CR 也视为换行 */
static void bump_yylineno(char c) {
    if (c == '\n') {
        yylineno++;
    } else if (c == '\r') {
        if (input_ptr >= input_end || *input_ptr != '\n')
            yylineno++;
    }
}

/* 根据 yytext 更新列号（Tab 按 8 列对齐） */
static void count(void) {
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n')
            column = 0;
        else if (yytext[i] == '\t')
            column += 8 - (column % 8);
        else
            column++;
    }
}

/* 去掉引号并展开 \\n 等转义，返回堆分配字符串 */
static char* unescape_string(const char* s) {
    size_t len = strlen(s);
    if (len < 2) return strdup(s);
    int has_prefix = (s[0] == 'L' && s[1] == '"');
    const char* start = has_prefix ? s + 2 : s + 1;
    const char* end = s + len - 1;
    if (*end != '"') return strdup(s);
    size_t out_len = end - start;
    char* result = (char*)malloc(out_len + 1);
    char* p = result;
    for (const char* q = start; q < end; q++) {
        if (*q == '\\' && q + 1 < end) {
            q++;
            switch (*q) {
                case 'n': *p++ = '\n'; break;
                case 't': *p++ = '\t'; break;
                case 'r': *p++ = '\r'; break;
                case 'v': *p++ = '\v'; break;
                case 'f': *p++ = '\f'; break;
                case 'a': *p++ = '\a'; break;
                case 'b': *p++ = '\b'; break;
                case '\\': *p++ = '\\'; break;
                case '\'': *p++ = '\''; break;
                case '\"': *p++ = '\"'; break;
                default: *p++ = *q; break;
            }
        } else {
            *p++ = *q;
        }
    }
    *p = '\0';
    return result;
}
