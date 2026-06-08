/**
 * @file ast_dot.cpp
 * @brief AST Graphviz DOT 导出实现
 */

#include "ast_dot.h"
#include "ast_format.h"
#include <fstream>
#include <cstdlib>
#include <sstream>

std::string ASTDotExporter::escapeDot(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string ASTDotExporter::labelFor(ASTNode* node) {
    if (!node) return "(null)";
    std::ostringstream oss;
    switch (node->kind) {
        case NodeKind::Identifier:
            oss << "Identifier\\n" << escapeDot(static_cast<IdentifierNode*>(node)->name);
            break;
        case NodeKind::Integer:
            oss << "Integer\\n" << static_cast<IntegerNode*>(node)->value;
            break;
        case NodeKind::Float:
            oss << "Float\\n" << static_cast<FloatNode*>(node)->value;
            break;
        case NodeKind::String:
            oss << "String\\n\\\"" << escapeDot(static_cast<StringNode*>(node)->value) << "\\\"";
            break;
        case NodeKind::BinaryOp:
            oss << "BinaryOp\\n(" << escapeDot(astOpName(static_cast<BinaryOpNode*>(node)->op)) << ")";
            break;
        case NodeKind::UnaryOp:
            oss << "UnaryOp\\n(" << escapeDot(astOpName(static_cast<UnaryOpNode*>(node)->op)) << ")";
            break;
        case NodeKind::AssignOp:
            oss << "AssignOp";
            break;
        case NodeKind::Call: {
            auto* c = static_cast<CallNode*>(node);
            oss << "Call\\n" << escapeDot(astCalleeSummary(c->callee.get()));
            break;
        }
        case NodeKind::ArraySubscript:
            oss << "ArraySubscript";
            break;
        case NodeKind::MemberAccess: {
            auto* ma = static_cast<MemberAccessNode*>(node);
            oss << (ma->throughPointer ? "PtrMemberAccess\\n" : "MemberAccess\\n")
                << escapeDot(ma->member);
            break;
        }
        default:
            oss << astKindName(node->kind);
            if (auto* multi = dynamic_cast<MultiNode*>(node))
                oss << "\\n[" << multi->children.size() << " children]";
            if (node->line > 0)
                oss << "\\nline " << node->line;
            break;
    }
    return oss.str();
}

void ASTDotExporter::exportToFile(ASTNode* root, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) return;
    exportToStream(root, out);
}

void ASTDotExporter::exportToStream(ASTNode* root, std::ostream& out) {
    out << "digraph AST {\n";
    out << "  rankdir=TB;\n";
    out << "  node [shape=box, fontname=\"Microsoft YaHei\"];\n";
    out << "  edge [arrowsize=0.7];\n";
    if (!root) {
        out << "  empty [label=\"(empty AST)\"];\n";
    } else {
        nextId_ = 0;
        visit(root, out);
    }
    out << "}\n";
}

void ASTDotExporter::visit(ASTNode* node, std::ostream& out) {
    if (!node) return;
    visitNode(node, out);
}

int ASTDotExporter::visitNode(ASTNode* node, std::ostream& out) {
    int myId = nextId_++;
    out << "  n" << myId << " [label=\"" << labelFor(node) << "\"];\n";

    auto link = [&](ASTNode* child) {
        if (!child) return;
        int childId = visitNode(child, out);
        out << "  n" << myId << " -> n" << childId << ";\n";
    };
    auto linkLabeled = [&](ASTNode* child, const std::string& edgeLabel) {
        if (!child) return;
        int childId = visitNode(child, out);
        out << "  n" << myId << " -> n" << childId << " [label=\"" << edgeLabel << "\"];\n";
    };

    switch (node->kind) {
        case NodeKind::BinaryOp: {
            auto* b = static_cast<BinaryOpNode*>(node);
            link(b->left.get());
            link(b->right.get());
            break;
        }
        case NodeKind::UnaryOp:
            link(static_cast<UnaryOpNode*>(node)->operand.get());
            break;
        case NodeKind::AssignOp: {
            auto* a = static_cast<AssignOpNode*>(node);
            link(a->left.get());
            link(a->right.get());
            break;
        }
        case NodeKind::Call: {
            auto* c = static_cast<CallNode*>(node);
            linkLabeled(c->callee.get(), "callee");
            for (size_t i = 0; i < c->args.size(); ++i)
                linkLabeled(c->args[i].get(), "arg" + std::to_string(i));
            break;
        }
        case NodeKind::ArraySubscript: {
            auto* s = static_cast<ArraySubscriptNode*>(node);
            link(s->array.get());
            link(s->index.get());
            break;
        }
        case NodeKind::MemberAccess:
            link(static_cast<MemberAccessNode*>(node)->object.get());
            break;
        default:
            if (auto* multi = dynamic_cast<MultiNode*>(node)) {
                for (auto& ch : multi->children)
                    link(ch.get());
            }
            break;
    }
    return myId;
}

bool renderDotToPng(const std::string& dotFile, const std::string& pngFile) {
    std::string cmd = "dot -Tpng \"" + dotFile + "\" -o \"" + pngFile + "\"";
    return std::system(cmd.c_str()) == 0;
}
