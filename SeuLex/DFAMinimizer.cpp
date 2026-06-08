/*
 * ============================================================================
 * DFAMinimizer.cpp — DFA 最小化（等价类划分）
 * ============================================================================
 *
 * 【算法名称】Partition Refinement / Moore 风格等价类划分
 *
 * 【初始划分】按 (isAccept, ruleId) 分组
 *   - 不同 ruleId 的接受态不可合并（对应不同词法动作）
 *   - 接受态与非接受态不可合并
 *
 * 【细化循环】
 *   对每组 G 内状态，构造签名向量 sig[ch] = stateGroup[next[ch]]
 *   按 sig 将 G 拆成子组；若组数变化则 changed=true，继续迭代
 *   直至组数稳定 → 每组为一个等价类
 *
 * 【合并】每类选一个代表态 rep，转移目标映射为 newId[nxt]
 *
 * 【重排】生成的 yylex 假定 DFA 状态 0 为起始态
 *   若原状态 0 所在类不在索引 0，则交换行使起始类位于 0
 *
 * 【复杂度】O(n² · |Σ|) 量级，n 为 DFA 状态数
 * ============================================================================
 */
#include "DFAMinimizer.h"
#include "lex_common.h"
#include <vector>
#include <map>
#include <set>
#include <cstring>
using namespace std;

vector<DFAState> DFAMinimizer::minimize(const vector<DFAState>& dfa) {
    if (dfa.empty()) return {};
    int n = dfa.size();

    /* 初始划分：非接受态一组；接受态按 ruleId 再细分（不同动作不可合并） */
    vector<vector<int>> groups;
    map<pair<bool, int>, int> groupIndex;
    for (int i = 0; i < n; ++i) {
        auto key = make_pair(dfa[i].isAccept, dfa[i].ruleId);
        if (!groupIndex.count(key)) {
            groupIndex[key] = groups.size();
            groups.emplace_back();
        }
        groups[groupIndex[key]].push_back(i);
    }

    vector<int> stateGroup(n, -1);
    auto updateStateGroup = [&]() {
        for (size_t g = 0; g < groups.size(); ++g)
            for (int s : groups[g])
                stateGroup[s] = (int)g;
    };
    updateStateGroup();

    /* 反复按“各字符上的目标等价类编号向量”分裂，直至组数不变 */
    bool changed = true;
    while (changed) {
        changed = false;
        vector<vector<int>> newGroups;
        for (const auto& group : groups) {
            if (group.empty()) continue;
            map<vector<int>, vector<int>> split;
            for (int s : group) {
                vector<int> transKey(ALPHABET_SIZE, -1);
                for (int ch = 0; ch < ALPHABET_SIZE; ++ch) {
                    int nxt = dfa[s].next[ch];
                    transKey[ch] = (nxt == -1) ? -1 : stateGroup[nxt];
                }
                split[transKey].push_back(s);
            }
            for (auto& kv : split) {
                newGroups.push_back(kv.second);
            }
        }
        if (newGroups.size() != groups.size()) changed = true;
        groups = move(newGroups);
        updateStateGroup();
    }

    /* 每组选一个代表态，合并转移目标为新的组编号 */
    vector<DFAState> minimized;
    vector<int> newId(n, -1);
    
    for (size_t g = 0; g < groups.size(); ++g) {
        int rep = groups[g][0];
        for (int s : groups[g]) {
            newId[s] = (int)minimized.size();
        }
        DFAState st;
        st.isAccept = dfa[rep].isAccept;
        st.ruleId = dfa[rep].ruleId;
        memset(st.next, -1, sizeof(st.next));
        minimized.push_back(st);
    }
    
    for (size_t g = 0; g < groups.size(); ++g) {
        int rep = groups[g][0];
        DFAState& st = minimized[newId[rep]];
        for (int ch = 0; ch < ALPHABET_SIZE; ++ch) {
            int nxt = dfa[rep].next[ch];
            if (nxt != -1) {
                st.next[ch] = newId[nxt];
            }
        }
    }

    /* 重排：生成的 yylex 假定 state=0 为起始态，须将含原状态 0 的组放到索引 0 */
    int start_old = 0;
    int start_new = newId[start_old];   // 原始起始状态在最小化 DFA 中的索引

    if (start_new != 0) {
        vector<DFAState> reordered;
        vector<int> old_to_new(minimized.size(), -1);

        reordered.push_back(minimized[start_new]);  /* 新表索引 0 = 原起始等价类 */
        old_to_new[start_new] = 0;

        int next_idx = 1;
        for (size_t i = 0; i < minimized.size(); ++i) {
            if ((int)i == start_new) continue;
            old_to_new[i] = next_idx++;
            reordered.push_back(minimized[i]);
        }

        /* 用 old_to_new 重映射每条 next[ch] */
        for (auto& state : reordered) {
            for (int ch = 0; ch < ALPHABET_SIZE; ++ch) {
                int nxt = state.next[ch];
                if (nxt != -1) {
                    if (nxt < (int)old_to_new.size()) {
                        state.next[ch] = old_to_new[nxt];
                    } else {
                        state.next[ch] = -1;  /* 防御：非法目标置无转移 */
                    }
                }
            }
        }

        minimized = move(reordered);
    }

    return minimized;
}