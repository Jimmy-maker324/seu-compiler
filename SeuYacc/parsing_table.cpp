/*
 * ============================================================================
 * parsing_table.cpp — Action/Goto 分析表构造（SeuYacc 第 5 阶段）
 * ============================================================================
 *
 * 【数据结构】
 *   action_table[state][terminal] -> Action { SHIFT|REDUCE|ACCEPT|ERROR, num }
 *   goto_table[state][nonterm_idx] -> 下一状态 ID（-1 表示无转移）
 *   prod_info[i] -> { left, right_len } 供归约时弹栈与执行语义动作
 *
 * 【填表规则（对状态 i 的每个项目 [A->α·β, a]）】
 *   | 条件              | Action[i, *]              |
 *   |-------------------|---------------------------|
 *   | 圆点后为终结符 t  | SHIFT → goto(i,t)         |
 *   | 圆点在末尾且为 S' | ACCEPT（lookahead=$）     |
 *   | 圆点在末尾        | REDUCE 产生式编号         |
 *   | 非终结符转移 B    | goto_table[i][B] = 目标态 |
 *
 * 【resolve_conflict 移进-归约策略】
 *   比较移进符 t 与归约产生式的优先级 prec_level（%left / %prec）：
 *   - 归约优先级高 → REDUCE；移进优先级高 → SHIFT
 *   - 相等：左结合→REDUCE，右结合→SHIFT，非结合→SHIFT
 *   双方均未声明优先级时计入未消解冲突（构建失败，不再默认 shift）
 *   归约-归约：双方均无优先级时计入未消解冲突；否则保留产生式编号较小者
 *
 * 【运行时配合】yyparse 用栈维护 (state, semantic_value)，查 action_table 驱动
 * ============================================================================
 */
#include "yacc_common.h"
#include <sstream>

namespace {
    int g_heuristic_conflict_resolutions = 0;
    int g_unresolved_reduce_reduce = 0;
    void init_parsing_tables();
    void process_state_for_action(LR1State& state);
    void process_state_for_goto(LR1State& state);
    void fill_prod_info();

    string terminal_name(int sym) {
        auto it = symbol_name_map.find(sym);
        if (it != symbol_name_map.end()) return it->second;
        ostringstream os;
        os << "sym(" << sym << ")";
        return os.str();
    }

    string production_label(int prod_id) {
        if (prod_id < 0 || prod_id >= (int)productions.size()) {
            return "prod(" + to_string(prod_id) + ")";
        }
        Production& p = productions[prod_id];
        ostringstream os;
        os << "prod " << prod_id << " (" << symbol_name_map[p.left] << " ->";
        for (int s : p.right) os << " " << symbol_name_map[s];
        os << ")";
        return os.str();
    }

    string conflict_signature(const ParseConflict& c) {
        ostringstream os;
        os << (int)c.kind << ":" << c.terminal << ":";
        switch (c.kind) {
            case ParseConflict::SHIFT_REDUCE:
                os << c.shift_state << ":" << c.reduce_prod;
                break;
            case ParseConflict::REDUCE_REDUCE:
                os << min(c.reduce_prod, c.reduce_prod2) << ":"
                   << max(c.reduce_prod, c.reduce_prod2);
                break;
            case ParseConflict::SHIFT_SHIFT:
                os << min(c.shift_state, c.reduce_prod) << ":"
                   << max(c.shift_state, c.reduce_prod);
                break;
        }
        return os.str();
    }

    void scan_state_conflicts(const LR1State& state, vector<ParseConflict>& out) {
        map<int, set<int>> reduces;
        map<int, set<int>> shifts;
        set<int> accepts;

        for (const LR1Item& item : state.items) {
            Production& prod = productions[item.prod_id];
            if ((size_t)item.dot_pos < prod.right.size()) {
                int X = prod.right[item.dot_pos];
                if (IS_TERMINAL(X) && state.transitions.count(X)) {
                    shifts[X].insert(state.transitions.at(X));
                }
            } else {
                int la = item.lookahead;
                if (item.prod_id == 0) {
                    accepts.insert(la);
                } else {
                    reduces[la].insert(item.prod_id);
                }
            }
        }

        set<int> all_terms;
        for (auto& p : reduces) all_terms.insert(p.first);
        for (auto& p : shifts) all_terms.insert(p.first);
        all_terms.insert(accepts.begin(), accepts.end());

        for (int term : all_terms) {
            auto rit = reduces.find(term);
            auto sit = shifts.find(term);
            bool has_reduce = rit != reduces.end() && !rit->second.empty();
            bool has_shift = sit != shifts.end() && !sit->second.empty();
            bool has_accept = accepts.count(term) > 0;

            if (has_shift && sit->second.size() > 1) {
                auto it = sit->second.begin();
                int s1 = *it++;
                int s2 = *it;
                ParseConflict c;
                c.kind = ParseConflict::SHIFT_SHIFT;
                c.state = state.id;
                c.terminal = term;
                c.shift_state = s1;
                c.reduce_prod = s2;
                c.reduce_prod2 = -1;
                out.push_back(c);
            }

            if (has_reduce && rit->second.size() > 1) {
                auto it = rit->second.begin();
                int p1 = *it++;
                for (; it != rit->second.end(); ++it) {
                    ParseConflict c;
                    c.kind = ParseConflict::REDUCE_REDUCE;
                    c.state = state.id;
                    c.terminal = term;
                    c.shift_state = -1;
                    c.reduce_prod = p1;
                    c.reduce_prod2 = *it;
                    out.push_back(c);
                }
            }

            if (has_shift && has_reduce) {
                int shift_state = *sit->second.begin();
                for (int prod_id : rit->second) {
                    ParseConflict c;
                    c.kind = ParseConflict::SHIFT_REDUCE;
                    c.state = state.id;
                    c.terminal = term;
                    c.shift_state = shift_state;
                    c.reduce_prod = prod_id;
                    c.reduce_prod2 = -1;
                    out.push_back(c);
                }
            }

            if (has_accept && has_reduce) {
                for (int prod_id : rit->second) {
                    ParseConflict c;
                    c.kind = ParseConflict::REDUCE_REDUCE;
                    c.state = state.id;
                    c.terminal = term;
                    c.shift_state = -1;
                    c.reduce_prod = 0;
                    c.reduce_prod2 = prod_id;
                    out.push_back(c);
                }
            }
        }
    }
}

/** 返回终结符优先级层次，未声明为 -1 */
int get_term_precedence(int term) {
    auto it = precedence_map.find(term);
    if (it != precedence_map.end())
        return it->second.level;
    return -1;
}

/** 返回终结符结合性，默认左结合 */
AssocType get_term_assoc(int term) {
    auto it = precedence_map.find(term);
    if (it != precedence_map.end())
        return it->second.assoc;
    return ASSOC_LEFT;
}

/** 扫描 DFA 各状态项目集，收集 Action 冲突（不填表） */
vector<ParseConflict> detect_parsing_conflicts(const LR1DFA& dfa) {
    vector<ParseConflict> conflicts;
    for (const LR1State& state : dfa.states) {
        scan_state_conflicts(state, conflicts);
    }
    return conflicts;
}

/** 对比 LALR 合并前后冲突集，输出警告；返回合并新暴露的冲突数 */
int report_lalr_merge_conflicts(const vector<ParseConflict>& pre_merge,
                                const vector<ParseConflict>& post_merge) {
    set<string> pre_sigs;
    for (const ParseConflict& c : pre_merge) {
        pre_sigs.insert(conflict_signature(c));
    }

    int newly_exposed = 0;
    if (post_merge.empty()) {
        return 0;
    }

    cerr << "\n[LALR] Warning: parsing conflicts detected after state merge:\n";
    for (const ParseConflict& c : post_merge) {
        bool is_new = pre_sigs.find(conflict_signature(c)) == pre_sigs.end();
        if (is_new) newly_exposed++;

        cerr << "  state I" << c.state << ", on " << terminal_name(c.terminal) << ": ";
        switch (c.kind) {
            case ParseConflict::REDUCE_REDUCE:
                cerr << "reduce-reduce between "
                     << production_label(c.reduce_prod) << " and "
                     << production_label(c.reduce_prod2);
                break;
            case ParseConflict::SHIFT_REDUCE:
                cerr << "shift-reduce (shift -> I" << c.shift_state
                     << ", reduce " << production_label(c.reduce_prod) << ")";
                break;
            case ParseConflict::SHIFT_SHIFT:
                cerr << "shift-shift (I" << c.shift_state << " vs I"
                     << c.reduce_prod << ")";
                break;
        }
        if (is_new) cerr << " [newly exposed by LALR merge]";
        cerr << "\n";
    }

    cerr << "[LALR] Total: " << post_merge.size() << " conflict(s)";
    if (newly_exposed > 0) {
        cerr << ", " << newly_exposed << " newly exposed (absent in LR(1) DFA)";
    }
    cerr << ". Must be resolved by grammar or %prec before code generation.\n\n";

    return newly_exposed;
}

/** 主入口：初始化表、填充 Action/Goto、生成 prod_info；返回未声明优先级的冲突消解次数 */
int build_parsing_table() {
    g_heuristic_conflict_resolutions = 0;
    g_unresolved_reduce_reduce = 0;
    DEBUG_PRINT("Start building Action/Goto parsing table...");
    init_parsing_tables();
    fill_prod_info();

    for (size_t i = 0; i < lr_dfa.states.size(); i++) {
        LR1State& state = lr_dfa.states[i];
        process_state_for_action(state);
        process_state_for_goto(state);
    }

    DEBUG_PRINT("Parsing table construction completed!");
    return g_heuristic_conflict_resolutions + g_unresolved_reduce_reduce;
}

/**
 * 冲突消解：移进-归约比较移进符与产生式优先级；相等时看结合性
 * 归约-归约：保留产生式编号较小者
 */
Action resolve_conflict(const Action& cur, const Action& new_act, int lookahead_token, int prod_id) {
    if (cur.type == ACTION_SHIFT && new_act.type == ACTION_REDUCE) {
        int term_prec = get_term_precedence(lookahead_token);
        int prod_prec = productions[prod_id].prec_level;
        if (prod_prec < 0 && term_prec < 0) {
            g_heuristic_conflict_resolutions++;
            return cur;
        }
        if (prod_prec > term_prec) {
            return new_act;
        } else if (prod_prec < term_prec) {
            return cur;
        } else {
            AssocType assoc = get_term_assoc(lookahead_token);
            if (assoc == ASSOC_LEFT)
                return new_act;
            else if (assoc == ASSOC_RIGHT)
                return cur;
            else
                return cur;
        }
    }
    if (cur.type == ACTION_REDUCE && new_act.type == ACTION_REDUCE) {
        if (productions[cur.num].prec_level < 0 && productions[new_act.num].prec_level < 0)
            g_unresolved_reduce_reduce++;
        return (cur.num < new_act.num) ? cur : new_act;
    }
    return cur;
}

namespace {
    /** 按状态数与非终结符数分配 action_table、goto_table 并置 ERROR */
    void init_parsing_tables() {
        int num_states = lr_dfa.states.size();
        int max_term = 0;
        for (auto& p : token_map) {
            if (p.second > max_term) max_term = p.second;
        }
        int num_terminals = 1000;   /* 列下标覆盖 0..999 的终结符 ID */
        int num_non_terms = 0;
        for (auto& pair : non_term_map) {
            int id = pair.second;
            if (id > num_non_terms) num_non_terms = id;
        }
        num_non_terms = num_non_terms - NON_TERM_BASE + 1;

        action_table.resize(num_states, vector<Action>(num_terminals));
        for (int i = 0; i < num_states; i++) {
            for (int j = 0; j < num_terminals; j++) {
                action_table[i][j].type = ACTION_ERROR;
                action_table[i][j].num = -1;
            }
        }
        goto_table.resize(num_states);
        for (int i = 0; i < num_states; i++) {
            goto_table[i].resize(num_non_terms, -1);
        }
    }

    /** 根据 completed / 未完成项目填写移进、归约、接受 */
    void process_state_for_action(LR1State& state) {
        int state_id = state.id;
        for (const LR1Item& item : state.items) {
            Production& prod = productions[item.prod_id];
            if ((size_t)item.dot_pos < prod.right.size()) {
                int X = prod.right[item.dot_pos];
                if (IS_TERMINAL(X) && state.transitions.count(X)) {
                    int next_state = state.transitions[X];
                    Action new_action;
                    new_action.type = ACTION_SHIFT;
                    new_action.num = next_state;
                    Action& cur = action_table[state_id][X];
                    if (cur.type != ACTION_ERROR) {
                        cur = resolve_conflict(cur, new_action, X, -1);
                    } else {
                        cur = new_action;
                    }
                }
            } else {
                int lookahead = item.lookahead;
                if (item.prod_id == 0) {
                    /* 增广产生式 S' -> S · , $  → 接受 */
                    Action new_action;
                    new_action.type = ACTION_ACCEPT;
                    Action& cur = action_table[state_id][lookahead];
                    if (cur.type != ACTION_ERROR) {
                        cur = resolve_conflict(cur, new_action, lookahead, 0);
                    } else {
                        cur = new_action;
                    }
                } else {
                    Action new_action;
                    new_action.type = ACTION_REDUCE;
                    new_action.num = item.prod_id;
                    Action& cur = action_table[state_id][lookahead];
                    if (cur.type != ACTION_ERROR) {
                        cur = resolve_conflict(cur, new_action, lookahead, item.prod_id);
                    } else {
                        cur = new_action;
                    }
                }
            }
        }
    }

    /** 非终结符转移 → goto_table[state][非终结符列] */
    void process_state_for_goto(LR1State& state) {
        int state_id = state.id;
        for (auto& trans : state.transitions) {
            int X = trans.first;
            int next_state = trans.second;
            if (IS_NON_TERM(X)) {
                int idx = X - NON_TERM_BASE;
                if (idx >= 0 && idx < (int)goto_table[state_id].size()) {
                    goto_table[state_id][idx] = next_state;
                }
            }
        }
    }

    /** 为代码生成准备每条产生式的左部索引与右部长度 */
    void fill_prod_info() {
        prod_info.resize(productions.size());
        for (size_t i = 0; i < productions.size(); i++) {
            Production& prod = productions[i];
            prod_info[i].left = prod.left - NON_TERM_BASE;
            prod_info[i].right_len = prod.right.size();
        }
    }
}
