/*
 * ============================================================================
 * DFAConverter.cpp — 子集构造法（NFA → DFA）
 * ============================================================================
 *
 * 【算法名称】Subset Construction（子集构造）
 *
 * 【数据结构】
 *   DFAState.nfaStates — 该 DFA 态对应的 NFA 状态集合（仅构造时用）
 *   DFAState.next[256] — 确定性转移表；-1 表示无转移
 *   map<set<int>, int> stateMap — NFA 状态集 → DFA 状态编号（去重）
 *
 * 【epsClosure(T)】从集合 T 出发沿 ε 边 DFS/BFS 可达的所有态
 * 【move(T, c)】T 中每态读 c 一步（不扩展 ε）得到的目标态集合
 *
 * 【convert 主流程】
 *   1. D0 = epsClosure({nfa.start})
 *   2. worklist = {D0}，stateMap[D0]=0
 *   3. while worklist 非空：
 *        对当前集合 S、每个字符 ch∈[0,255]：
 *          U = epsClosure(move(S,ch))
 *          若 U 非空：分配/复用 DFA 态 ID，填 next[ch]
 *   4. 若 S 含 NFA 接受态：DFA 态为接受，ruleId = min(接受态.ruleId)
 *
 * 【Flex 规则优先级】多条规则同时可匹配时，ruleId 较小者优先
 * ============================================================================
 */
#include "DFAConverter.h"
#include "lex_common.h"
#include <cstring>
#include <map>
#include <queue>
#include <stack>
using namespace std;

/** 从 states 出发沿 ε 边可达的全部 NFA 状态（含自身） */
static set<int> epsClosure(const NFA& nfa, const set<int>& states) {
    set<int> closure = states;
    stack<int> stk;
    for (int s : closure) stk.push(s);
    while (!stk.empty()) {
        int s = stk.top(); stk.pop();
        for (const auto& p : nfa.states[s].trans) {
            if (p.first == EPSILON) {
                int t = p.second;
                if (closure.find(t) == closure.end()) {
                    closure.insert(t);
                    stk.push(t);
                }
            }
        }
    }
    return closure;
}

/** 在 states 中每个态上读入字符 ch 能到达的目标态集合（不含 ε 扩展） */
static set<int> moveSet(const NFA& nfa, const set<int>& states, int ch) {
    set<int> result;
    for (int s : states) {
        for (const auto& p : nfa.states[s].trans) {
            if (p.first == ch) {
                result.insert(p.second);
            }
        }
    }
    return result;
}

/** 子集构造主循环：BFS 发现新 DFA 状态并填充 next[ch] */
vector<DFAState> DFAConverter::convert(const NFA& nfa) {
    vector<DFAState> dfaStates;
    map<set<int>, int> stateMap;
    queue<set<int>> worklist;

    set<int> startClosure = epsClosure(nfa, { nfa.startState });

    dfaStates.emplace_back();
    dfaStates[0].nfaStates = startClosure;
    stateMap[startClosure] = 0;
    worklist.push(startClosure);

    while (!worklist.empty()) {
        set<int> curSet = worklist.front(); worklist.pop();
        int curId = stateMap[curSet];

        /* 若子集中含接受态，DFA 态为接受；多规则时 ruleId 取最小（Flex 优先靠前规则） */
        int minRule = -1;
        for (int s : curSet) {
            if (nfa.states[s].isAccept) {
                int rid = nfa.states[s].ruleId;
                if (minRule == -1 || rid < minRule) minRule = rid;
            }
        }
        if (minRule != -1) {
            dfaStates[curId].isAccept = true;
            dfaStates[curId].ruleId = minRule;
        }

        /* 对每个输入字节（跳过 EPSILON 标记）计算下一 DFA 状态 */
        for (int ch = 0; ch < ALPHABET_SIZE; ++ch) {
            if (ch == EPSILON) continue;
            set<int> mv = moveSet(nfa, curSet, ch);
            if (mv.empty()) continue;
            set<int> closure = epsClosure(nfa, mv);
            if (closure.empty()) continue;

            int nextId;
            auto it = stateMap.find(closure);
            if (it == stateMap.end()) {
                nextId = (int)dfaStates.size();
                dfaStates.emplace_back();
                dfaStates[nextId].nfaStates = closure;
                stateMap[closure] = nextId;
                worklist.push(closure);
            } else {
                nextId = it->second;
            }
            dfaStates[curId].next[ch] = nextId;
        }
    }
    return dfaStates;
}