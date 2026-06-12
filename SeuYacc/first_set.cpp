/*
 * ============================================================================
 * first_set.cpp — First 集计算（SeuYacc 第 2 阶段）
 * ============================================================================
 *
 * 【算法名称】迭代不动点法（Iterative Fixed-Point）计算 First 集
 *
 * 【数据结构】
 *   map<int, set<int>> first_set
 *     - 键：符号 ID（终结符 0..999，非终结符 >= NON_TERM_BASE）
 *     - 值：该符号的 First 集合；特殊元素 EPSILON(-1) 表示可推导出 ε
 *
 * 【算法设计】
 *   First(α) 定义：从 α 能推导出的所有终结符集合；若 α=>*ε 则含 ε。
 *   对产生式 A -> X1 X2 ... Xn：
 *     - 将 First(X1)\{ε} 并入 First(A)
 *     - 若 X1..Xi 均可空，则继续并入 First(Xi+1)\{ε}
 *     - 若全部 Xi 可空，则 First(A) 含 ε
 *
 * 【compute_first_sets 流程】
 *   1. 初始化：First(t)={t}（终结符）；First(A)=∅（非终结符）
 *   2. repeat
 *        对每条产生式 A->β 按上述规则尝试向 First(A) 插入符号
 *        若有新元素加入则 changed=true
 *      until !changed
 *
 * 【compute_first_of_string 流程】
 *   对符号串 β = Y1..Yk 模拟“从左到右读入”：
 *   1. result = ∅
 *   2. 对每个 Yi：并入 First(Yi)\{ε}；若 Yi 无 ε 则停止
 *   3. 若全部 Yi 含 ε，result 加入 ε
 *   用途：LR(1) closure 中计算 First(βa)，决定向前看符号集合
 *
 * 【复杂度】O(|P| * |N| * |Σ|) 量级，|P| 为产生式数，迭代轮数有界
 * ============================================================================
 */
#include "yacc_common.h"

#define EPSILON -1   /* ε 在 First 集中的占位 ID，非真实终结符 */

/**
 * 计算文法中所有终结符与非终结符的 First 集
 * 终结符：First(t) = {t}；空产生式右部为空时加入 ε
 */
void compute_first_sets() {
    DEBUG_PRINT("Start computing First sets...");
    first_set.clear();

    /* 终结符 First 集为自身 */
    for (auto& pair : token_map) {
        int term = pair.second;
        first_set[term].insert(term);
    }
    /* 非终结符初始为空 */
    for (auto& pair : non_term_map) {
        int nonterm = pair.second;
        first_set[nonterm] = set<int>();
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < productions.size(); i++) {
            Production& prod = productions[i];
            int A = prod.left;
            vector<int>& rhs = prod.right;

            /* A -> ε */
            if (rhs.empty()) {
                if (first_set[A].insert(EPSILON).second) {
                    changed = true;
                    DEBUG_PRINT("Added ε to First(" << symbol_name_map[A] << ")");
                }
                continue;
            }

            /* A -> X1 X2 ... Xn：依次并入 First(Xi) 直至某 Xi 不含 ε */
            bool all_epsilon = true;
            for (size_t j = 0; j < rhs.size(); j++) {
                int X = rhs[j];
                for (int t : first_set[X]) {
                    if (t != EPSILON) {
                        if (first_set[A].insert(t).second) {
                            changed = true;
                            DEBUG_PRINT("Added " << symbol_name_map[t]
                                       << " to First(" << symbol_name_map[A] << ")");
                        }
                    }
                }
                if (!first_set[X].count(EPSILON)) {
                    all_epsilon = false;
                    break;
                }
            }
            if (all_epsilon) {
                if (first_set[A].insert(EPSILON).second) {
                    changed = true;
                    DEBUG_PRINT("Added ε to First(" << symbol_name_map[A] << ")");
                }
            }
        }
    }

    DEBUG_PRINT("First sets computation completed");
}

/**
 * 计算符号串 β 的 First(β)
 * 用于 closure：First(βa) 决定可闭入项目的向前看符
 */
set<int> compute_first_of_string(const vector<int>& syms) {
    set<int> result;
    if (syms.empty()) {
        result.insert(EPSILON);
        return result;
    }
    bool all_epsilon = true;
    for (int sym : syms) {
        if (IS_TERMINAL(sym)) {
            result.insert(sym);
            all_epsilon = false;
            break;
        }
        auto it = first_set.find(sym);
        if (it == first_set.end()) {
            result.insert(sym);
            all_epsilon = false;
            break;
        }
        bool has_eps = false;
        for (int t : it->second) {
            if (t == EPSILON) has_eps = true;
            else result.insert(t);
        }
        if (!has_eps) {
            all_epsilon = false;
            break;
        }
    }
    if (all_epsilon) result.insert(EPSILON);
    return result;
}
