/**
 * @file ast_printer.h
 * @brief AST 文本树形打印器
 *
 * 【算法】先序遍历 + 缩进 depth*2 空格；MultiNode 遍历 children，
 *         二元/一元节点先打印本节点再递归左右子树。
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef AST_PRINTER_H
#define AST_PRINTER_H

#include "ast.h"
#include <iostream>
#include <ostream>

/**
 * @class ASTPrinter
 * @brief 递归遍历 AST 并以可读文本格式打印
 */
class ASTPrinter {
public:
    /** @brief 从根节点开始打印整棵 AST */
    void print(ASTNode* root, std::ostream& out = std::cout);
private:
    /** @brief 递归打印单个节点及其子树，indent 为当前缩进空格数 */
    void printNode(ASTNode* node, int indent, std::ostream& out);
};

#endif
