/*
 * LexFileParser.h — Flex 规格文件（.l）解析器接口
 *
 * 负责读取 .l 文件、拆分定义区/规则区/用户代码区，
 * 并串联后续正则解析与自动机代码生成流水线。
 */
#pragma once
#ifndef LEX_FILE_PARSER_H
#define LEX_FILE_PARSER_H

#include "lex_common.h"

/**
 * Flex 文件解析与编译驱动类。
 * 将 .l 解析为规则列表后，调用 NFA→DFA→最小化→CodeGenerator。
 */
class LexFileParser {
public:
    /**
     * 解析并编译指定的 Flex 输入文件。
     * @param filename .l 文件路径（须含 %% 分隔符）
     */
    void parse(const std::string& filename);
};

#endif
