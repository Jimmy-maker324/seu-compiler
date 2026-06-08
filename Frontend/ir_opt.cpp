/**
 * @file ir_opt.cpp
 * @brief IR 优化实现：常量传播/折叠、死代码消除、循环不变式外提
 */

#include "ir_opt.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace {

using ValMap = std::unordered_map<std::string, std::string>;

bool isPureComputeOp(const std::string& op) {
    return op == "+" || op == "-" || op == "*" || op == "/"
        || op == "==" || op == "!=" || op == "<" || op == "<="
        || op == ">" || op == ">=" || op == "&&" || op == "||"
        || op == "neg" || op == "not" || op == "copy";
}

bool hasSideEffect(const IRQuad& q) {
    return q.op == "func" || q.op == "param" || q.op == "call"
        || q.op == "return" || q.op == "goto" || q.op == "if"
        || q.op == "label" || q.op == "=" || q.op == "[]="
        || q.op == ".=" || q.op == "->=" || q.op == "*="
        || q.op == "[]" || q.op == "." || q.op == "->"
        || q.op == "&" || q.op == "&." || q.op == "&->"
        || q.op == "&[]";
}

bool isControlFlow(const IRQuad& q) {
    return q.op == "goto" || q.op == "if" || q.op == "label"
        || q.op == "return" || q.op == "func";
}

bool rhsInResultField(const std::string& op) {
    return op == "[]=" || op == ".=" || op == "->=" || op == "*=";
}

void collectUses(const IRQuad& q, std::unordered_set<std::string>& uses) {
    if (!q.arg1.empty() && !irIsConstant(q.arg1)) uses.insert(q.arg1);
    if (!q.arg2.empty() && !irIsConstant(q.arg2)) uses.insert(q.arg2);
    if (rhsInResultField(q.op) && !q.result.empty() && !irIsConstant(q.result))
        uses.insert(q.result);
    if (q.op == "if" && !q.result.empty() && !irIsConstant(q.result)) {
        if (q.result.compare(0, 5, "goto ") != 0)
            uses.insert(q.result);
    }
}

void killVar(ValMap& env, const std::string& v) {
    env.erase(v);
}

void killAllTemps(ValMap& env) {
    for (auto it = env.begin(); it != env.end(); ) {
        if (irIsTemp(it->first)) it = env.erase(it);
        else ++it;
    }
}

void applyDefKill(ValMap& env, const IRQuad& q) {
    if (q.op == "call") {
        killAllTemps(env);
        return;
    }
    if (q.op == "=" || q.op == "[]=" || q.op == ".=" || q.op == "->="
        || q.op == "*=") {
        if (!q.result.empty()) killVar(env, q.result);
        if (q.op == ".=" || q.op == "->=" || q.op == "[]=")
            killAllTemps(env);
        return;
    }
    if (q.op == "str") {
        if (!q.result.empty()) killVar(env, q.result);
        return;
    }
    if (isPureComputeOp(q.op) || q.op == "[]"
        || q.op == "." || q.op == "->" || q.op == "&"
        || q.op == "&." || q.op == "&->" || q.op == "&[]") {
        if (!q.result.empty()) killVar(env, q.result);
    }
}

std::string substitute(const std::string& s, const ValMap& env) {
    if (s.empty()) return s;
    auto it = env.find(s);
    return it != env.end() ? it->second : s;
}

bool toNumber(const std::string& s, double& out) {
    if (!irIsConstant(s)) return false;
    out = std::atof(s.c_str());
    return true;
}

std::string formatNumber(double v, bool preferInt) {
    if (preferInt && std::fabs(v - std::round(v)) < 1e-9)
        return std::to_string((long long)std::round(v));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

bool evalUnary(const std::string& op, const std::string& arg, std::string& out) {
    double x;
    if (!toNumber(arg, x)) return false;
    if (op == "neg") {
        out = formatNumber(-x, irIsIntegerLiteral(arg));
        return true;
    }
    if (op == "not") {
        out = (x == 0.0) ? "1" : "0";
        return true;
    }
    return false;
}

bool evalBinary(const std::string& op, const std::string& a1,
                const std::string& a2, std::string& out) {
    double x, y;
    if (!toNumber(a1, x) || !toNumber(a2, y)) return false;
    bool preferInt = irIsIntegerLiteral(a1) && irIsIntegerLiteral(a2);
    double r = 0;
    if (op == "+") r = x + y;
    else if (op == "-") r = x - y;
    else if (op == "*") r = x * y;
    else if (op == "/") {
        if (y == 0.0) return false;
        r = x / y;
        preferInt = false;
    } else if (op == "==") { out = (x == y) ? "1" : "0"; return true; }
    else if (op == "!=") { out = (x != y) ? "1" : "0"; return true; }
    else if (op == "<") { out = (x < y) ? "1" : "0"; return true; }
    else if (op == "<=") { out = (x <= y) ? "1" : "0"; return true; }
    else if (op == ">") { out = (x > y) ? "1" : "0"; return true; }
    else if (op == ">=") { out = (x >= y) ? "1" : "0"; return true; }
    else if (op == "&&") { out = (x != 0.0 && y != 0.0) ? "1" : "0"; return true; }
    else if (op == "||") { out = (x != 0.0 || y != 0.0) ? "1" : "0"; return true; }
    else return false;
    out = formatNumber(r, preferInt);
    return true;
}

struct BasicBlock {
    size_t start;
    size_t end;
};

std::vector<BasicBlock> splitBasicBlocks(const std::vector<IRQuad>& code) {
    std::vector<BasicBlock> blocks;
    if (code.empty()) return blocks;
    size_t start = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        if (isControlFlow(code[i])) {
            blocks.push_back({start, i});
            start = i + 1;
        }
    }
    if (start < code.size())
        blocks.push_back({start, code.size() - 1});
    return blocks;
}

size_t labelIndex(const std::vector<IRQuad>& code, const std::string& label) {
    for (size_t i = 0; i < code.size(); ++i)
        if (code[i].op == "label" && code[i].arg1 == label)
            return i;
    return code.size();
}

struct LoopRegion {
    size_t headerLabel;
    size_t bodyStart;
    size_t bodyEnd;
};

std::vector<LoopRegion> findLoops(const std::vector<IRQuad>& code) {
    std::vector<LoopRegion> loops;
    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i].op != "goto") continue;
        size_t h = labelIndex(code, code[i].result);
        if (h < i) {
            loops.push_back({h, h + 1, i});
        }
    }
    return loops;
}

std::unordered_set<std::string> defsInRange(const std::vector<IRQuad>& code,
                                            size_t start, size_t end) {
    std::unordered_set<std::string> defs;
    for (size_t i = start; i <= end && i < code.size(); ++i) {
        const IRQuad& q = code[i];
        if (q.op == "label") continue;
            if (!q.result.empty()
                && (q.op == "=" || q.op == "str" || isPureComputeOp(q.op)
                || q.op == "[]" || q.op == "." || q.op == "->"
                || q.op == "call" || q.op == "&" || q.op == "&."
                || q.op == "&->" || q.op == "&[]")) {
            defs.insert(q.result);
        }
    }
    return defs;
}

bool operandInvariant(const std::string& opnd,
                      const std::unordered_set<std::string>& loopDefs,
                      const std::unordered_set<std::string>& hoistedTemps) {
    if (opnd.empty() || irIsConstant(opnd)) return true;
    if (hoistedTemps.count(opnd)) return true;
    return loopDefs.find(opnd) == loopDefs.end();
}

bool quadIsHoistableInvariant(const IRQuad& q,
                              const std::unordered_set<std::string>& loopDefs,
                              const std::unordered_set<std::string>& hoistedTemps) {
    if (!isPureComputeOp(q.op)) return false;
    if (q.result.empty() || !irIsTemp(q.result)) return false;
    return operandInvariant(q.arg1, loopDefs, hoistedTemps)
        && operandInvariant(q.arg2, loopDefs, hoistedTemps);
}

std::string resolveRepl(const std::string& s,
                        const std::unordered_map<std::string, std::string>& repl) {
    auto it = repl.find(s);
    return it != repl.end() ? it->second : s;
}

std::string exprKey(const IRQuad& q) {
    return q.op + '\x01' + q.arg1 + '\x01' + q.arg2;
}

void invalidateExprsUsing(std::unordered_map<std::string, std::string>& expr2res,
                          const std::string& var) {
    if (var.empty()) return;
    for (auto it = expr2res.begin(); it != expr2res.end(); ) {
        size_t p1 = it->first.find('\x01');
        if (p1 == std::string::npos) { ++it; continue; }
        size_t p2 = it->first.find('\x01', p1 + 1);
        std::string a1 = it->first.substr(p1 + 1, p2 - p1 - 1);
        std::string a2 = (p2 != std::string::npos) ? it->first.substr(p2 + 1) : "";
        if (a1 == var || a2 == var)
            it = expr2res.erase(it);
        else
            ++it;
    }
}

} // namespace

IROptStats IROptimizer::run(std::vector<IRQuad>& code, int maxRound) {
    IROptStats total;
    for (int r = 0; r < maxRound; ++r) {
        bool changed = false;
        if (constantPropagationAndFolding(code, total)) changed = true;
        if (commonSubexpressionElimination(code, total)) changed = true;
        if (deadCodeElimination(code, total)) changed = true;
        if (loopInvariantCodeMotion(code, total)) changed = true;
        if (!changed) break;
    }
    return total;
}

bool IROptimizer::constantPropagationAndFolding(std::vector<IRQuad>& code,
                                                IROptStats& stats) {
    int foldBefore = stats.constFold;
    int propBefore = stats.constProp;
    auto blocks = splitBasicBlocks(code);

    for (const BasicBlock& bb : blocks) {
        ValMap env;
        for (size_t i = bb.start; i <= bb.end && i < code.size(); ++i) {
            IRQuad& q = code[i];
            if (q.op == "label") {
                env.clear();
                continue;
            }

            std::string na1 = substitute(q.arg1, env);
            std::string na2 = substitute(q.arg2, env);
            if (na1 != q.arg1 || na2 != q.arg2) {
                q.arg1 = na1;
                q.arg2 = na2;
                ++stats.constProp;
            }

            if (q.op == "=" && irIsConstant(q.arg1) && !q.result.empty()) {
                env[q.result] = q.arg1;
                continue;
            }

            if (isPureComputeOp(q.op)) {
                std::string folded;
                if (q.arg2.empty()) {
                    if (evalUnary(q.op, q.arg1, folded)) {
                        q.op = "=";
                        q.arg2.clear();
                        q.arg1 = folded;
                        env[q.result] = folded;
                        ++stats.constFold;
                        continue;
                    }
                } else if (evalBinary(q.op, q.arg1, q.arg2, folded)) {
                    q.op = "=";
                    q.arg2.clear();
                    q.arg1 = folded;
                    env[q.result] = folded;
                    ++stats.constFold;
                    continue;
                }
            }

            if (q.op == "copy" && irIsConstant(q.arg1)) {
                q.op = "=";
                env[q.result] = q.arg1;
                ++stats.constFold;
                continue;
            }

            applyDefKill(env, q);
        }
    }
    return stats.constFold > foldBefore || stats.constProp > propBefore;
}

bool IROptimizer::commonSubexpressionElimination(std::vector<IRQuad>& code,
                                                 IROptStats& stats) {
    int before = stats.cseElim;
    auto blocks = splitBasicBlocks(code);
    std::unordered_map<std::string, std::string> repl;
    std::vector<bool> remove(code.size(), false);

    for (const BasicBlock& bb : blocks) {
        std::unordered_map<std::string, std::string> expr2res;
        for (size_t i = bb.start; i <= bb.end && i < code.size(); ++i) {
            IRQuad& q = code[i];
            if (q.op == "label") {
                expr2res.clear();
                continue;
            }

            q.arg1 = resolveRepl(q.arg1, repl);
            q.arg2 = resolveRepl(q.arg2, repl);

            if (isPureComputeOp(q.op) && irIsTemp(q.result)) {
                std::string key = exprKey(q);
                auto it = expr2res.find(key);
                if (it != expr2res.end()) {
                    repl[q.result] = it->second;
                    remove[i] = true;
                    ++stats.cseElim;
                } else {
                    expr2res[key] = q.result;
                }
            } else if (q.op == "=") {
                invalidateExprsUsing(expr2res, q.result);
                if (!irIsConstant(q.arg1))
                    invalidateExprsUsing(expr2res, q.arg1);
            } else if (hasSideEffect(q)) {
                expr2res.clear();
            }
        }
    }

    for (IRQuad& q : code) {
        q.arg1 = resolveRepl(q.arg1, repl);
        q.arg2 = resolveRepl(q.arg2, repl);
        if (rhsInResultField(q.op))
            q.result = resolveRepl(q.result, repl);
    }

    std::vector<IRQuad> out;
    out.reserve(code.size());
    for (size_t i = 0; i < code.size(); ++i) {
        if (!remove[i]) out.push_back(code[i]);
    }
    code.swap(out);
    return stats.cseElim > before;
}

bool IROptimizer::deadCodeElimination(std::vector<IRQuad>& code,
                                      IROptStats& stats) {
    std::unordered_set<std::string> live;
    std::vector<bool> keep(code.size(), false);

    for (size_t i = code.size(); i-- > 0; ) {
        const IRQuad& q = code[i];
        if (q.op == "label") {
            keep[i] = true;
            continue;
        }
        if (hasSideEffect(q) || isControlFlow(q)) {
            keep[i] = true;
            collectUses(q, live);
            continue;
        }
        if (isPureComputeOp(q.op)) {
            if (!q.result.empty() && live.count(q.result)) {
                keep[i] = true;
                collectUses(q, live);
            }
            continue;
        }
        keep[i] = true;
        collectUses(q, live);
    }

    std::vector<IRQuad> out;
    out.reserve(code.size());
    int removed = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        if (keep[i]) out.push_back(code[i]);
        else ++removed;
    }
    code.swap(out);
    stats.deadRemoved += removed;
    return removed > 0;
}

bool IROptimizer::loopInvariantCodeMotion(std::vector<IRQuad>& code,
                                          IROptStats& stats) {
    auto loops = findLoops(code);
    if (loops.empty()) return false;

    std::sort(loops.begin(), loops.end(),
        [](const LoopRegion& a, const LoopRegion& b) {
            return (a.bodyEnd - a.bodyStart) < (b.bodyEnd - b.bodyStart);
        });

    int hoistedBefore = stats.hoisted;
    for (const LoopRegion& loop : loops) {
        if (loop.bodyEnd <= loop.bodyStart) continue;
        auto loopDefs = defsInRange(code, loop.bodyStart, loop.bodyEnd);

        std::vector<IRQuad> toHoist;
        std::vector<size_t> hoistIdx;
        std::unordered_set<std::string> hoistedTemps;

        for (size_t i = loop.bodyStart; i <= loop.bodyEnd && i < code.size(); ++i) {
            if (code[i].op == "label") continue;
            if (!quadIsHoistableInvariant(code[i], loopDefs, hoistedTemps))
                continue;
            toHoist.push_back(code[i]);
            hoistedTemps.insert(code[i].result);
            hoistIdx.push_back(i);
        }
        if (toHoist.empty()) continue;

        std::unordered_set<size_t> eraseSet(hoistIdx.begin(), hoistIdx.end());
        std::vector<IRQuad> rebuilt;
        rebuilt.reserve(code.size());
        for (size_t i = 0; i < code.size(); ++i) {
            if (i == loop.headerLabel) {
                for (const IRQuad& hq : toHoist)
                    rebuilt.push_back(hq);
            }
            if (eraseSet.count(i)) continue;
            rebuilt.push_back(code[i]);
        }
        code.swap(rebuilt);
        stats.hoisted += (int)toHoist.size();
    }
    return stats.hoisted > hoistedBefore;
}
