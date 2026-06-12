/*
 * ============================================================================
 * lr1_dfa.cpp — LR(1) 项目集规范族与 DFA 构造（SeuYacc 第 3 阶段）
 * ============================================================================
 *
 * 【核心数据结构】
 *   LR1Item { prod_id, dot_pos, lookahead }
 *     - 表示 [A -> α·β, a]：产生式 A->αβ，圆点位置，向前看 a
 *   LR1State { id, items, transitions }
 *     - items: 一个项目集（状态）
 *     - transitions: map<符号, 下一状态ID>
 *   LR1DFA { deque<LR1State> states, start_state }
 *
 * 【closure(I) 算法】
 *   输入：项目集 I
 *   输出：含 I 的最小闭包集
 *   1. result = I，worklist = I 中所有项目
 *   2. while worklist 非空：
 *        取 [A->α·Bβ, a]
 *        若圆点后非终结符 B：
 *          βa = β 拼接 a；L = First(βa)
 *          对 B 的每条产生式 B->γ、每个 b∈L：
 *            加入 [B->·γ, b] 到 result（若新）
 *   3. return result
 *
 * 【goto(I, X) 算法】
 *   1. 对 I 中 [A->α·Xβ, a] 且 X 匹配：生成 [A->αX·β, a]
 *   2. return 上述项目集合（未闭包）
 *
 * 【build_lr1_dfa 流程（BFS）】
 *   1. I0 = closure({[S'->·S, $]})
 *   2. 队列 q = {0}
 *   3. while q 非空：
 *        取状态 s；收集 s 中所有“圆点后符号”集合 Γ
 *        对每个 X∈Γ：
 *          J = closure(goto(s.items, X))
 *          若 J 已存在 → 连边到旧状态；否则新建状态并入队
 *
 * 【与 LR(0) 区别】项目含 lookahead，状态数更多，冲突更少
 * ============================================================================
 */
#include "yacc_common.h"
#include <queue>
#include <unordered_set>

namespace {
    /** 判断两个 LR(1) 项目集是否相同 */
    bool is_same_item_set(const unordered_set<LR1Item>& set1,
                          const unordered_set<LR1Item>& set2) {
        if (set1.size() != set2.size()) return false;
        for (const LR1Item& item : set1) {
            if (set2.find(item) == set2.end()) return false;
        }
        return true;
    }

    /** 在已有状态中查找与 items 同构的项目集，返回状态 ID 或 -1 */
    int find_existing_state(const unordered_set<LR1Item>& items) {
        for (size_t i = 0; i < lr_dfa.states.size(); ++i) {
            if (is_same_item_set(items, lr_dfa.states[i].items))
                return (int)i;
        }
        return -1;
    }
}

/**
 * LR(1) 闭包：对 [A→α·Bβ, a] 加入 B 的所有 [B→·γ, b]，b ∈ First(βa)
 */
unordered_set<LR1Item> closure(const unordered_set<LR1Item>& items) {
    DEBUG_PRINT("closure called with " << items.size() << " items");
    unordered_set<LR1Item> result = items;
    queue<LR1Item> worklist;
    for (const LR1Item& it : items) worklist.push(it);

    while (!worklist.empty()) {
        LR1Item item = worklist.front();
        worklist.pop();

        Production& prod = productions[item.prod_id];
        if (item.dot_pos >= (int)prod.right.size()) continue;

        int B = prod.right[item.dot_pos];
        if (!IS_NON_TERM(B)) continue;

        /* β 为圆点后的剩余符号；向前看为 First(βa) */
        vector<int> beta;
        for (size_t i = item.dot_pos + 1; i < prod.right.size(); ++i)
            beta.push_back(prod.right[i]);
        vector<int> beta_a = beta;
        beta_a.push_back(item.lookahead);
        set<int> lookaheads = compute_first_of_string(beta_a);

        for (size_t pid = 0; pid < productions.size(); ++pid) {
            Production& bprod = productions[pid];
            if (bprod.left != B) continue;
            for (int la : lookaheads) {
                if (la == EPSILON) continue;
                LR1Item new_item;
                new_item.prod_id = pid;
                new_item.dot_pos = 0;
                new_item.lookahead = la;
                if (result.find(new_item) == result.end()) {
                    result.insert(new_item);
                    worklist.push(new_item);
                    DEBUG_PRINT("    Added: " << symbol_name_map[bprod.left] << " -> ... , lookahead=" << symbol_name_map[la]);
                }
            }
        }
    }
    DEBUG_PRINT("closure returns " << result.size() << " items");
    return result;
}

/**
 * Go(I, X)：将 I 中圆点后为 X 的项目圆点右移一位
 */
unordered_set<LR1Item> goto_trans(const unordered_set<LR1Item>& items, int sym) {
    DEBUG_PRINT("      goto_trans called with sym=" << sym << " (" << symbol_name_map[sym] << "), items.size()=" << items.size());
    unordered_set<LR1Item> result;
    for (const LR1Item& item : items) {
        Production& prod = productions[item.prod_id];
        if (item.dot_pos < (int)prod.right.size()) {
            int next_sym = prod.right[item.dot_pos];
            DEBUG_PRINT("        item: " << symbol_name_map[prod.left] << " -> ... dot=" << item.dot_pos
                        << " next_sym=" << next_sym << " (" << symbol_name_map[next_sym] << ")");
            if (next_sym == sym) {
                LR1Item new_item = item;
                new_item.dot_pos++;
                result.insert(new_item);
                DEBUG_PRINT("          matched");
            }
        }
    }
    DEBUG_PRINT("      goto_trans returning " << result.size() << " items");
    return result;
}

/**
 * 构造完整 LR(1) DFA：初始状态为 closure({[S'→·S, $]})
 */
void build_lr1_dfa() {
    DEBUG_PRINT("Start building LR(1) automaton...");
    lr_dfa.states.clear();
    lr_dfa.start_state = -1;

    LR1Item initial_item;
    initial_item.prod_id = 0;      /* 增广产生式 S' -> S */
    initial_item.dot_pos = 0;
    initial_item.lookahead = T_EOF;

    unordered_set<LR1Item> initial_items = { initial_item };
    unordered_set<LR1Item> closure0 = closure(initial_items);

    LR1State state0;
    state0.id = 0;
    state0.items = closure0;
    lr_dfa.states.push_back(state0);
    lr_dfa.start_state = 0;

    queue<int> q;
    q.push(0);
    int next_id = 1;

    while (!q.empty()) {
        int cur_id = q.front();
        q.pop();
        LR1State& cur_state = lr_dfa.states[cur_id];

        DEBUG_PRINT("Processing state I" << cur_id << " with " << cur_state.items.size() << " items");

        /* 收集本状态所有可移进符号（圆点后的第一个符号） */
        unordered_set<int> symbols;
        for (const LR1Item& item : cur_state.items) {
            Production& prod = productions[item.prod_id];
            if (item.dot_pos < (int)prod.right.size()) {
                symbols.insert(prod.right[item.dot_pos]);
            }
        }

        DEBUG_PRINT("  Symbols to process (" << symbols.size() << "):");
        for (int sym : symbols) {
            DEBUG_PRINT("    " << symbol_name_map[sym] << " (" << sym << ")");
            (void)sym;
        }

        for (int X : symbols) {
            DEBUG_PRINT("  Goto(I" << cur_id << ", " << symbol_name_map[X] << ")...");
            unordered_set<LR1Item> goto_items = goto_trans(cur_state.items, X);
            if (goto_items.empty()) {
                DEBUG_PRINT("    goto_items empty, skip");
                continue;
            }
            unordered_set<LR1Item> new_items = closure(goto_items);
            DEBUG_PRINT("    new_items size = " << new_items.size());

            int exist = find_existing_state(new_items);
            if (exist != -1) {
                cur_state.transitions[X] = exist;
                DEBUG_PRINT("    Reuse existing state I" << exist);
            } else {
                LR1State new_state;
                new_state.id = next_id++;
                new_state.items = new_items;
                lr_dfa.states.push_back(new_state);
                cur_state.transitions[X] = new_state.id;
                q.push(new_state.id);
                DEBUG_PRINT("    Create new state I" << new_state.id);
            }
        }
    }

    DEBUG_PRINT("LR(1) DFA construction completed, total states: " << lr_dfa.states.size());
#if DEBUG
    print_lr_dfa();
#endif
}
