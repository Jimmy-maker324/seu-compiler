/*
 * ============================================================================
 * lalr.cpp — LALR(1) 状态合并（SeuYacc 第 4 阶段，可选）
 * ============================================================================
 *
 * 【算法思想】
 *   LR(1) 中若两个状态的项目“核心”相同（同一组 [A->α·β, *]），
 *   仅 lookahead 不同，则合并为一个 LALR 状态，lookahead 取并集。
 *
 * 【数据结构】
 *   ItemCore { prod_id, dot_pos }     — 项目核心（忽略 lookahead）
 *   core_groups: vector<vector<int>>   — 每组为可合并的 LR(1) 状态 ID 列表
 *   old_to_new_id: vector<int>          — 旧状态 → 新状态编号映射
 *
 * 【merge_lalr_dfa 五步流程】
 *   Step1 分组：O(n²) 两两比较 has_same_core，核心相同划入同组
 *   Step2 映射：每组分配一个新状态 ID
 *   Step3 合并项目：组内所有 (core, lookahead) 合并为并集后写入新状态
 *   Step4 重建转移：用组代表状态的 transitions，目标改为 new_id
 *   Step5 写回 lr_dfa.states，更新 start_state
 *
 * 【风险】合并可能引入新的移进-归约 / 归约-归约冲突；
 *         merge 后由 detect_parsing_conflicts + report_lalr_merge_conflicts 显式警告
 * ============================================================================
 */
#include "yacc_common.h"

namespace {
    /** LR(1) 项目核心：忽略 lookahead */
    struct ItemCore {
        int prod_id;
        int dot_pos;

        bool operator==(const ItemCore& other) const {
            return prod_id == other.prod_id && dot_pos == other.dot_pos;
        }
    };

    struct CoreHash {
        size_t operator()(const ItemCore& core) const {
            return hash<int>()(core.prod_id) ^ (hash<int>()(core.dot_pos) << 1);
        }
    };

    ItemCore get_item_core(const LR1Item& item);
    unordered_set<ItemCore, CoreHash> get_state_core(const LR1State& state);
    bool has_same_core(const LR1State& s1, const LR1State& s2);
}

/**
 * 按项目核心对 LR(1) 状态分组并合并，原地替换 lr_dfa.states
 */
void merge_lalr_dfa() {
    DEBUG_PRINT("Start merging LALR(1) core sets...");
    size_t original_state_count = lr_dfa.states.size();

    if (original_state_count == 0) {
        DEBUG_PRINT("LR DFA is empty, no need to merge");
        return;
    }

    /* 步骤 1：将核心相同的状态划入同一组 */
    DEBUG_PRINT("Step 1: Grouping by core...");

    vector<vector<int>> core_groups;
    vector<bool> processed(original_state_count, false);

    for (size_t i = 0; i < original_state_count; i++) {
        if (processed[i]) continue;

        vector<int> group;
        group.push_back((int)i);
        processed[i] = true;

        for (size_t j = i + 1; j < original_state_count; j++) {
            if (!processed[j] && has_same_core(lr_dfa.states[i], lr_dfa.states[j])) {
                group.push_back((int)j);
                processed[j] = true;
            }
        }

        core_groups.push_back(group);
        DEBUG_PRINT("  Found core group containing " << group.size() << " states");
    }

    DEBUG_PRINT("Total core groups: " << core_groups.size());

    /* 步骤 2：旧状态 ID → 新状态 ID */
    DEBUG_PRINT("Step 2: Building state mapping...");

    vector<int> old_to_new_id(original_state_count, -1);
    deque<LR1State> new_states;

    for (size_t group_idx = 0; group_idx < core_groups.size(); group_idx++) {
        vector<int>& group = core_groups[group_idx];
        int new_id = (int)group_idx;

        for (int old_id : group) {
            old_to_new_id[old_id] = new_id;
        }

        /* 步骤 3：合并组内所有项目的 lookahead */
        LR1State new_state;
        new_state.id = new_id;

        unordered_map<ItemCore, unordered_set<int>, CoreHash> core_to_lookaheads;

        for (int old_id : group) {
            LR1State& old_state = lr_dfa.states[old_id];
            for (const LR1Item& item : old_state.items) {
                ItemCore core = get_item_core(item);
                core_to_lookaheads[core].insert(item.lookahead);
            }
        }

        for (auto& pair : core_to_lookaheads) {
            const ItemCore& core = pair.first;
            unordered_set<int>& lookaheads = pair.second;

            for (int lookahead : lookaheads) {
                LR1Item new_item;
                new_item.prod_id = core.prod_id;
                new_item.dot_pos = core.dot_pos;
                new_item.lookahead = lookahead;
                new_state.items.insert(new_item);
            }
        }

        new_states.push_back(new_state);
        DEBUG_PRINT("  New state I" << new_id << " merged, item count: " << new_state.items.size());
    }

    /* 步骤 4：用代表状态的转移映射到新目标状态 */
    DEBUG_PRINT("Step 4: Rebuilding transitions...");

    for (size_t group_idx = 0; group_idx < core_groups.size(); group_idx++) {
        vector<int>& group = core_groups[group_idx];
        int new_id = (int)group_idx;
        LR1State& new_state = new_states[new_id];

        int representative_old_id = group[0];
        LR1State& old_state = lr_dfa.states[representative_old_id];

        for (auto& trans : old_state.transitions) {
            int sym = trans.first;
            int old_target_id = trans.second;
            int new_target_id = old_to_new_id[old_target_id];

            if (new_target_id != -1) {
                new_state.transitions[sym] = new_target_id;
            }
        }
    }

    /* 步骤 5：写回全局 DFA */
    lr_dfa.states.swap(new_states);
    lr_dfa.start_state = old_to_new_id[lr_dfa.start_state];

    DEBUG_PRINT("LALR(1) merge completed!");
    DEBUG_PRINT("  Original state count: " << original_state_count);
    DEBUG_PRINT("  Merged state count: " << lr_dfa.states.size());
    DEBUG_PRINT("  Reduced by: " << (original_state_count - lr_dfa.states.size()) << " states");

    #if DEBUG
    print_lr_dfa();
    #endif
}

namespace {
    ItemCore get_item_core(const LR1Item& item) {
        ItemCore core;
        core.prod_id = item.prod_id;
        core.dot_pos = item.dot_pos;
        return core;
    }

    unordered_set<ItemCore, CoreHash> get_state_core(const LR1State& state) {
        unordered_set<ItemCore, CoreHash> cores;
        for (const LR1Item& item : state.items) {
            cores.insert(get_item_core(item));
        }
        return cores;
    }

    bool has_same_core(const LR1State& s1, const LR1State& s2) {
        if (s1.items.size() != s2.items.size()) {
            return false;
        }

        unordered_set<ItemCore, CoreHash> core1 = get_state_core(s1);
        unordered_set<ItemCore, CoreHash> core2 = get_state_core(s2);

        if (core1.size() != core2.size()) {
            return false;
        }

        for (const ItemCore& c : core1) {
            if (core2.find(c) == core2.end()) {
                return false;
            }
        }

        return true;
    }
}
