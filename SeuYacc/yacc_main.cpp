/*
 * ============================================================================
 * yacc_main.cpp — SeuYacc 主程序
 * ============================================================================
 *
 * 【命令行驱动的六步流水线】
 *   1. parse_yacc_file(input.y)
 *   2. compute_first_sets()
 *   3. build_lr1_dfa()
 *   4. merge_lalr_dfa()        [仅 --lalr]
 *   5. build_parsing_table()
 *   6. generate_yyparse_c(output.cpp)
 *
 * 【调试选项】--print-first / --print-dfa / --print-table 打印中间结果
 * ============================================================================
 */
#include "yacc_common.h"
#include <chrono>

using namespace std;

/* 全局调试开关，由 -v/--verbose 设置 */
bool g_debug_enabled = false;

/** RAII 计时器：析构时在调试模式下打印各阶段耗时（毫秒） */
class Timer {
public:
    Timer(const string& name) : name_(name), start_(chrono::high_resolution_clock::now()) {}
    ~Timer() {
        if (!g_debug_enabled) return;
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start_).count();
        cout << "   [Time] " << name_ << ": " << duration << " ms" << endl;
    }
private:
    string name_;
    chrono::high_resolution_clock::time_point start_;
};

/** 打印 SeuYacc 用法与示例 */
void print_help(const char* program_name) {
    cout << "========================================\n";
    cout << "  SeuYacc - Syntax Parser Generator\n";
    cout << "  Southeast University Compiler Design\n";
    cout << "========================================\n\n";
    cout << "Usage: " << program_name << " [options] <input.y> <output.c>\n\n";
    cout << "Options:\n";
    cout << "  -h, --help       Show this help message\n";
    cout << "  -v, --verbose    Enable verbose debug output\n";
    cout << "  --lalr           Enable LALR(1) optimization (default: LR(1))\n";
    cout << "  --print-first    Print First sets\n";
    cout << "  --print-dfa      Print LR DFA states\n";
    cout << "  --print-table    Print parsing table info\n\n";
    cout << "Examples:\n";
    cout << "  " << program_name << " minic.y yyparse.c\n";
    cout << "  " << program_name << " --lalr --print-dfa minic.y yyparse.c\n";
}

/**
 * 主入口：解析命令行，依次执行六步生成流程
 * @return 0 成功，1 失败
 */
int main(int argc, char* argv[]) {
    string input_file, output_file;
    bool use_lalr = false;
    bool print_first = false;
    bool print_dfa = false;
    bool print_table = false;

    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    /* 解析选项与输入/输出路径 */
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            g_debug_enabled = true;
        } else if (arg == "--lalr") {
            use_lalr = true;
        } else if (arg == "--print-first") {
            print_first = true;
        } else if (arg == "--print-dfa") {
            print_dfa = true;
        } else if (arg == "--print-table") {
            print_table = true;
        } else {
            if (input_file.empty()) input_file = arg;
            else if (output_file.empty()) output_file = arg;
            else {
                cerr << "Error: Extra argument '" << arg << "'\n";
                print_help(argv[0]);
                return 1;
            }
        }
    }

    if (input_file.empty() || output_file.empty()) {
        cerr << "Error: Must specify input file and output file\n";
        print_help(argv[0]);
        return 1;
    }

    cout << "SeuYacc: " << input_file << " -> " << output_file
         << " (" << (use_lalr ? "LALR(1)" : "LR(1)") << ")\n\n";

    try {
        /* [1/6] 读取并解析 Yacc 文法文件 */
        if (!parse_yacc_file(input_file)) {
            cerr << "Failed to parse '" << input_file << "'\n";
            return 1;
        }
        cout << "[1/6] parse: " << productions.size() << " productions, start="
             << symbol_name_map[origin_start_symbol] << "\n";

        /* [2/6] 计算全体符号的 First 集 */
        compute_first_sets();
        cout << "[2/6] First sets ok";
        if (print_first) {
            cout << "\n";
            print_first_set();
        } else {
            cout << "\n";
        }

        /* [3/6] 构造 LR(1) 项目集规范族（DFA） */
        build_lr1_dfa();
        cout << "[3/6] LR(1) DFA: " << lr_dfa.states.size() << " states\n";

        /* [4/6] 可选：按核心合并状态得到 LALR(1) */
        vector<ParseConflict> table_conflicts;
        if (use_lalr) {
            vector<ParseConflict> pre_conflicts = detect_parsing_conflicts(lr_dfa);
            merge_lalr_dfa();
            table_conflicts = detect_parsing_conflicts(lr_dfa);
            int newly_exposed = report_lalr_merge_conflicts(pre_conflicts, table_conflicts);
            cout << "[4/6] LALR merge: " << lr_dfa.states.size() << " states";
            if (!table_conflicts.empty()) {
                cout << ", " << table_conflicts.size() << " conflict(s)";
                if (newly_exposed > 0)
                    cout << " (" << newly_exposed << " newly exposed by merge)";
            }
            cout << "\n";
        } else {
            table_conflicts = detect_parsing_conflicts(lr_dfa);
            cout << "[4/6] LALR skipped";
            if (!table_conflicts.empty())
                cout << ", " << table_conflicts.size() << " LR(1) conflict(s)";
            cout << "\n";
        }
        if (print_dfa) print_lr_dfa();

        /* [5/6] 填表；DFA 冲突可由 %prec 消解，仅未声明优先级者视为失败 */
        int unresolved = build_parsing_table();
        if (unresolved > 0) {
            cerr << "[5/6] parsing table FAILED: " << unresolved
                 << " conflict(s) unresolved by grammar or %prec/%left\n";
            cerr << "Fix the grammar (preferred) or add explicit precedence declarations.\n";
            return 1;
        }
        cout << "[5/6] parsing table ok (0 unresolved";
        if (!table_conflicts.empty())
            cout << ", " << table_conflicts.size() << " resolved by %prec/grammar";
        cout << ")\n";
        if (print_table) print_parsing_table();

        /* [6/6] 输出可编译的 yyparse.c */
        generate_yyparse_c(output_file);
        cout << "[6/6] wrote " << output_file << "\n";
        cout << "SeuYacc done.\n";

    } catch (const exception& e) {
        cerr << "\nFatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
