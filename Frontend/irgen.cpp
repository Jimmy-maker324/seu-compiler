/**
 * @file irgen.cpp
 * @brief 中间代码（四元式）生成器实现
 *
 * 【IR 形式】Quad { op, arg1, arg2, result }，顺序存入 vector<code>
 *
 * 【表达式翻译（语法制导）】
 *   后序遍历：先 visit 子表达式得变量名/常量，再 emit(op, a1, a2, tN)
 *   每个运算结果存入新临时变量 t0,t1,...
 *
 * 【控制流翻译】
 *   比较条件为真跳转：emit(relop, a, b, L)（relop 成立则转到 L）
 *   为假则顺序执行下一行（如循环出口 goto）；其它条件 emit(!=, cond, 0, L)
 *   无条件跳转：emit(goto, "", "", L)
 *
 * 【赋值】标识符直接 emit(=, rhs, "", lhs)；数组 emit([]=, arr, idx, rhs)
 *
 * 【变量命名】符号表 Symbol::irName：与语义分析共用 enterScope/leaveScope/addSymbol
 * 【字符串】字面量生成 (str, "...", , strN)，类型为 char*
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#include "irgen.h"
#include "symbol.h"
#include "common_defs.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

/** @brief 将字符串内容编码为 IR 字面量（带引号与转义） */
std::string irQuoteString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
        }
    }
    out += "\"";
    return out;
}

} // namespace

/** @brief 初始化输出路径与计数器 */
IRGenerator::IRGenerator(const std::string& outFile)
    : outFile(outFile), tempCounter(0), labelCounter(0), stringCounter_(0) {}

/** @brief 生成唯一临时变量名 t0, t1, t2, ... */
std::string IRGenerator::newTemp() {
    return "t" + std::to_string(tempCounter++);
}

/** @brief 生成唯一标签名 L0, L1, L2, ... */
std::string IRGenerator::newLabel() {
    return "L" + std::to_string(labelCounter++);
}

/** @brief 生成带当前函数名的标签说明 */
std::string IRGenerator::labelContext(const std::string& meaning) const {
    if (currentFunc.empty()) return meaning;
    return currentFunc + " — " + meaning;
}

/** @brief 登记标签语义并输出 label 四元式 */
void IRGenerator::emitLabel(const std::string& label, const std::string& desc) {
    labelDesc[label] = desc;
    code.push_back({"label", label, "", ""});
}

/** @brief 向 code 向量追加一条四元式 */
void IRGenerator::emit(const std::string& op, const std::string& a1,
                       const std::string& a2, const std::string& r) {
    code.push_back({op, a1, a2, r});
}

/** @brief 从 goto / 比较条件跳转四元式中提取目标标签名 */
static std::string jumpTarget(const IRQuad& q) {
    if (q.op == "goto" && !q.result.empty())
        return q.result;
    if (irIsCondJump(q))
        return q.result;
    return "";
}

/** @brief 标签名 → 目标四元式行号（字符串）；未知则原样返回 */
static std::string jumpLineOf(const std::string& label,
                              const std::unordered_map<std::string, size_t>& labelLine) {
    auto it = labelLine.find(label);
    if (it != labelLine.end())
        return std::to_string(it->second);
    return label;
}

/** @brief 将四元式序列按序号写入文件，标签前缀在该基本块首条四元式行 */
void IRGenerator::dumpTo(const std::string& path) {
    std::string saved = outFile;
    outFile = path;
    dump();
    outFile = saved;
}

void IRGenerator::dump() {
    std::ofstream out(outFile);
    if (!out) { std::cerr << "Cannot open " << outFile << std::endl; return; }

    out << ";; IR quads: " << code.size() << "\n\n";

    std::unordered_map<std::string, size_t> labelLine;
    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i].op != "label") continue;
        for (size_t j = i + 1; j < code.size(); ++j) {
            if (code[j].op != "label") {
                labelLine[code[i].arg1] = j;
                break;
            }
        }
    }

    std::string pendingLabel;

    for (size_t i = 0; i < code.size(); ++i) {
        const IRQuad& q = code[i];

        if (q.op == "label") {
            pendingLabel = q.arg1;
            continue;
        }

        out << i << ": ";
        out << "(" << q.op << ", " << q.arg1 << ", " << q.arg2 << ", ";

        std::string target = jumpTarget(q);
        if (!target.empty())
            out << jumpLineOf(target, labelLine);
        else
            out << q.result;
        out << ")";

        if (!pendingLabel.empty()) {
            auto it = labelDesc.find(pendingLabel);
            if (it != labelDesc.end())
                out << "    ; " << it->second;
            pendingLabel.clear();
        } else if (!target.empty()) {
            auto it = labelDesc.find(target);
            if (it != labelDesc.end())
                out << "    ; " << it->second;
        }
        out << "\n";
    }
    std::cout << "IR dumped to " << outFile << std::endl;
}

/** @brief 表达式访问分发 */
std::string IRGenerator::visit(ASTNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NodeKind::BinaryOp: return visitBinaryOp((BinaryOpNode*)node);
        case NodeKind::UnaryOp:  return visitUnaryOp((UnaryOpNode*)node);
        case NodeKind::AssignOp: return visitAssignOp((AssignOpNode*)node);
        case NodeKind::Identifier: return visitIdentifier((IdentifierNode*)node);
        case NodeKind::Integer:  return visitInteger((IntegerNode*)node);
        case NodeKind::Float:    return visitFloat((FloatNode*)node);
        case NodeKind::String:   return visitString((StringNode*)node);
        case NodeKind::Call:     return visitCall((CallNode*)node);
        case NodeKind::ArraySubscript: {
            auto* sub = (ArraySubscriptNode*)node;
            std::string arr = visit(sub->array.get());
            std::string idx = visit(sub->index.get());
            std::string tmp = newTemp();
            emit("[]", arr, idx, tmp);  // 数组/指针取值
            return tmp;
        }
        case NodeKind::MemberAccess: {
            auto* ma = (MemberAccessNode*)node;
            std::string obj = visit(ma->object.get());
            std::string tmp = newTemp();
            if (ma->throughPointer)
                emit("->", obj, ma->member, tmp);
            else
                emit(".", obj, ma->member, tmp);
            return tmp;
        }
        default: return "";
    }
}

/** @brief 二元比较运算符 → IR 操作符 */
std::string IRGenerator::relOpFromBinary(int tokenOp) {
    switch (tokenOp) {
        case '<': return "<";
        case T_LE_OP: return "<=";
        case '>': return ">";
        case T_GE_OP: return ">=";
        case T_EQ_OP: return "==";
        case T_NE_OP: return "!=";
        default: return "";
    }
}

/** @brief 条件为真时跳转；比较 emit(relop, a, b, L)，否则 emit(!=, val, 0, L) */
void IRGenerator::emitTrueCondJump(ASTNode* cond, const std::string& trueLabel) {
    if (!cond) {
        emit("goto", "", "", trueLabel);
        return;
    }
    if (cond->kind == NodeKind::BinaryOp) {
        auto* bin = static_cast<BinaryOpNode*>(cond);
        std::string op = relOpFromBinary(bin->op);
        if (!op.empty()) {
            std::string left = visit(bin->left.get());
            std::string right = visit(bin->right.get());
            emit(op, left, right, trueLabel);
            return;
        }
    }
    std::string val = visit(cond);
    emit("!=", val, "0", trueLabel);
}

/** @brief 先递归求值左右操作数，再 emit 运算四元式 */
std::string IRGenerator::visitBinaryOp(BinaryOpNode* bin) {
    std::string left = visit(bin->left.get());
    std::string right = visit(bin->right.get());
    std::string tmp = newTemp();
    std::string op = relOpFromBinary(bin->op);
    if (op.empty()) {
        switch (bin->op) {
            case '+': op = "+"; break;
            case '-': op = "-"; break;
            case '*': op = "*"; break;
            case '/': op = "/"; break;
            case T_AND_OP: op = "&&"; break;
            case T_OR_OP: op = "||"; break;
            default: op = "?"; break;
        }
    }
    emit(op, left, right, tmp);
    return tmp;
}

/** @brief 一元负号、逻辑非或普通拷贝 */
std::string IRGenerator::visitUnaryOp(UnaryOpNode* un) {
    if (un->op == '&') {
        ASTNode* op = un->operand.get();
        std::string tmp = newTemp();
        if (op->kind == NodeKind::Identifier) {
            emit("&", visitIdentifier((IdentifierNode*)op), "", tmp);
            return tmp;
        }
        if (op->kind == NodeKind::MemberAccess) {
            auto* ma = (MemberAccessNode*)op;
            std::string obj = visit(ma->object.get());
            if (ma->throughPointer)
                emit("&->", obj, ma->member, tmp);
            else
                emit("&.", obj, ma->member, tmp);
            return tmp;
        }
        if (op->kind == NodeKind::ArraySubscript) {
            auto* sub = (ArraySubscriptNode*)op;
            std::string arr = visit(sub->array.get());
            std::string idx = visit(sub->index.get());
            emit("&[]", arr, idx, tmp);
            return tmp;
        }
        visit(op);
        emit("&", "?", "", tmp);
        return tmp;
    }
    if (un->op == '*') {
        std::string operand = visit(un->operand.get());
        std::string tmp = newTemp();
        emit("*", operand, "", tmp);
        return tmp;
    }
    std::string operand = visit(un->operand.get());
    std::string tmp = newTemp();
    if (un->op == '-') emit("neg", operand, "", tmp);
    else if (un->op == '!') emit("not", operand, "", tmp);
    else emit("copy", operand, "", tmp);
    return tmp;
}

/** @brief 简单变量赋值或数组元素赋值（[]=） */
std::string IRGenerator::visitAssignOp(AssignOpNode* assign) {
    std::string rhs = visit(assign->right.get());
    if (assign->left->kind == NodeKind::Identifier) {
        std::string lhs = visitIdentifier((IdentifierNode*)assign->left.get());
        emit("=", rhs, "", lhs);
        return lhs;
    }
    if (assign->left->kind == NodeKind::ArraySubscript) {
        auto* sub = (ArraySubscriptNode*)assign->left.get();
        std::string arr = visit(sub->array.get());
        std::string idx = visit(sub->index.get());
        emit("[]=", arr, idx, rhs);  // 数组元素写入
        return rhs;
    }
    if (assign->left->kind == NodeKind::MemberAccess) {
        auto* ma = (MemberAccessNode*)assign->left.get();
        std::string obj = visit(ma->object.get());
        if (ma->throughPointer)
            emit("->=", obj, ma->member, rhs);
        else
            emit(".=", obj, ma->member, rhs);
        return rhs;
    }
    if (assign->left->kind == NodeKind::UnaryOp) {
        auto* un = (UnaryOpNode*)assign->left.get();
        if (un->op == '*') {
            std::string ptr = visit(un->operand.get());
            emit("*=", ptr, "", rhs);
            return rhs;
        }
    }
    std::string lhs = visit(assign->left.get());
    emit("=", rhs, "", lhs);
    return lhs;
}

/** @brief 解析标识符为符号表中的 IR 名 */
std::string IRGenerator::visitIdentifier(IdentifierNode* id) {
    Symbol* sym = getSymbol(id->name);
    return sym ? sym->irName : id->name;
}

/** @brief 整型常量直接作为操作数字符串 */
std::string IRGenerator::visitInteger(IntegerNode* num) {
    return std::to_string(num->value);
}

/** @brief 浮点常量直接作为操作数字符串 */
std::string IRGenerator::visitFloat(FloatNode* num) {
    std::ostringstream oss;
    oss << num->value;
    return oss.str();
}

/** @brief 字符串常量：(str, "...", , strN)，结果为 char* 符号 */
std::string IRGenerator::visitString(StringNode* str) {
    std::string sym = "str" + std::to_string(stringCounter_++);
    emit("str", irQuoteString(str->value), "", sym);
    return sym;
}

/** @brief 先求值各实参并 emit param，再 emit call */
std::string IRGenerator::visitCall(CallNode* call) {
    for (auto& arg : call->args) {
        std::string val = visit(arg.get());
        emit("param", val, "", "");
    }
    std::string fn = visit(call->callee.get());
    std::string tmp = newTemp();
    emit("call", fn, std::to_string(call->args.size()), tmp);
    return tmp;
}

/** @brief 语句访问：复合块、表达式语句、if/while、return、函数定义 */
void IRGenerator::visitStmt(ASTNode* stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case NodeKind::CompoundStmt: {
            auto* block = (MultiNode*)stmt;
            if (skipCompoundScope_) {
                skipCompoundScope_ = false;
                for (auto& child : block->children)
                    visitStmt(child.get());
            } else {
                enterScope();
                for (auto& child : block->children)
                    visitStmt(child.get());
                leaveScope();
            }
            break;
        }
        case NodeKind::ExprStmt: {
            auto* exprStmt = (MultiNode*)stmt;
            if (!exprStmt->children.empty()) visit(exprStmt->children[0].get());
            break;
        }
        case NodeKind::VarDecl: {
            auto* decl = (MultiNode*)stmt;
            if (decl->children.size() >= 2) {
                auto* tn = (TypeNode*)decl->children[0].get();
                auto* id = (IdentifierNode*)decl->children[1].get();
                Symbol* sym = addSymbol(id->name, tn->type, false);
                if (decl->children.size() >= 3) {
                    std::string val = visit(decl->children[2].get());
                    emit("=", val, "", sym->irName);
                }
            }
            break;
        }
        case NodeKind::IfStmt: {
            auto* ifNode = (MultiNode*)stmt;
            std::string labelThen = newLabel();
            std::string labelElse = newLabel();
            std::string labelEnd = newLabel();
            bool hasElse = ifNode->children.size() >= 3;
            emitTrueCondJump(ifNode->children[0].get(), labelThen);
            emit("goto", "", "", hasElse ? labelElse : labelEnd);
            emitLabel(labelThen, labelContext("if then 分支"));
            if (ifNode->children.size() >= 2) visitStmt(ifNode->children[1].get());
            emit("goto", "", "", labelEnd);
            if (hasElse) {
                emitLabel(labelElse, labelContext("if else 分支"));
                visitStmt(ifNode->children[2].get());
            }
            emitLabel(labelEnd, labelContext("if 结束"));
            break;
        }
        case NodeKind::WhileStmt: {
            auto* whileNode = (MultiNode*)stmt;
            std::string labelStart = newLabel();
            std::string labelBody = newLabel();
            std::string labelEnd = newLabel();
            emitLabel(labelStart, labelContext("while 循环条件"));
            emitTrueCondJump(whileNode->children[0].get(), labelBody);
            emit("goto", "", "", labelEnd);
            emitLabel(labelBody, labelContext("while 循环体"));
            breakTargetStack.push_back(labelEnd);
            continueTargetStack.push_back(labelStart);
            if (whileNode->children.size() >= 2) visitStmt(whileNode->children[1].get());
            breakTargetStack.pop_back();
            continueTargetStack.pop_back();
            emit("goto", "", "", labelStart);
            emitLabel(labelEnd, labelContext("while 循环结束"));
            break;
        }
        case NodeKind::ForStmt: {
            auto* forNode = (MultiNode*)stmt;
            std::string labelStart = newLabel();
            std::string labelBody = newLabel();
            std::string labelContinue = newLabel();
            std::string labelEnd = newLabel();
            if (forNode->children.size() >= 1 && forNode->children[0])
                visitStmt(forNode->children[0].get());
            emitLabel(labelStart, labelContext("for 循环条件"));
            bool hasCond = forNode->children.size() >= 2 && forNode->children[1];
            if (hasCond) {
                emitTrueCondJump(forNode->children[1].get(), labelBody);
                emit("goto", "", "", labelEnd);
                emitLabel(labelBody, labelContext("for 循环体"));
            }
            breakTargetStack.push_back(labelEnd);
            continueTargetStack.push_back(labelContinue);
            if (forNode->children.size() >= 4 && forNode->children[3])
                visitStmt(forNode->children[3].get());
            breakTargetStack.pop_back();
            continueTargetStack.pop_back();
            emitLabel(labelContinue, labelContext("for 循环步进"));
            if (forNode->children.size() >= 3 && forNode->children[2])
                visitStmt(forNode->children[2].get());
            emit("goto", "", "", labelStart);
            emitLabel(labelEnd, labelContext("for 循环结束"));
            break;
        }
        case NodeKind::SwitchStmt: {
            auto* sw = (MultiNode*)stmt;
            std::string swVal = visit(sw->children[0].get());
            std::string labelEnd = newLabel();

            struct ClauseInfo {
                bool isDefault;
                int value;
                std::string bodyLabel;
                MultiNode* clause;
            };
            std::vector<ClauseInfo> clauses;
            std::string defaultLabel;
            bool hasDefault = false;

            for (size_t i = 1; i < sw->children.size(); ++i) {
                auto* clause = static_cast<MultiNode*>(sw->children[i].get());
                ClauseInfo info;
                info.clause = clause;
                info.bodyLabel = newLabel();
                if (clause->kind == NodeKind::DefaultStmt) {
                    info.isDefault = true;
                    defaultLabel = info.bodyLabel;
                    hasDefault = true;
                } else {
                    info.isDefault = false;
                    info.value = static_cast<IntegerNode*>(clause->children[0].get())->value;
                }
                clauses.push_back(info);
            }

            for (const ClauseInfo& info : clauses) {
                if (info.isDefault) continue;
                std::string nextTest = newLabel();
                emit("==", swVal, std::to_string(info.value), info.bodyLabel);
                emit("goto", "", "", nextTest);
                emitLabel(nextTest, labelContext("switch 下一分支测试"));
            }
            if (hasDefault) {
                emit("goto", "", "", defaultLabel);
            } else {
                emit("goto", "", "", labelEnd);
            }

            breakTargetStack.push_back(labelEnd);
            for (const ClauseInfo& info : clauses) {
                emitLabel(info.bodyLabel, info.isDefault
                    ? labelContext("switch default")
                    : labelContext("switch case " + std::to_string(info.value)));
                size_t start = info.isDefault ? 0 : 1;
                for (size_t j = start; j < info.clause->children.size(); ++j)
                    visitStmt(info.clause->children[j].get());
            }
            breakTargetStack.pop_back();
            emitLabel(labelEnd, labelContext("switch 结束"));
            break;
        }
        case NodeKind::BreakStmt:
            if (!breakTargetStack.empty())
                emit("goto", "", "", breakTargetStack.back());
            break;
        case NodeKind::ContinueStmt:
            if (!continueTargetStack.empty())
                emit("goto", "", "", continueTargetStack.back());
            break;
        case NodeKind::ReturnStmt: {
            auto* retNode = (MultiNode*)stmt;
            if (!retNode->children.empty()) {
                std::string val = visit(retNode->children[0].get());
                emit("return", val, "", "");
            } else {
                emit("return", "", "", "");
            }
            break;
        }
        case NodeKind::FuncDef: {
            auto* func = (MultiNode*)stmt;
            std::string savedFunc = currentFunc;
            if (func->children.size() >= 2) {
                auto* id = (IdentifierNode*)func->children[1].get();
                auto* retType = (TypeNode*)func->children[0].get();
                addSymbol(id->name, retType->type, true);
                currentFunc = id->name;
                code.push_back({"func", id->name, "", ""});
            }
            enterScope();
            size_t bodyIdx = 2;
            if (func->children.size() >= 4 &&
                func->children[2]->kind == NodeKind::ParamList) {
                auto* paramList = (MultiNode*)func->children[2].get();
                for (auto& p : paramList->children) {
                    auto* paramDecl = (MultiNode*)p.get();
                    if (paramDecl->children.size() >= 2) {
                        auto* tn = (TypeNode*)paramDecl->children[0].get();
                        auto* pid = (IdentifierNode*)paramDecl->children[1].get();
                        Symbol* sym = addSymbol(pid->name, tn->type, false);
                        emit("param", "", "", sym->irName);
                    }
                }
                bodyIdx = 3;
            }
            if (bodyIdx < func->children.size()) {
                skipCompoundScope_ = true;
                visitStmt(func->children[bodyIdx].get());
            }
            leaveScope();
            currentFunc = savedFunc;
            break;
        }
        default:
            break;
    }
}

/** @brief 遍历 Program 下所有顶层声明/定义生成 IR */
void IRGenerator::generate(ASTNode* root) {
    if (!root || root->kind != NodeKind::Program) return;
    resetSymbolTable();
    stringCounter_ = 0;
    skipCompoundScope_ = false;
    enterScope();
    auto* prog = (MultiNode*)root;
    for (auto& decl : prog->children) {
        visitStmt(decl.get());
    }
    leaveScope();
}
