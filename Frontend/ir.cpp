/**
 * @file ir.cpp
 * @brief IR 工具函数
 */

#include "ir.h"
#include <cctype>

bool irIsIntegerLiteral(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' && s.size() > 1) i = 1;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i)
        if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

bool irIsFloatLiteral(const std::string& s) {
    if (s.empty()) return false;
    bool dot = false;
    size_t i = (s[0] == '-' ? 1 : 0);
    for (; i < s.size(); ++i) {
        if (s[i] == '.') {
            if (dot) return false;
            dot = true;
        } else if (!std::isdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return dot;
}

bool irIsConstant(const std::string& s) {
    return irIsIntegerLiteral(s) || irIsFloatLiteral(s);
}

bool irIsTemp(const std::string& s) {
    return s.size() >= 2 && s[0] == 't' && irIsIntegerLiteral(s.substr(1));
}

bool irIsStringSymbol(const std::string& s) {
    if (s.size() < 4 || s.compare(0, 3, "str") != 0) return false;
    return irIsIntegerLiteral(s.substr(3));
}
