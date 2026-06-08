/**
 * @file token_names.cpp
 * @brief 终结符显示名（数据来自 include/tokens.def）
 */

#include "token_names.h"
#include "common_defs.h"
#include <cstdio>

const char* tokenDisplayName(int token) {
    static char buf[16];
    switch (token) {
        case T_EOF: return "EOF";
#define TOKEN(name, id, disp) case name: return disp;
#include "tokens.def"
#undef TOKEN
        default:
            if (token >= 32 && token <= 126) {
                snprintf(buf, sizeof(buf), "'%c'", (char)token);
                return buf;
            }
            snprintf(buf, sizeof(buf), "TOKEN(%d)", token);
            return buf;
    }
}
