/*
 * RegExpParser.h — 正则表达式语法分析器接口
 *
 * 将 Flex 风格的模式串（含 | * + ? () [] . 与转义）
 * 解析为 RegexNode 抽象语法树，供 Thompson NFA 构造使用。
 */
#pragma once
#ifndef REGEX_PARSER_H
#define REGEX_PARSER_H

#include "lex_common.h"

/**
 * 正则表达式递归下降解析器（仅静态方法，无实例状态）。
 */
class RegExpParser {
public:
    /**
     * 解析整条正则模式串为 AST 根节点。
     * @param regex 宏展开后的模式（调用方负责 delete 返回的节点）
     * @return AST 根指针；失败返回 nullptr
     */
    static RegexNode* parse(const std::string& regex);
};

#endif
