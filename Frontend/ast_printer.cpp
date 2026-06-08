/**
 * @file ast_printer.cpp
 * @brief AST 文本树形打印器实现
 */

#include "ast_printer.h"
#include "ast_format.h"
#include <iostream>
#include <string>

static std::string indentStr(int n) {
    return std::string((size_t)n, ' ');
}

void ASTPrinter::printNode(ASTNode* node, int indent, std::ostream& out) {
    if (!node) return;

    out << indentStr(indent);

    switch (node->kind) {
        case NodeKind::Identifier:
            out << "Identifier: " << static_cast<IdentifierNode*>(node)->name;
            break;
        case NodeKind::Integer:
            out << "Integer: " << static_cast<IntegerNode*>(node)->value;
            break;
        case NodeKind::Float:
            out << "Float: " << static_cast<FloatNode*>(node)->value;
            break;
        case NodeKind::String:
            out << "String: \"" << static_cast<StringNode*>(node)->value << "\"";
            break;
        case NodeKind::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node);
            out << "BinaryOp(" << astOpName(b->op) << ")" << std::endl;
            printNode(b->left.get(), indent + 2, out);
            printNode(b->right.get(), indent + 2, out);
            return;
        }
        case NodeKind::UnaryOp: {
            auto* u = static_cast<UnaryOpNode*>(node);
            out << "UnaryOp(" << astOpName(u->op) << ")" << std::endl;
            printNode(u->operand.get(), indent + 2, out);
            return;
        }
        case NodeKind::AssignOp:
            out << "AssignOp" << std::endl;
            printNode(static_cast<AssignOpNode*>(node)->left.get(), indent + 2, out);
            printNode(static_cast<AssignOpNode*>(node)->right.get(), indent + 2, out);
            return;
        case NodeKind::Call: {
            auto* c = static_cast<CallNode*>(node);
            out << "Call(" << astCalleeSummary(c->callee.get()) << ")" << std::endl;
            printNode(c->callee.get(), indent + 2, out);
            for (auto& arg : c->args)
                printNode(arg.get(), indent + 2, out);
            return;
        }
        case NodeKind::ArraySubscript:
            out << "ArraySubscript" << std::endl;
            printNode(static_cast<ArraySubscriptNode*>(node)->array.get(), indent + 2, out);
            printNode(static_cast<ArraySubscriptNode*>(node)->index.get(), indent + 2, out);
            return;
        case NodeKind::MemberAccess: {
            auto* ma = static_cast<MemberAccessNode*>(node);
            out << (ma->throughPointer ? "PtrMemberAccess(" : "MemberAccess(")
                << ma->member << ")" << std::endl;
            printNode(ma->object.get(), indent + 2, out);
            return;
        }
        default:
            if (node->kind == NodeKind::CaseStmt) {
                out << "CaseStmt("
                    << static_cast<IntegerNode*>(static_cast<MultiNode*>(node)->children[0].get())->value
                    << ") [" << static_cast<MultiNode*>(node)->children.size() - 1 << " stmts]"
                    << std::endl;
                auto* multi = static_cast<MultiNode*>(node);
                for (size_t i = 1; i < multi->children.size(); ++i)
                    printNode(multi->children[i].get(), indent + 2, out);
                return;
            }
            out << astKindName(node->kind);
            if (auto* multi = dynamic_cast<MultiNode*>(node)) {
                out << " [" << multi->children.size() << " children]" << std::endl;
                for (auto& ch : multi->children)
                    printNode(ch.get(), indent + 2, out);
                return;
            }
            break;
    }
    out << std::endl;
}

void ASTPrinter::print(ASTNode* root, std::ostream& out) {
    if (!root) {
        out << "(empty AST)" << std::endl;
        return;
    }
    printNode(root, 0, out);
}
