/**
 * @file symbol.h
 * @brief 符号表与作用域管理
 *
 * 【数据结构】Scope 单向链表 + 每层 unordered_map<name, Symbol*>
 * 【lookup】从内层向外层查找，实现块作用域遮蔽
 * 【enterScope/leaveScope】函数体、复合语句进入/退出时压栈弹栈
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>
#include <unordered_map>
#include "type.h"

/**
 * @struct Symbol
 * @brief 符号表项，描述一个标识符的名称、类型及存储信息
 */
struct Symbol {
    std::string name;    ///< 标识符名（源代码）
    std::string irName;  ///< IR 中的变量/函数名（全局/函数名保留源名，局部为 name_N）
    Type* type;          ///< 语义类型
    bool isFunction;     ///< true 表示函数符号，false 表示变量
    /** @brief 构造符号表项 */
    Symbol(const std::string& n, Type* t, bool isFunc = false)
        : name(n), irName(n), type(t), isFunction(isFunc) {}
};

/**
 * @class Scope
 * @brief 单层作用域，通过 parent 指针形成作用域链
 */
class Scope {
public:
    Scope* parent;  ///< 外层作用域指针，全局作用域为 nullptr
    std::unordered_map<std::string, Symbol*> symbols;  ///< 本层符号哈希表
    /** @brief 构造作用域并链接到父作用域 */
    Scope(Scope* p = nullptr) : parent(p) {}
    ~Scope();
    /** @brief 在本作用域插入符号 */
    void insert(Symbol* sym);
    /** @brief 仅在本层查找（不沿 parent 链向上） */
    Symbol* lookupLocal(const std::string& name);
    /** @brief 从当前作用域向外链查找符号，遵循最近作用域优先 */
    Symbol* lookup(const std::string& name);
};

/** @brief 当前活跃作用域的全局指针 */
extern Scope* currentScope;

/** @brief 进入新的嵌套作用域 */
void enterScope();
/** @brief 离开当前作用域并释放其符号 */
void leaveScope();
/** @brief 清空符号表（弹出全部作用域）并重置局部变量 IR 名计数 */
void resetSymbolTable();
/** @brief 在当前作用域添加符号；若无作用域则自动创建全局作用域 */
Symbol* addSymbol(const std::string& name, Type* type, bool isFunc = false);
/** @brief 从当前作用域链查找符号 */
Symbol* getSymbol(const std::string& name);

#endif
