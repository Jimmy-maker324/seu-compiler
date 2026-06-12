/**
 * @file ast_walk.h
 * @brief AST 语句/函数定义的共享遍历辅助（TypeChecker 与 IRGenerator 共用）
 *
 * 统一 FuncDef 子节点布局、CompoundStmt 作用域跳过、控制流子节点索引，
 * 避免两处 visitStmt 各自维护一份相同约定。
 */

#ifndef AST_WALK_H
#define AST_WALK_H

#include "ast.h"
#include <functional>

namespace astwalk {

/** @brief FuncDef 子节点：[0]返回类型 [1]函数名 [2]?ParamList [bodyIdx]函数体 */
struct FuncDefLayout {
    TypeNode* retType = nullptr;
    IdentifierNode* id = nullptr;
    MultiNode* paramList = nullptr;
    size_t bodyIdx = 0;
    ASTNode* body = nullptr;
};

/** @brief 解析 FuncDef 布局；失败时返回 false */
inline bool parseFuncDefLayout(MultiNode* func, FuncDefLayout& out) {
    if (!func || func->children.size() < 3)
        return false;
    out.retType = static_cast<TypeNode*>(func->children[0].get());
    out.id = static_cast<IdentifierNode*>(func->children[1].get());
    out.bodyIdx = 2;
    out.paramList = nullptr;
    if (func->children[2]->kind == NodeKind::ParamList) {
        out.paramList = static_cast<MultiNode*>(func->children[2].get());
        out.bodyIdx = 3;
    }
    out.body = out.bodyIdx < func->children.size()
                   ? func->children[out.bodyIdx].get()
                   : nullptr;
    return out.retType && out.id;
}

/** @brief VarDecl 子节点：[0]TypeNode [1]IdentifierNode [2]?初值 */
struct VarDeclLayout {
    TypeNode* typeNode = nullptr;
    IdentifierNode* id = nullptr;
    ASTNode* init = nullptr;
};

inline bool parseVarDeclLayout(MultiNode* decl, VarDeclLayout& out) {
    if (!decl || decl->children.size() < 2)
        return false;
    out.typeNode = static_cast<TypeNode*>(decl->children[0].get());
    out.id = static_cast<IdentifierNode*>(decl->children[1].get());
    out.init = decl->children.size() >= 3 ? decl->children[2].get() : nullptr;
    return out.typeNode && out.id;
}

/**
 * @brief 遍历 CompoundStmt 子语句
 * @param skipCompoundScope 函数体最外层块与形参共享作用域时为 true
 * @param enterScope / leaveScope 由调用方注入（可带日志）
 */
template<typename EnterFn, typename LeaveFn, typename VisitFn>
inline void walkCompound(MultiNode* block, bool& skipCompoundScope,
                         EnterFn enterScope, LeaveFn leaveScope, VisitFn visitStmt) {
    if (!block) return;
    auto walkChildren = [&]() {
        for (auto& child : block->children) {
            if (child)
                visitStmt(child.get());
        }
    };
    if (skipCompoundScope) {
        skipCompoundScope = false;
        walkChildren();
    } else {
        enterScope();
        walkChildren();
        leaveScope();
    }
}

/** @brief 遍历 ParamList 中每个 ParamDecl（TypeNode + IdentifierNode） */
template<typename ParamFn>
inline void walkParams(MultiNode* paramList, ParamFn onParam) {
    if (!paramList) return;
    for (auto& pch : paramList->children) {
        auto* p = static_cast<MultiNode*>(pch.get());
        if (p && p->children.size() >= 2)
            onParam(static_cast<TypeNode*>(p->children[0].get()),
                    static_cast<IdentifierNode*>(p->children[1].get()));
    }
}

/** @brief switch 各 case/default 子句内的语句（跳过 case 常量节点） */
template<typename ClauseFn>
inline void walkSwitchClauseStmts(MultiNode* sw, ClauseFn onClauseStmt) {
    if (!sw) return;
    for (size_t i = 1; i < sw->children.size(); ++i) {
        auto* clause = static_cast<MultiNode*>(sw->children[i].get());
        if (!clause) continue;
        size_t start = (clause->kind == NodeKind::CaseStmt) ? 1 : 0;
        for (size_t j = start; j < clause->children.size(); ++j)
            onClauseStmt(clause->children[j].get());
    }
}

} // namespace astwalk

#endif
