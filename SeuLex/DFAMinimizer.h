/*
 * DFAMinimizer.h — DFA 最小化（划分 refine）接口
 *
 * 在保持接受信息与转移等价的前提下合并等价状态，
 * 并保证原 DFA 状态 0（起始态）在最小化后仍映射到索引 0。
 */
#pragma once
#ifndef DFA_MINIMIZER_H
#define DFA_MINIMIZER_H

#include "lex_common.h"

/**
 * Hopcroft 风格迭代划分：先按 (isAccept, ruleId) 分组，再按转移签名细分。
 */
class DFAMinimizer {
public:
    /**
     * 对 DFA 状态表做等价类合并并重建转移。
     * @param dfa 子集构造得到的 DFA
     * @return 状态数更少的最小化 DFA，起始态在索引 0
     */
    static std::vector<DFAState> minimize(const std::vector<DFAState>& dfa);
};

#endif
