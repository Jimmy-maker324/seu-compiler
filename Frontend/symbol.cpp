/**
 * @file symbol.cpp
 * @brief 符号表与作用域管理实现
 *
 * 【lookup 算法】从当前 Scope 沿 parent 向上，在每层 symbols 哈希表中查找
 *   时间复杂度 O(层数)，平均 O(1) 每层哈希查找
 *
 * 【enterScope】new Scope(parent=currentScope)，实现嵌套块/函数形参隔离
 * 【leaveScope】delete 当前层全部 Symbol，恢复 parent
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#include "symbol.h"
#include <iostream>

/** @brief 析构时释放本作用域内所有符号对象 */
Scope::~Scope() {
    for (auto& p : symbols) delete p.second;
}

/** @brief 将符号插入本层哈希表，键为符号名 */
void Scope::insert(Symbol* sym) {
    symbols[sym->name] = sym;
}

/** @brief 仅在本层哈希表中查找 */
Symbol* Scope::lookupLocal(const std::string& name) {
    auto it = symbols.find(name);
    return it != symbols.end() ? it->second : nullptr;
}

/** @brief 从本层向外逐层查找，找到即返回（内层遮蔽外层同名符号） */
Symbol* Scope::lookup(const std::string& name) {
    for (Scope* s = this; s; s = s->parent) {
        auto it = s->symbols.find(name);
        if (it != s->symbols.end()) return it->second;
    }
    return nullptr;
}

/** @brief 全局当前作用域指针，初始为 nullptr（尚未进入任何作用域） */
Scope* currentScope = nullptr;

/** @brief 局部变量 IR 名后缀计数（全局/函数符号不参与） */
static int varUidCounter = 0;

/** @brief 清空符号表并重置 IR 局部名计数 */
void resetSymbolTable() {
    while (currentScope)
        leaveScope();
    varUidCounter = 0;
}

/** @brief 创建新作用域，parent 指向原 currentScope */
void enterScope() {
    currentScope = new Scope(currentScope);
}

/** @brief 弹出当前作用域，恢复父作用域并 delete 当前层 */
void leaveScope() {
    Scope* old = currentScope;
    currentScope = currentScope->parent;
    delete old;
}

/** @brief 在当前作用域注册新符号；首次调用时自动 enterScope */
Symbol* addSymbol(const std::string& name, Type* type, bool isFunc) {
    if (!currentScope)
        enterScope();
    Symbol* sym = new Symbol(name, type, isFunc);
    if (isFunc || !currentScope->parent) {
        sym->irName = name;
    } else {
        sym->irName = name + "_" + std::to_string(varUidCounter++);
    }
    currentScope->insert(sym);
    return sym;
}

/** @brief 从 currentScope 链上查找符号；无作用域时返回 nullptr */
Symbol* getSymbol(const std::string& name) {
    return currentScope ? currentScope->lookup(name) : nullptr;
}
