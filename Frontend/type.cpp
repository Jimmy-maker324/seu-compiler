/**
 * @file type.cpp
 * @brief 类型系统实现
 *
 * 【单例模式】BasicType::Void/Int/Char 全局唯一，避免重复分配
 * 【toString】递归打印类型结构，用于调试与错误信息
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#include "type.h"
#include <unordered_map>

static std::unordered_map<std::string, StructType*> structRegistry;
static std::unordered_map<std::string, UnionType*> unionRegistry;

Type* StructType::getMemberType(const std::string& member) const {
    for (const Member& m : members) {
        if (m.name == member) return m.type;
    }
    return nullptr;
}

bool StructType::hasMember(const std::string& member) const {
    return getMemberType(member) != nullptr;
}

void StructType::addMember(const std::string& member, Type* type) {
    members.push_back({member, type});
}

StructType* lookupStructType(const std::string& name) {
    auto it = structRegistry.find(name);
    return it != structRegistry.end() ? it->second : nullptr;
}

StructType* defineStructType(const std::string& name) {
    auto* st = new StructType(name);
    structRegistry[name] = st;
    return st;
}

Type* UnionType::getMemberType(const std::string& member) const {
    for (const Member& m : members) {
        if (m.name == member) return m.type;
    }
    return nullptr;
}

bool UnionType::hasMember(const std::string& member) const {
    return getMemberType(member) != nullptr;
}

void UnionType::addMember(const std::string& member, Type* type) {
    members.push_back({member, type});
}

UnionType* lookupUnionType(const std::string& name) {
    auto it = unionRegistry.find(name);
    return it != unionRegistry.end() ? it->second : nullptr;
}

UnionType* defineUnionType(const std::string& name) {
    auto* ut = new UnionType(name);
    unionRegistry[name] = ut;
    return ut;
}

/** @brief 递归将类型格式化为 C 风格字符串（如 int*、char[10]、struct Point） */
std::string Type::toString() const {
    switch (kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Int:    return "int";
        case TypeKind::Char:   return "char";
        case TypeKind::Float:  return "float";
        case TypeKind::Double: return "double";
        case TypeKind::Pointer: {
            auto* pt = (PointerType*)this;
            return pt->base->toString() + "*";
        }
        case TypeKind::Array: {
            auto* at = (ArrayType*)this;
            // size >= 0 时输出具体长度，否则输出空括号表示未知大小
            return at->base->toString() + "[" + (at->size >= 0 ? std::to_string(at->size) : "") + "]";
        }
        case TypeKind::Function: {
            auto* ft = (FunctionType*)this;
            std::string s = ft->returnType->toString() + "(";
            for (size_t i = 0; i < ft->paramTypes.size(); i++) {
                s += ft->paramTypes[i]->toString();
                if (i != ft->paramTypes.size() - 1) s += ",";
            }
            s += ")";
            return s;
        }
        case TypeKind::Struct: {
            auto* st = (StructType*)this;
            return "struct " + st->name;
        }
        case TypeKind::Union: {
            auto* ut = (UnionType*)this;
            return "union " + ut->name;
        }
        default: return "unknown";
    }
}

// 基本类型全局单例，程序生命周期内常驻
Type* BasicType::Void   = new BasicType(TypeKind::Void);
Type* BasicType::Int    = new BasicType(TypeKind::Int);
Type* BasicType::Char   = new BasicType(TypeKind::Char);
Type* BasicType::Float  = new BasicType(TypeKind::Float);
Type* BasicType::Double = new BasicType(TypeKind::Double);
