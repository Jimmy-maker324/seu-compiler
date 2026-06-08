/*
 * ============================================================================
 * NFAConstructor.cpp — Thompson 构造法（正则 → ε-NFA）
 * ============================================================================
 *
 * 【数据结构 NFA】
 *   vector<NFAState> states — 每态含 trans: vector<pair<字符,目标态>>
 *   startState, acceptState — 片段入口/出口
 *   边标签 EPSILON(-1) 表示 ε 转移
 *
 * 【Thompson 片段设计】
 *   CHAR c:   (s0) --c--> (s1)
 *   CONCAT:   N(left) --ε--> N(right)，整体起止为 left 起、right 止
 *   ALT:      新起止，ε 分叉到两子 NFA，接受态 ε 汇合
 *   STAR:     新起止 + ε 跳过/重复/结束（Kleene 闭包）
 *   PLUS:     至少一次（类似 STAR 但不可零次跳过）
 *   OPT:      ε 跳过或走子 NFA 一次
 *
 * 【copyNFA】将子 NFA 状态追加到 result，边目标 +offset 重编号
 *
 * 【mergeRules】多词法规则并联：统一 super_start --ε--> 各规则 NFA 起点
 *   接受态记录 ruleId，供 DFA 接受时选择最长/最前匹配规则（Flex 语义）
 * ============================================================================
 */
#include "NFAConstructor.h"
#include "lex_common.h"
using namespace std;

/**
 * 将 src 的全部状态复制到 dst 末尾，返回本次复制的状态 id 偏移量。
 * 复制后需将每条边的目标状态 id 加上 offset。
 */
static void copyNFA(const NFA& src, NFA& dst, int& offset) {
    offset = (int)dst.states.size();
    for (const auto& st : src.states) {
        dst.states.push_back(st);
        NFAState& newSt = dst.states.back();
        newSt.id = offset + st.id;
        newSt.isAccept = st.isAccept;
        newSt.ruleId = st.ruleId;
        /* 边目标指向 dst 中的新编号，须加上 offset */
        for (auto& p : newSt.trans) {
            p.second += offset;
        }
    }
}

/** 按 AST 节点类型递归构造 NFA 片段 */
NFA NFAConstructor::build(RegexNode* ast) {
    if (!ast) {
        /* 空 AST：仅 ε 转移的起止态 */
        NFA nfa;
        nfa.startState = nfa.addState();
        nfa.acceptState = nfa.addState();
        nfa.addEpsEdge(nfa.startState, nfa.acceptState);
        return nfa;
    }
    switch (ast->type) {
    case RegexType::CHAR: {
        /* 单字符边；ch==0 表示 ε */
        NFA nfa;
        nfa.startState = nfa.addState();
        nfa.acceptState = nfa.addState();
        if (ast->ch != 0) {
            nfa.addEdge(nfa.startState, nfa.acceptState, (unsigned char)ast->ch);
        } else {
            nfa.addEpsEdge(nfa.startState, nfa.acceptState);
        }
        return nfa;
    }
    case RegexType::RANGE: {
        /* 字符类：起点到终点对每个字符各建一条并行边 */
        NFA nfa;
        nfa.startState = nfa.addState();
        nfa.acceptState = nfa.addState();
        for (char c : ast->rangeChars) {
            nfa.addEdge(nfa.startState, nfa.acceptState, (unsigned char)c);
        }
        return nfa;
    }
    case RegexType::CONCAT: {
        /* left 接受态 ε 连接 right 起始态 */
        NFA left = build(ast->left);
        NFA right = build(ast->right);
        NFA result;
        int offsetL, offsetR;
        copyNFA(left, result, offsetL);
        copyNFA(right, result, offsetR);
        result.startState = offsetL + left.startState;
        result.acceptState = offsetR + right.acceptState;
        result.addEpsEdge(offsetL + left.acceptState, offsetR + right.startState);
        return result;
    }
    case RegexType::ALT: {
        /* 新起止态，ε 分叉到 left/right，两路接受态汇合 */
        NFA left = build(ast->left);
        NFA right = build(ast->right);
        NFA result;
        result.startState = result.addState();
        result.acceptState = result.addState();
        int offsetL, offsetR;
        copyNFA(left, result, offsetL);
        copyNFA(right, result, offsetR);
        result.addEpsEdge(result.startState, offsetL + left.startState);
        result.addEpsEdge(result.startState, offsetR + right.startState);
        result.addEpsEdge(offsetL + left.acceptState, result.acceptState);
        result.addEpsEdge(offsetR + right.acceptState, result.acceptState);
        return result;
    }
    case RegexType::STAR: {
        /* Kleene 星：可跳过、可重复、可结束 */
        NFA inner = build(ast->left);
        NFA result;
        result.startState = result.addState();
        result.acceptState = result.addState();
        int offset;
        copyNFA(inner, result, offset);
        result.addEpsEdge(result.startState, offset + inner.startState);
        result.addEpsEdge(result.startState, result.acceptState);
        result.addEpsEdge(offset + inner.acceptState, offset + inner.startState);
        result.addEpsEdge(offset + inner.acceptState, result.acceptState);
        return result;
    }
    case RegexType::PLUS: {
        /* 至少一次：须经过 inner，接受后可循环或结束 */
        NFA inner = build(ast->left);
        NFA result;
        result.startState = result.addState();
        result.acceptState = result.addState();
        int offset;
        copyNFA(inner, result, offset);
        result.addEpsEdge(result.startState, offset + inner.startState);
        result.addEpsEdge(offset + inner.acceptState, offset + inner.startState);
        result.addEpsEdge(offset + inner.acceptState, result.acceptState);
        return result;
    }
    case RegexType::OPT: {
        /* 零次或一次：起点可 ε 直达接受，或经 inner */
        NFA inner = build(ast->left);
        NFA result;
        result.startState = result.addState();
        result.acceptState = result.addState();
        int offset;
        copyNFA(inner, result, offset);
        result.addEpsEdge(result.startState, offset + inner.startState);
        result.addEpsEdge(result.startState, result.acceptState);
        result.addEpsEdge(offset + inner.acceptState, result.acceptState);
        return result;
    }
    default: return NFA();
    }
}

/**
 * 多规则合并：全局起点 ε 指向各规则 NFA 起点；
 * 接受态记录 ruleId（1-based），并清空出边以实现“首次匹配即停”。
 */
NFA NFAConstructor::mergeRules(const vector<NFA>& ruleNfas) {
    NFA merged;
    merged.startState = merged.addState();
    for (size_t i = 0; i < ruleNfas.size(); ++i) {
        const auto& nfa = ruleNfas[i];
        int offset;
        copyNFA(nfa, merged, offset);
        merged.addEpsEdge(merged.startState, offset + nfa.startState);
        int accept = offset + nfa.acceptState;
        merged.states[accept].isAccept = true;
        merged.states[accept].ruleId = (int)i + 1;
        /* 接受态无出边：防止继续读入字符导致更长但错误的路径 */
        merged.states[accept].trans.clear();
    }
    return merged;
}