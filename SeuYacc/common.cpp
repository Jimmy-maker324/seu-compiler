/*
 * ============================================================================
 * common.cpp — SeuYacc 全局变量定义与调试辅助
 * ============================================================================
 *
 * 【职责】yacc_common.h 中 extern 声明的全局容器在此唯一实例化，
 *         避免多编译单元重复定义链接错误。
 *
 * 【print_first_set / print_lr_dfa / print_parsing_table】
 *         供 --print-* 命令行选项输出中间结果，便于实验调试。
 * ============================================================================
 */
#include "yacc_common.h"

/* ---------- 文法与符号表 ---------- */
map<string, int> token_map;
map<string, int> non_term_map;
map<int, string> symbol_name_map;
vector<Production> productions;
map<int, Precedence> precedence_map;
int origin_start_symbol = -1;
int aug_start_symbol = -1;
map<int, set<int>> first_set;
LR1DFA lr_dfa;
vector<vector<Action>> action_table;
vector<vector<int>> goto_table;
vector<ProdInfo> prod_info;
string decl_user_code;
string user_sub_code;
int yylineno = 1;

/** 生成器内部语法错误报告（与目标 yyparse 的 yyerror 不同） */
void yyerror(const string& msg) {
    cerr << "[Syntax error] Line:" << yylineno << ", Error: " << msg << endl;
}

/** 调试：仅打印非终结符的 First 集 */
void print_first_set() {
    cout << "   --- First (non-terminals only) ---" << endl;
    for (auto& pair : first_set) {
        int sym = pair.first;
        if (!IS_NON_TERM(sym)) continue;
        cout << "   " << symbol_name_map[sym] << " = { ";
        bool first = true;
        for (int t : pair.second) {
            if (!first) cout << ", ";
            first = false;
            if (t == EPSILON) cout << "eps";
            else cout << symbol_name_map[t];
        }
        cout << " }" << endl;
    }
}

/** 调试：打印 LR DFA 各状态的项目与转移 */
void print_lr_dfa() {
    cout << "\n===== LR DFA States =====" << endl;
    for (auto& state : lr_dfa.states) {
        cout << "\nState I" << state.id << ":" << endl;
        for (auto& item : state.items) {
            Production& prod = productions[item.prod_id];
            cout << "  " << symbol_name_map[prod.left] << " -> ";
            for (size_t i = 0; i < prod.right.size(); i++) {
                if (i == (size_t)item.dot_pos) cout << "· ";
                cout << symbol_name_map[prod.right[i]] << " ";
            }
            if ((size_t)item.dot_pos == prod.right.size()) cout << "· ";
            cout << ", " << symbol_name_map[item.lookahead] << endl;
        }
        if (!state.transitions.empty()) {
            cout << "  Transitions:" << endl;
            for (auto& trans : state.transitions) {
                cout << "    " << symbol_name_map[trans.first] << " -> I" << trans.second << endl;
            }
        }
    }
}

/** 调试：打印分析表规模摘要 */
void print_parsing_table() {
    cout << "\n===== Parsing Table Construction Completed =====" << endl;
    cout << "Action table rows (states): " << action_table.size() << endl;
    cout << "Goto table rows (states): " << goto_table.size() << endl;
    cout << "Total productions: " << productions.size() << endl;
}

/** 注册终结符（已存在则忽略） */
void add_token(const string& name, int id) {
    if (token_map.count(name)) return;
    token_map[name] = id;
    symbol_name_map[id] = name;
}

/**
 * 注册非终结符并分配唯一 ID（从 NON_TERM_BASE 递增）
 * @return 符号 ID
 */
int add_non_term(const string& name) {
    if (non_term_map.count(name)) return non_term_map[name];
    static int next_id = NON_TERM_BASE;
    int id = next_id++;
    non_term_map[name] = id;
    symbol_name_map[id] = name;
    return id;
}
