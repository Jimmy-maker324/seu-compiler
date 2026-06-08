/*
 * DFAConverter.h — NFA 到 DFA 的子集构造法接口
 *
 * 对合并后的 ε-NFA 做子集构造，得到按字节转移的确定有限自动机。
 */
#pragma once
#ifndef DFA_CONVERTER_H
#define DFA_CONVERTER_H

#include "lex_common.h"

/**
 * 子集构造（subset construction）：ε-闭包 + move + 状态去重。
 */
class DFAConverter {
public:
    /**
     * 将 NFA 转换为 DFA 状态向量；状态 0 为起始态。
     * @param nfa 已合并的多规则 NFA
     */
    static std::vector<DFAState> convert(const NFA& nfa);
};

#endif
