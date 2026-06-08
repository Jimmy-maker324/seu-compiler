/*
 * lex_common.h — SeuLex 模块公共类型与常量定义
 *
 * 定义正则 AST 节点、NFA/DFA 状态结构、词法规则记录，
 * 供 LexFileParser、RegExpParser、NFAConstructor、DFAConverter、
 * DFAMinimizer、CodeGenerator 等子模块共享。
 */
/**
 * @file lex_common.h
 * @brief SeuLex 内部共享数据结构（正则 AST、NFA、DFA、规则）
 *
 * 【RegexNode】正则抽象语法树，供 RegExpParser 输出、NFAConstructor 输入
 * 【NFA / NFAState】Thompson 构造的中间自动机，含 ε 边
 * 【DFAState】子集构造结果，next[256] 为确定性转移
 * 【Rule】一条词法规则：regexStr + actionCode + 规则序号 id
 */
#pragma once
#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <stack>
#include <queue>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>

/** 输入字母表大小（按字节 0..255 建 DFA 转移表） */
#define ALPHABET_SIZE 256
/** NFA 中空转移（ε 边）的标记，不与任何输入字符冲突 */
#define EPSILON   -1

using namespace std;

/** 正则表达式抽象语法树节点类型 */
enum class RegexType {
    CHAR,    /* 单字符或 ε（ch==0） */
    CONCAT,  /* 连接 ab */
    ALT,     /* 选择 a|b */
    STAR,    /* 闭包 a* */
    PLUS,    /* 正闭包 a+ */
    OPT,     /* 可选 a? */
    RANGE    /* 字符类 [a-z] 或 . 的展开集合 */
};

/** 正则 AST 节点：递归结构，析构时释放子树 */
struct RegexNode {
    RegexType type;
    char ch;                      /* CHAR 类型的字面字符 */
    RegexNode* left;
    RegexNode* right;
    vector<char> rangeChars;      /* RANGE 类型：允许出现的字符集合 */
    RegexNode(RegexType t) : type(t), ch(0), left(nullptr), right(nullptr) {}
    ~RegexNode() { delete left; delete right; }
};

/** NFA 状态：带编号的有向边表，可标记为某条规则的接受态 */
struct NFAState {
    int id;
    vector<pair<int, int>> trans; /* (输入符号或 EPSILON, 目标状态 id) */
    bool isAccept;
    int ruleId;                   /* 接受时对应的 Flex 规则编号（从 1 起） */
    NFAState(int i) : id(i), isAccept(false), ruleId(-1) {}
};

/** 非确定有限自动机：Thompson 构造的结果 */
class NFA {
public:
    vector<NFAState> states;
    int startState;
    int acceptState;
    NFA() : startState(-1), acceptState(-1) {}
    int addState() { states.emplace_back((int)states.size()); return (int)states.size() - 1; }
    void addEdge(int from, int to, int ch) { states[from].trans.emplace_back(ch, to); }
    void addEpsEdge(int from, int to) { addEdge(from, to, EPSILON); }
};

/** DFA 状态：对应 NFA 状态集合的子集，含完整 next[256] 转移表 */
struct DFAState {
    set<int> nfaStates;           /* 子集构造法中的 NFA 状态集合 */
    bool isAccept;
    int ruleId;                   /* 多规则冲突时取最小 ruleId（最长匹配优先） */
    int next[ALPHABET_SIZE];      /* 按输入字节转移，-1 表示无转移 */
    DFAState() : isAccept(false), ruleId(-1) { memset(next, -1, sizeof(next)); }
};

/** 一条 Flex 规则：展开后的正则模式 + 动作代码块 */
struct Rule {
    string regexStr;              /* 宏展开后的模式串 */
    string actionCode;            /* 花括号内的 C 动作，原样嵌入生成代码 */
    int id;                       /* 规则序号，与 DFA 接受态 ruleId 一致 */
};

#endif
