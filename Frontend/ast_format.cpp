/**
 * @file ast_format.cpp
 * @brief AST 共享格式化实现
 */

#include "ast_format.h"
#include "common_defs.h"

const char* astKindName(NodeKind k) {
    switch (k) {
        case NodeKind::Program: return "Program";
        case NodeKind::VarDecl: return "VarDecl";
        case NodeKind::FuncDef: return "FuncDef";
        case NodeKind::ParamDecl: return "ParamDecl";
        case NodeKind::ParamList: return "ParamList";
        case NodeKind::CompoundStmt: return "CompoundStmt";
        case NodeKind::IfStmt: return "IfStmt";
        case NodeKind::WhileStmt: return "WhileStmt";
        case NodeKind::ForStmt: return "ForStmt";
        case NodeKind::SwitchStmt: return "SwitchStmt";
        case NodeKind::StructDef: return "StructDef";
        case NodeKind::UnionDef: return "UnionDef";
        case NodeKind::FieldDecl: return "FieldDecl";
        case NodeKind::FieldList: return "FieldList";
        case NodeKind::CaseStmt: return "CaseStmt";
        case NodeKind::DefaultStmt: return "DefaultStmt";
        case NodeKind::CaseList: return "CaseList";
        case NodeKind::ReturnStmt: return "ReturnStmt";
        case NodeKind::BreakStmt: return "BreakStmt";
        case NodeKind::ContinueStmt: return "ContinueStmt";
        case NodeKind::ExprStmt: return "ExprStmt";
        case NodeKind::NullStmt: return "NullStmt";
        case NodeKind::StmtList: return "StmtList";
        case NodeKind::ArgList: return "ArgList";
        case NodeKind::Identifier: return "Identifier";
        case NodeKind::Integer: return "Integer";
        case NodeKind::Float: return "Float";
        case NodeKind::String: return "String";
        case NodeKind::BinaryOp: return "BinaryOp";
        case NodeKind::UnaryOp: return "UnaryOp";
        case NodeKind::AssignOp: return "AssignOp";
        case NodeKind::Call: return "Call";
        case NodeKind::ArraySubscript: return "ArraySubscript";
        case NodeKind::MemberAccess: return "MemberAccess";
        case NodeKind::TypeNode: return "Type";
        default: return "Unknown";
    }
}

std::string astOpName(int op) {
    switch (op) {
        case T_OR_OP: return "||";
        case T_AND_OP: return "&&";
        case T_EQ_OP: return "==";
        case T_NE_OP: return "!=";
        case T_LE_OP: return "<=";
        case T_GE_OP: return ">=";
        case T_PTR_OP: return "->";
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '<': return "<";
        case '>': return ">";
        case '!': return "!";
        case '=': return "=";
        case '&': return "&";
        default:
            if (op >= 32 && op < 127) return std::string(1, (char)op);
            return "?";
    }
}

std::string astCalleeSummary(ASTNode* node) {
    if (!node) return "?";
    switch (node->kind) {
        case NodeKind::Identifier:
            return static_cast<IdentifierNode*>(node)->name;
        case NodeKind::UnaryOp: {
            auto* u = static_cast<UnaryOpNode*>(node);
            if (u->op == '*')
                return "*" + astCalleeSummary(u->operand.get());
            return astOpName(u->op) + astCalleeSummary(u->operand.get());
        }
        case NodeKind::MemberAccess: {
            auto* ma = static_cast<MemberAccessNode*>(node);
            return astCalleeSummary(ma->object.get())
                + (ma->throughPointer ? "->" : ".") + ma->member;
        }
        case NodeKind::ArraySubscript: {
            auto* s = static_cast<ArraySubscriptNode*>(node);
            std::string base = astCalleeSummary(s->array.get());
            if (s->index && s->index->kind == NodeKind::Integer)
                return base + "[" + std::to_string(static_cast<IntegerNode*>(s->index.get())->value) + "]";
            return base + "[...]";
        }
        default:
            return "<expr>";
    }
}
