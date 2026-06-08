/* SeuYacc 终结符注册（由 grammar.cpp init_builtin_tokens 包含） */
token_map["T_EOF"] = T_EOF;
symbol_name_map[T_EOF] = "T_EOF";

#define TOKEN(name, id, disp) \
    token_map[#name] = id; \
    symbol_name_map[id] = #name;

#include "tokens.def"

#undef TOKEN

/* 历史别名（与 grammar/yacc.y 兼容） */
token_map["T_IDENT"] = T_IDENTIFIER;
token_map["T_INTEGER"] = T_CONSTANT;
token_map["T_STRING"] = T_STRING_LITERAL;

token_map["T_EQ"] = T_EQ_OP;
token_map["T_NE"] = T_NE_OP;
token_map["T_LE"] = T_LE_OP;
token_map["T_GE"] = T_GE_OP;
token_map["T_AND"] = T_AND_OP;
token_map["T_OR"] = T_OR_OP;

token_map["T_PLUS"] = '+';
token_map["T_MINUS"] = '-';
token_map["T_MULT"] = '*';
token_map["T_DIV"] = '/';
token_map["T_ASSIGN"] = '=';
token_map["T_LT"] = '<';
token_map["T_GT"] = '>';
token_map["T_NOT"] = '!';
token_map["T_LPAREN"] = '(';
token_map["T_RPAREN"] = ')';
token_map["T_LBRACE"] = '{';
token_map["T_RBRACE"] = '}';
token_map["T_LBRACKET"] = '[';
token_map["T_RBRACKET"] = ']';
token_map["T_SEMI"] = ';';
token_map["T_COMMA"] = ',';
token_map["T_COLON"] = ':';
token_map["T_DOT"] = '.';
token_map["T_AMP"] = '&';

token_map["+"] = '+';
token_map["-"] = '-';
token_map["*"] = '*';
token_map["/"] = '/';
token_map["="] = '=';
token_map["<"] = '<';
token_map[">"] = '>';
token_map["!"] = '!';
token_map["("] = '(';
token_map[")"] = ')';
token_map["{"] = '{';
token_map["}"] = '}';
token_map["["] = '[';
token_map["]"] = ']';
token_map[";"] = ';';
token_map[","] = ',';
token_map[":"] = ':';
token_map["."] = '.';
token_map["&"] = '&';

token_map["error"] = T_ERROR;

for (auto& p : token_map) {
    if (symbol_name_map.find(p.second) == symbol_name_map.end())
        symbol_name_map[p.second] = p.first;
}

symbol_name_map['+'] = "'+'";
symbol_name_map['-'] = "'-'";
symbol_name_map['*'] = "'*'";
symbol_name_map['/'] = "'/'";
symbol_name_map['='] = "'='";
symbol_name_map['<'] = "'<'";
symbol_name_map['>'] = "'>'";
symbol_name_map['!'] = "'!'";
symbol_name_map['('] = "'('";
symbol_name_map[')'] = "')'";
symbol_name_map['{'] = "'{'";
symbol_name_map['}'] = "'}'";
symbol_name_map['['] = "'['";
symbol_name_map[']'] = "']'";
symbol_name_map[';'] = "';'";
symbol_name_map[','] = "','";
symbol_name_map[':'] = "':'";
symbol_name_map['.'] = "'.'";
symbol_name_map['&'] = "'&'";
