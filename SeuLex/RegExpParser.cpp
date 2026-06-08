/*
 * ============================================================================
 * RegExpParser.cpp — Flex 风格正则表达式的递归下降解析
 * ============================================================================
 *
 * 【输出数据结构】RegexNode 抽象语法树（见 lex_common.h）
 *   RegexType: CHAR | CONCAT | ALT | STAR | PLUS | OPT | RANGE
 *   树形结构：CONCAT/ALT 用 left/right；RANGE 用 rangeChars 向量
 *
 * 【文法层次（优先级从低到高）】
 *   expr  → term ('|' term)*        并（|）最低
 *   term  → factor+                 隐式连接（相邻因子）
 *   factor→ atom [*+?]              量词最高
 *   atom  → '(' expr ')' | '['..']' | '.' | "lit" | char
 *
 * 【parse 主流程】
 *   1. i=0，调用 parseExpr 得到根节点
 *   2. 若 i != len(regex) 则报错（未消费完）
 *
 * 【parseTerm 隐式连接】
 *   Flex 中 "ab" 表示 CONCAT(a,b)，通过循环读取相邻 factor 实现
 *
 * 【错误处理】解析失败返回 nullptr，调用方负责不泄漏已分配节点
 * ============================================================================
 */
#include "RegExpParser.h"
#include "lex_common.h"
#include <cctype>
#include <iostream>
#include <algorithm>
using namespace std;

/* 前向声明：各层级解析函数 */
static RegexNode* parseExpr(const string& s, size_t& i);
static RegexNode* parseTerm(const string& s, size_t& i);
static RegexNode* parseFactor(const string& s, size_t& i);
static RegexNode* parseAtom(const string& s, size_t& i);
static RegexNode* parseRange(const string& s, size_t& i);
static char parseChar(const string& s, size_t& i);
static char parseEscapedChar(const string& s, size_t& i);

/** 入口：解析完整模式串，并检查是否消费全部输入 */
RegexNode* RegExpParser::parse(const string& regex) {
    if (regex.empty()) {
        RegexNode* node = new RegexNode(RegexType::CHAR);
        node->ch = 0;
        return node;
    }
    size_t i = 0;
    RegexNode* node = parseExpr(regex, i);
    if (i != regex.size()) {
        cerr << "Regex parse error: unexpected character '" << regex[i] << "' at position " << i << endl;
        delete node;
        return nullptr;
    }
    return node;
}

/** 解析 alternation：term | term | ... */
static RegexNode* parseExpr(const string& s, size_t& i) {
    RegexNode* left = parseTerm(s, i);
    while (i < s.size() && s[i] == '|') {
        i++;
        RegexNode* right = parseTerm(s, i);
        if (!right) {
            cerr << "Parse error: expected expression after '|'" << endl;
            delete left;
            return nullptr;
        }
        RegexNode* node = new RegexNode(RegexType::ALT);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

/** 解析隐式连接：相邻 factor 构成 CONCAT 节点 */
static RegexNode* parseTerm(const string& s, size_t& i) {
    RegexNode* left = parseFactor(s, i);
    if (!left) return nullptr;
    while (i < s.size() && s[i] != '|' && s[i] != ')') {
        RegexNode* right = parseFactor(s, i);
        if (!right) break;
        RegexNode* node = new RegexNode(RegexType::CONCAT);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

/** 解析带后缀量词的因子：atom * | + | ? */
static RegexNode* parseFactor(const string& s, size_t& i) {
    if (i >= s.size()) return nullptr;
    RegexNode* node = parseAtom(s, i);
    if (!node) return nullptr;
    if (i < s.size() && (s[i] == '*' || s[i] == '+' || s[i] == '?')) {
        char op = s[i++];
        RegexType t = (op == '*') ? RegexType::STAR : (op == '+') ? RegexType::PLUS : RegexType::OPT;
        RegexNode* newNode = new RegexNode(t);
        newNode->left = node;
        node = newNode;
    }
    return node;
}

/** 解析原子：分组、字符类、点号、引号串或单字符字面量 */
static RegexNode* parseAtom(const string& s, size_t& i) {
    if (i >= s.size()) return nullptr;
    char c = s[i];
    if (c == '(') {
        i++;
        RegexNode* node = parseExpr(s, i);
        if (!node) {
            cerr << "Parse error: empty group or invalid expression after '('" << endl;
            return nullptr;
        }
        if (i >= s.size() || s[i] != ')') {
            cerr << "Missing closing ')' at position " << i << endl;
            delete node;
            return nullptr;
        }
        i++;
        return node;
    }
    else if (c == '[') {
        return parseRange(s, i);
    }
    else if (c == '.') {
        /* Flex 约定：. 匹配除换行外的任意字节 */
        i++;
        RegexNode* node = new RegexNode(RegexType::RANGE);
        for (int ch = 1; ch <= 255; ++ch) {
            if (ch != '\n') node->rangeChars.push_back((char)ch);
        }
        return node;
    }
    else if (c == '\\') {
        char ch = parseChar(s, i);
        RegexNode* node = new RegexNode(RegexType::CHAR);
        node->ch = ch;
        return node;
    }
    else if (c == '"') {
        /* 双引号内字符按顺序 CONCAT，用于匹配固定串如 "++" */
        i++;
        string literal;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                literal += parseChar(s, i);
            } else {
                literal += s[i++];
            }
        }
        if (i >= s.size() || s[i] != '"') {
            cerr << "Unterminated string literal starting at position " << (i - literal.length() - 1) << endl;
            return nullptr;
        }
        i++;
        if (literal.empty()) {
            RegexNode* node = new RegexNode(RegexType::OPT);
            node->left = new RegexNode(RegexType::CHAR);
            node->left->ch = 0;
            return node;
        }
        RegexNode* node = nullptr;
        for (char ch : literal) {
            RegexNode* charNode = new RegexNode(RegexType::CHAR);
            charNode->ch = ch;
            if (!node) {
                node = charNode;
            } else {
                RegexNode* concat = new RegexNode(RegexType::CONCAT);
                concat->left = node;
                concat->right = charNode;
                node = concat;
            }
        }
        return node;
    }
    else {
        /* 未加引号的字符按字面量 CHAR 处理（含 ( ) 等） */
        RegexNode* node = new RegexNode(RegexType::CHAR);
        node->ch = s[i++];
        return node;
    }
}

/** 解析反斜杠后的转义序列（\n \t \\ 等） */
static char parseEscapedChar(const string& s, size_t& i) {
    /* 调用时 i 已越过 '\\'，指向转义目标字符 */
    if (i >= s.size()) return '\\';
    char c = s[i++];
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'v': return '\v';
        case 'f': return '\f';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        case 'a': return '\a';
        case 'b': return '\b';
        default: return c;
    }
}

/** 读取一个逻辑字符：普通字符或 \\ 开头的转义 */
static char parseChar(const string& s, size_t& i) {
    if (i >= s.size()) return 0;
    char c = s[i++];
    if (c == '\\') {
        return parseEscapedChar(s, i);
    }
    return c;
}

/**
 * 解析字符类 [...]：支持 ^ 取反、a-z 范围、类内转义。
 * 结果存入 RANGE 节点的 rangeChars（去重排序）。
 */
static RegexNode* parseRange(const string& s, size_t& i) {
    if (i >= s.size() || s[i] != '[') return nullptr;
    i++; /* 跳过 '[' */
    bool invert = false;
    if (i < s.size() && s[i] == '^') {
        invert = true;
        i++;
    }
    vector<char> chars;
    while (i < s.size() && s[i] != ']') {
        if (s[i] == '\\') {
            i++; // skip backslash
            if (i >= s.size()) {
                cerr << "Unexpected end after backslash in character class" << endl;
                return nullptr;
            }
            char ch = parseEscapedChar(s, i);
            chars.push_back(ch);
        } else {
            char start = s[i++];
            if (i < s.size() && s[i] == '-' && i+1 < s.size() && s[i+1] != ']') {
                i++; /* 跳过 '-'，读取范围上界 */
                char end = s[i++];
                if (start > end) {
                    cerr << "Invalid range: " << start << "-" << end << endl;
                    return nullptr;
                }
                for (char c = start; c <= end; ++c)
                    chars.push_back(c);
            } else {
                chars.push_back(start);
            }
        }
    }
    if (i >= s.size() || s[i] != ']') {
        cerr << "Missing closing ']' in character class" << endl;
        return nullptr;
    }
    i++; /* 跳过 ']' */

    if (invert) {
        /* 补全集 1..255 再减去已列字符 */
        vector<char> all;
        for (int c = 1; c <= 255; ++c) all.push_back((char)c);
        for (char c : chars)
            all.erase(remove(all.begin(), all.end(), c), all.end());
        chars = all;
    }

    sort(chars.begin(), chars.end());
    chars.erase(unique(chars.begin(), chars.end()), chars.end());

    RegexNode* node = new RegexNode(RegexType::RANGE);
    node->rangeChars = chars;
    return node;
}