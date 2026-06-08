/*
 * CodeGenerator.h — 最小化 DFA 到 C 词法分析代码的生成器接口
 *
 * 输出 lex.yy.cpp / lex.yy.h：嵌入用户 prologue/epilogue、
 * 转移表 nextState[][]、接受表 acceptInfo[] 及最长匹配的 yylex 实现。
 */
#pragma once
#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "lex_common.h"

/**
 * 根据最小化 DFA 与规则动作列表生成可编译的 Flex 兼容词法器源文件。
 */
class CodeGenerator {
public:
    /**
     * @param dfaStates 最小化后的 DFA
     * @param rules     各规则的展开模式与动作代码
     * @param prologue  %{ ... %} 中的 C 代码（插入生成 .cpp 头部）
     * @param epilogue  第二个 %% 之后的用户代码
     */
    CodeGenerator(const std::vector<DFAState>& dfaStates,
        const std::vector<Rule>& rules,
        const std::string& prologue,
        const std::string& epilogue);

    /**
     * 写入词法器头文件与实现文件。
     * @param outCpp 一般为 lex.yy.cpp
     * @param outH   一般为 lex.yy.h
     */
    void generate(const std::string& outCpp, const std::string& outH);
private:
    std::vector<DFAState> states;
    std::vector<Rule> rules;
    std::string prologue;
    std::string epilogue;
};

#endif
