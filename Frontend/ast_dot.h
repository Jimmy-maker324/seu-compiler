/**
 * @file ast_dot.h
 * @brief AST Graphviz DOT 格式导出器
 *
 * 【算法】DFS 遍历；每节点分配 n{id}，边 parent→child
 * 【数据结构】输出 digraph AST { node[label=...]; edge ... }
 * 【转义】escapeDot 处理 DOT 特殊字符
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef AST_DOT_H
#define AST_DOT_H

#include "ast.h"
#include <ostream>
#include <string>

/**
 * @class ASTDotExporter
 * @brief 深度优先遍历 AST，生成有向图 DOT 描述
 */
class ASTDotExporter {
public:
    /** @brief 将 AST 导出到指定文件 */
    void exportToFile(ASTNode* root, const std::string& filename);
    /** @brief 将 AST 导出到任意输出流 */
    void exportToStream(ASTNode* root, std::ostream& out);

private:
    int nextId_;  ///< 节点 ID 计数器，保证每个 DOT 节点唯一
    /** @brief 访问入口，委托 visitNode 处理 */
    void visit(ASTNode* node, std::ostream& out);
    /** @brief 为当前节点分配 ID、输出节点与边，返回本节点 ID */
    int visitNode(ASTNode* node, std::ostream& out);
    /** @brief 根据节点种类生成 DOT label 字符串 */
    static std::string labelFor(ASTNode* node);
    /** @brief 转义 DOT 标签中的特殊字符 */
    static std::string escapeDot(const std::string& s);
};

/** @brief 调用 Graphviz 将 DOT 文件渲染为 PNG；成功返回 true */
bool renderDotToPng(const std::string& dotFile, const std::string& pngFile);

#endif
