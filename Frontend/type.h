/**
 * @file type.h
 * @brief 编译器类型系统定义
 *
 * 【设计】面向对象的类型层次，用 TypeKind 区分种类，复合类型持子类型指针。
 *
 * 【数据结构关系】
 *   Type (基类, kind)
 *     ├── BasicType  — Void/Int/Char/Float/Double 单例指针（type.cpp 静态分配）
 *     ├── PointerType — base 指向被指向类型
 *     ├── ArrayType   — base 元素类型, size 长度（-1 未知）
 *     └── FunctionType — returnType + paramTypes 向量
 *
 * 【使用场景】
 *   - yacc.y 构建 AST 时 TypeNode 持有 Type*
 *   - symbol.cpp 每个 Symbol 绑定 Type*
 *   - typecheck.cpp 结构等价比较、isInt() 检查
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <vector>

/**
 * @enum TypeKind
 * @brief 类型种类枚举
 */
enum class TypeKind {
    Void, Int, Char, Float, Double, Pointer, Array, Function, Struct, Union
};

/**
 * @class Type
 * @brief 类型基类
 */
class Type {
public:
    TypeKind kind;  ///< 类型种类
    /** @brief 构造指定种类的类型对象 */
    explicit Type(TypeKind k) : kind(k) {}
    virtual ~Type() = default;
    /** @brief 是否为 int 类型 */
    virtual bool isInt() const { return kind == TypeKind::Int; }
    /** @brief 是否为 void 类型 */
    virtual bool isVoid() const { return kind == TypeKind::Void; }
    /** @brief 是否为浮点类型（float / double） */
    virtual bool isFloat() const {
        return kind == TypeKind::Float || kind == TypeKind::Double;
    }
    /** @brief 是否为数值类型（int / char / float / double） */
    virtual bool isNumeric() const {
        return kind == TypeKind::Int || kind == TypeKind::Char ||
               kind == TypeKind::Float || kind == TypeKind::Double;
    }
    /** @brief 将类型格式化为 C 风格字符串 */
    virtual std::string toString() const;
};

/**
 * @class BasicType
 * @brief 基本类型（void / int / char / float / double），使用静态单例
 */
class BasicType : public Type {
public:
    BasicType(TypeKind k) : Type(k) {}
    static Type* Void;    ///< void 类型单例
    static Type* Int;     ///< int 类型单例
    static Type* Char;    ///< char 类型单例
    static Type* Float;   ///< float 类型单例
    static Type* Double;  ///< double 类型单例
};

/**
 * @class PointerType
 * @brief 指针类型，base 指向被指向的类型
 */
class PointerType : public Type {
public:
    Type* base;  ///< 基类型
    /** @brief 构造指针类型 */
    PointerType(Type* b) : Type(TypeKind::Pointer), base(b) {}
};

/**
 * @class ArrayType
 * @brief 数组类型
 */
class ArrayType : public Type {
public:
    Type* base;  ///< 元素类型
    int size;    ///< 数组长度；-1 表示未指定大小（如 extern 声明）
    /** @brief 构造数组类型 */
    ArrayType(Type* b, int sz) : Type(TypeKind::Array), base(b), size(sz) {}
};

/**
 * @class FunctionType
 * @brief 函数类型，包含返回类型与形参类型列表
 */
class FunctionType : public Type {
public:
    Type* returnType;              ///< 返回类型
    std::vector<Type*> paramTypes; ///< 形参类型序列
    /** @brief 构造函数类型（仅指定返回类型） */
    FunctionType(Type* ret) : Type(TypeKind::Function), returnType(ret) {}
    /** @brief 追加一个形参类型 */
    void addParam(Type* t) { paramTypes.push_back(t); }
};

/**
 * @class StructType
 * @brief 结构体类型，包含 tag 名与有序成员列表
 */
class StructType : public Type {
public:
    std::string name;  ///< 结构体 tag 名
    struct Member {
        std::string name;
        Type* type;
    };
    std::vector<Member> members;

    explicit StructType(const std::string& n) : Type(TypeKind::Struct), name(n) {}

    Type* getMemberType(const std::string& member) const;
    bool hasMember(const std::string& member) const;
    void addMember(const std::string& member, Type* type);
};

/** @brief 查找已注册的结构体类型，未定义则返回 nullptr */
StructType* lookupStructType(const std::string& name);
StructType* defineStructType(const std::string& name);

/**
 * @class UnionType
 * @brief 联合体类型，包含 tag 名与成员列表（与 struct 共享成员访问语义）
 */
class UnionType : public Type {
public:
    std::string name;
    struct Member {
        std::string name;
        Type* type;
    };
    std::vector<Member> members;

    explicit UnionType(const std::string& n) : Type(TypeKind::Union), name(n) {}

    Type* getMemberType(const std::string& member) const;
    bool hasMember(const std::string& member) const;
    void addMember(const std::string& member, Type* type);
};

UnionType* lookupUnionType(const std::string& name);
UnionType* defineUnionType(const std::string& name);

#endif
