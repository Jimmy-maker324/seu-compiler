/*
 * NFAConstructor.h — 正则 AST 到 NFA 的 Thompson 构造接口
 *
 * 按 AST 节点类型递归拼装 ε-NFA，并支持将多条规则的 NFA
 * 合并为带统一起点的多接受态自动机。
 */
#pragma once
#ifndef NFA_CONSTRUCTOR_H
#define NFA_CONSTRUCTOR_H

#include "lex_common.h"

/**
 * Thompson 构造法：从 RegexNode 生成 NFA，并合并多规则。
 */
class NFAConstructor {
public:
    /**
     * 对单条正则 AST 构建 NFA（含起止态）。
     * @param ast 正则语法树根节点，可为 nullptr（视为 ε）
     */
    static NFA build(RegexNode* ast);

    /**
     * 将多条规则的 NFA 用 ε 边并联到新的全局起点，
     * 各规则接受态标记 ruleId 并清除出边以防贪婪延长匹配。
     */
    static NFA mergeRules(const std::vector<NFA>& ruleNfas);
};

#endif
