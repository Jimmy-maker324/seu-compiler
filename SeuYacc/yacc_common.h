/*
 * yacc_common.h — SeuYacc 语法分析器生成器公共头文件
 *
 * 汇总：符号编号约定、产生式/LR(1) 数据结构、全局状态、
 *       各编译阶段（文法解析、First、DFA、LALR、分析表、代码生成）的接口声明。
 */
/**
 * @file yacc_common.h
 * @brief SeuYacc 全局数据结构与接口声明
 *
 * 【符号编码】终结符 ID ≤ 999；非终结符 ID ≥ NON_TERM_BASE(1000)
 * 【Production】left + right[] + semantic_action 字符串 + 优先级
 * 【LR1Item / LR1State / LR1DFA】LR(1) 自动机
 * 【Action / action_table / goto_table】LR 分析表，供 code_gen 生成 yyparse
 *
 * 【六阶段流水线】
 *   parse_yacc_file → compute_first_sets → build_lr1_dfa
 *   → merge_lalr_dfa(可选) → build_parsing_table → generate_yyparse_c
 */
#ifndef YACC_COMMON_H
#define YACC_COMMON_H

// ==================== 1. 标准库头文件 ====================
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <deque>

using namespace std;

#include "common_defs.h"

// ==================== 2. 调试宏 ====================
extern bool g_debug_enabled;   /* 由命令行 -v/--verbose 控制 */
#if DEBUG
#define DEBUG_PRINT(x) if (g_debug_enabled) { cout << "[DEBUG] " << x << endl; }
#else
#define DEBUG_PRINT(x)
#endif

// ==================== 3. 符号编号分区 ====================
/* 终结符 ≤999；非终结符从 NON_TERM_BASE(1000) 起编，便于 IS_TERMINAL/IS_NON_TERM 判别 */
#define EPSILON -1   /* 空串 ε 在 First 集计算中的特殊 ID */
#define TERMINAL_MAX    999
#define NON_TERM_BASE   1000

#define IS_TERMINAL(sym)    (sym <= TERMINAL_MAX)
#define IS_NON_TERM(sym)    (sym >= NON_TERM_BASE)

// ==================== 4. 优先级与结合性 ====================
/** 运算符结合性：用于移进-归约冲突消解 */
enum AssocType {
    ASSOC_LEFT,      /* 左结合，同级时倾向归约 */
    ASSOC_RIGHT,     /* 右结合，同级时倾向移进 */
    ASSOC_NONASSOC   /* 不可结合，冲突时应报错 */
};

/** 某终结符在 %left/%right/%nonassoc 声明中的优先级信息 */
struct Precedence {
    int level;       /* 优先级层次，数值越大优先级越高 */
    AssocType assoc;
};

// ==================== 5. 产生式结构 ====================
/** 从文法文件中解析出的一条产生式及其语义动作 */
struct Production {
    int left;                     /* 左部非终结符编号 */
    vector<int> right;            /* 右部符号串（终结符或非终结符 ID） */
    string semantic_action;       /* 花括号内语义动作代码（原样保留供代码生成） */
    int prec_level;               /* 本条产生式优先级（-1 表示未用 %prec 指定） */
    int prec_token;               /* %prec 指定的参考终结符 */

    Production() : left(-1), prec_level(-1), prec_token(-1) {}
};

// ==================== 6. LR(1) 项目 ====================
/** LR(1) 项目：[产生式编号, 圆点位置, 向前看符号] */
struct LR1Item {
    int prod_id;
    int dot_pos;
    int lookahead;

    bool operator==(const LR1Item& other) const {
        return prod_id == other.prod_id && dot_pos == other.dot_pos && lookahead == other.lookahead;
    }
};

namespace std {
    template<> struct hash<LR1Item> {
        size_t operator()(const LR1Item& item) const {
            return hash<int>()(item.prod_id) ^ (hash<int>()(item.dot_pos) << 1) ^ (hash<int>()(item.lookahead) << 2);
        }
    };
}

// ==================== 7. LR(1) DFA 状态 ====================
/** DFA 中的一个状态：项目集 + 各符号上的转移 */
struct LR1State {
    int id;
    unordered_set<LR1Item> items;
    map<int, int> transitions;   /* 符号 ID → 目标状态 ID */

    LR1State() : id(-1) {}
};

/** 完整的 LR(1) 自动机 */
struct LR1DFA {
    deque<LR1State> states;
    int start_state;
    LR1DFA() : start_state(-1) {}
};

// ==================== 8. 分析表结构 ====================
/** Action 表项类型 */
enum ActionType {
    ACTION_SHIFT,
    ACTION_REDUCE,
    ACTION_ACCEPT,
    ACTION_ERROR
};

/** Action[state][terminal]：移进目标状态 / 归约产生式编号 / 接受 */
struct Action {
    ActionType type;
    int num;
    Action() : type(ACTION_ERROR), num(-1) {}
};

/** 分析表冲突描述（用于 LALR 合并后检测） */
struct ParseConflict {
    enum Kind { SHIFT_REDUCE, REDUCE_REDUCE, SHIFT_SHIFT } kind;
    int state;
    int terminal;
    int shift_state;    /* SHIFT_* */
    int reduce_prod;    /* 归约产生式（shift-reduce 时） */
    int reduce_prod2;   /* 第二条归约产生式（reduce-reduce 时） */
};

/** 归约时查表用的产生式摘要（左部索引、右部长度） */
struct ProdInfo {
    int left;          /* 非终结符在 goto 表中的列索引（已减 NON_TERM_BASE） */
    int right_len;
};

// ==================== 9. 全局变量声明 ====================
extern map<string, int> token_map;           /* 终结符名 → ID */
extern map<string, int> non_term_map;      /* 非终结符名 → ID */
extern map<int, string> symbol_name_map;   /* ID → 显示名 */
extern vector<Production> productions;
extern map<int, Precedence> precedence_map;
extern int origin_start_symbol;            /* 文法 %start 或第一条规则的左部 */
extern int aug_start_symbol;               /* 增广开始符号 S' */
extern map<int, set<int>> first_set;
extern LR1DFA lr_dfa;
extern vector<vector<Action>> action_table;
extern vector<vector<int>> goto_table;
extern vector<ProdInfo> prod_info;
extern string decl_user_code;              /* %{ ... %} 用户代码 */
extern string user_sub_code;             /* 第二个 %% 之后的用户代码 */
extern int yylineno;
extern bool g_debug_enabled;

// ==================== 10. 函数声明 ====================
/** 解析 .y 文件，填充 productions 等全局结构 */
bool parse_yacc_file(const string& filename);
/** 迭代不动点算法计算全体 First 集 */
void compute_first_sets();
/** 计算符号串 β 的 First(β)，用于 closure */
set<int> compute_first_of_string(const vector<int>& syms);
/** 由初始项目构造 LR(1) DFA */
void build_lr1_dfa();
/** 项目集的闭包运算 */
unordered_set<LR1Item> closure(const unordered_set<LR1Item>& items);
/** 项目集在符号 X 上的 Go 函数 */
unordered_set<LR1Item> goto_trans(const unordered_set<LR1Item>& items, int sym);
/** 合并核心相同的 LR(1) 状态为 LALR(1) */
void merge_lalr_dfa();
/** 扫描 DFA 项目集，收集移进-归约 / 归约-归约等冲突 */
vector<ParseConflict> detect_parsing_conflicts(const LR1DFA& dfa);
/** 对比合并前后冲突，向 stderr 输出警告；返回合并新暴露的冲突数 */
int report_lalr_merge_conflicts(const vector<ParseConflict>& pre_merge,
                                const vector<ParseConflict>& post_merge);
/** 根据 DFA 填充 action_table / goto_table；返回未声明优先级而强行消解的冲突数 */
int build_parsing_table();
/** 移进-归约 / 归约-归约冲突消解（优先级与结合性） */
Action resolve_conflict(const Action& cur, const Action& new_act, int token, int prod_id);
/** 生成目标 yyparse.c */
void generate_yyparse_c(const string& output_filename);
void yyerror(const string& msg);
void print_first_set();
void print_lr_dfa();
void print_parsing_table();

void add_token(const string& name, int id);
int add_non_term(const string& name);

/** 查询终结符优先级层次；-1 表示未声明 */
int get_term_precedence(int term);
/** 查询终结符结合性 */
AssocType get_term_assoc(int term);

#endif // YACC_COMMON_H
