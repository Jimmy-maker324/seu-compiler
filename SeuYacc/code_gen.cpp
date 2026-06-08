/*
 * ============================================================================
 * code_gen.cpp — 生成 yyparse.cpp（SeuYacc 第 6 阶段）
 * ============================================================================
 *
 * 【生成内容】
 *   - 稀疏压缩 action/goto 表（按状态仅存非默认项）+ lookup 函数
 *   - prod_info[]：归约产生式左部索引与右部长度
 *   - yyparse()：LR 分析主循环 + 语义动作执行
 *
 * 【分析表压缩】
 *   稠密 action[state][term][2] 绝大多数为 ERROR；改为每行 (sym,type,num) 列表 + row 偏移
 *   goto 同理，仅保留非 -1 转移
 *
 * 【yyparse 运行时数据结构（生成代码中）】
 *   state_stack[] — 状态栈
 *   val_stack[]   — 语义值栈（YYSTYPE，存 AST 指针等）
 *   sp            — 栈顶指针
 *
 * 【主循环流程】
 *   1. tok = yylex()；查 lookup_action(state, tok)
 *   2. SHIFT：压入 (新状态, yylval)，继续
 *   3. REDUCE：按 prod_info 弹 |β| 个状态与值，执行语义动作 C++ 代码，
 *              lookup_goto(栈顶, A) 入新状态，压入归约结果 $$
 *   4. ACCEPT：成功返回 0
 *
 * 【translate_semantic_action】
 *   $n  → val_stack[sp - (right_len - n)]  （第 n 个符号的语义值）
 *   $$  → val_stack[sp]                     （归约结果槽，动作执行后写入）
 * ============================================================================
 */
#include "yacc_common.h"
#include <iomanip>
#include <sstream>
#include <regex>
#include <vector>

namespace {
    void generate_header(ofstream& out);
    void generate_yystype(ofstream& out);
    void generate_tables(ofstream& out);
    void generate_yyparse(ofstream& out);
    void generate_yyerror(ofstream& out);
    void generate_footer(ofstream& out);
    string escape_string(const string& s);

    int action_type_code(ActionType type) {
        switch (type) {
            case ACTION_SHIFT: return 1;
            case ACTION_REDUCE: return 2;
            case ACTION_ACCEPT: return 3;
            default: return 4;
        }
    }

    /**
     * 将 Yacc 语义动作中的 $n、$$ 替换为 val_stack[sp + n-1]、val_stack[sp]
     * 字符串/字符字面量及 C 注释内的 $ 不替换
     */
    string translate_semantic_action(const string& action, int right_len) {
        string result;
        size_t len = action.size();
        size_t i = 0;
        bool in_string = false;
        bool in_char = false;
        bool in_block_comment = false;
        bool escape = false;

        while (i < len) {
            char c = action[i];
            
            if (escape) {
                result += c;
                escape = false;
                i++;
                continue;
            }
            if (c == '\\' && (in_string || in_char)) {
                result += c;
                escape = true;
                i++;
                continue;
            }

            if (!in_string && !in_char && !in_block_comment
                && c == '/' && i + 1 < len && action[i + 1] == '*') {
                in_block_comment = true;
                result += "/*";
                i += 2;
                continue;
            }
            if (in_block_comment) {
                result += c;
                if (c == '*' && i + 1 < len && action[i + 1] == '/') {
                    result += '/';
                    in_block_comment = false;
                    i += 2;
                } else {
                    i++;
                }
                continue;
            }
            if (!in_string && !in_char
                && c == '/' && i + 1 < len && action[i + 1] == '/') {
                while (i < len) {
                    result += action[i];
                    if (action[i] == '\n') { i++; break; }
                    i++;
                }
                continue;
            }

            if (c == '"' && !in_char) {
                in_string = !in_string;
                result += c;
                i++;
                continue;
            }
            if (c == '\'' && !in_string) {
                in_char = !in_char;
                result += c;
                i++;
                continue;
            }
            if ((c == '$') && !in_string && !in_char && i+1 < len && isdigit(action[i+1])) {
                i++; // 跳过 $
                int n = 0;
                while (i < len && isdigit(action[i])) {
                    n = n * 10 + (action[i] - '0');
                    i++;
                }
                if (n >= 1 && n <= right_len) {
                    result += "val_stack[sp + " + to_string(n - 1) + "]";
                } else {
                    result += "$" + to_string(n);   // 保留原样作为错误提示
                }
                continue;
            }
            if (c == '$' && !in_string && !in_char && i+1 < len && action[i+1] == '$') {
                result += "val_stack[sp]";
                i += 2;
                continue;
            }
            result += c;
            i++;
        }
        return result;
    }
}

/** 依次生成头、类型声明、分析表、yyparse、yyerror 与用户尾段 */
void generate_yyparse_c(const string& output_filename) {
    DEBUG_PRINT("Start generating " << output_filename << "...");
    ofstream out(output_filename);
    if (!out.is_open()) {
        cerr << "Failed to open output file: " << output_filename << endl;
        return;
    }

    generate_header(out);
    generate_yystype(out);
    generate_tables(out);
    generate_yyparse(out);
    generate_yyerror(out);
    generate_footer(out);

    out.close();
    DEBUG_PRINT(output_filename << " generated successfully!");
}

namespace {
    /** 文件头、#include、yyerrok 宏与 decl_user_code */
    void generate_header(ofstream& out) {
        out << "/* =========================================\n";
        out << "   This file is automatically generated by SeuYacc\n";
        out << "   Compiler Principles Course Design - Southeast University\n";
        out << "   ========================================= */\n\n";

        out << "#include <stdio.h>\n";
        out << "#include <stdlib.h>\n";
        out << "#include <string.h>\n";
        out << "#include <memory>\n";
        out << "#include <vector>\n";
        out << "#include \"common_defs.h\"\n";
        out << "#include \"seulex.h\"\n";
        out << "#include \"ast.h\"\n";
        out << "#include \"symbol.h\"\n";
        out << "#include \"type.h\"\n\n";

        out << "/* Error recovery macros (Bison-compatible) */\n";
        out << "#define yyerrok    (yyerrstatus = 0)\n";
        out << "#define yyclearin  (token_valid = 0)\n\n";

        out << "/* --- User declaration section code --- */\n";
        if (!decl_user_code.empty()) {
            out << decl_user_code << "\n";
        }
        out << "\n/* --- Function declarations --- */\n";
        out << "void yyerror(const char* msg);\n\n";
    }

    /** 声明 extern yylval（类型已在 common_defs.h） */
    void generate_yystype(ofstream& out) {
        out << "/* --- Semantic value type definition --- */\n";
        out << "extern YYSTYPE yylval;\n\n";
    }

    /** 输出稀疏 action/goto 表、lookup 函数与 prod_info */
    void generate_tables(ofstream& out) {
        out << "/* --- LR parsing table definitions --- */\n";
        out << "#define ACTION_SHIFT  1\n";
        out << "#define ACTION_REDUCE 2\n";
        out << "#define ACTION_ACCEPT 3\n";
        out << "#define ACTION_ERROR  4\n\n";

        int num_states = (int)action_table.size();
        int num_terms = action_table.empty() ? 0 : (int)action_table[0].size();
        int num_non_terms = goto_table.empty() ? 0 : (int)goto_table[0].size();

        struct ActionEntry { int sym; int type; int num; };
        struct GotoEntry { int sym; int state; };
        vector<ActionEntry> action_entries;
        vector<int> action_row(num_states + 1, 0);
        vector<GotoEntry> goto_entries;
        vector<int> goto_row(num_states + 1, 0);

        for (int i = 0; i < num_states; i++) {
            action_row[i] = (int)action_entries.size();
            for (int j = 0; j < num_terms; j++) {
                Action& a = action_table[i][j];
                if (a.type != ACTION_ERROR) {
                    action_entries.push_back({
                        j, action_type_code(a.type), a.num >= 0 ? a.num : -1
                    });
                }
            }
        }
        action_row[num_states] = (int)action_entries.size();

        for (int i = 0; i < num_states; i++) {
            goto_row[i] = (int)goto_entries.size();
            for (int j = 0; j < num_non_terms; j++) {
                int next = goto_table[i][j];
                if (next >= 0) {
                    goto_entries.push_back({ j, next });
                }
            }
        }
        goto_row[num_states] = (int)goto_entries.size();

        int dense_action_cells = num_states * num_terms * 2;
        int sparse_action_ints = (int)action_entries.size() * 3 + (num_states + 1);
        int dense_goto_cells = num_states * num_non_terms;
        int sparse_goto_ints = (int)goto_entries.size() * 2 + (num_states + 1);

        out << "/* Action table (sparse): " << action_entries.size() << " entries, "
            << "dense " << dense_action_cells << " ints -> "
            << sparse_action_ints << " ints\n";
        out << "   Goto table (sparse): " << goto_entries.size() << " entries, "
            << "dense " << dense_goto_cells << " ints -> "
            << sparse_goto_ints << " ints */\n\n";

        out << "typedef struct { int sym; int type; int num; } ActionEntry;\n";
        out << "static const ActionEntry action_entries[" << action_entries.size() << "] = {\n";
        for (size_t i = 0; i < action_entries.size(); i++) {
            const ActionEntry& e = action_entries[i];
            out << "    {" << e.sym << "," << e.type << "," << e.num << "}";
            if (i + 1 != action_entries.size()) out << ",";
            out << "\n";
        }
        out << "};\n\n";

        out << "static const int action_row[" << num_states + 1 << "] = {\n    ";
        for (int i = 0; i <= num_states; i++) {
            out << action_row[i];
            if (i != num_states) out << ",";
            if (i != num_states && (i + 1) % 16 == 0) out << "\n    ";
        }
        out << "\n};\n\n";

        out << "static void lookup_action(int state, int token, int* act_type, int* act_num) {\n";
        out << "    int lo = action_row[state], hi = action_row[state + 1];\n";
        out << "    for (int i = lo; i < hi; i++) {\n";
        out << "        if (action_entries[i].sym == token) {\n";
        out << "            *act_type = action_entries[i].type;\n";
        out << "            *act_num = action_entries[i].num;\n";
        out << "            return;\n";
        out << "        }\n";
        out << "    }\n";
        out << "    *act_type = ACTION_ERROR;\n";
        out << "    *act_num = -1;\n";
        out << "}\n\n";

        out << "typedef struct { int sym; int state; } GotoEntry;\n";
        out << "static const GotoEntry goto_entries[" << goto_entries.size() << "] = {\n";
        for (size_t i = 0; i < goto_entries.size(); i++) {
            const GotoEntry& e = goto_entries[i];
            out << "    {" << e.sym << "," << e.state << "}";
            if (i + 1 != goto_entries.size()) out << ",";
            out << "\n";
        }
        out << "};\n\n";

        out << "static const int goto_row[" << num_states + 1 << "] = {\n    ";
        for (int i = 0; i <= num_states; i++) {
            out << goto_row[i];
            if (i != num_states) out << ",";
            if (i != num_states && (i + 1) % 16 == 0) out << "\n    ";
        }
        out << "\n};\n\n";

        out << "static int lookup_goto(int state, int nonterm) {\n";
        out << "    int lo = goto_row[state], hi = goto_row[state + 1];\n";
        out << "    for (int i = lo; i < hi; i++) {\n";
        out << "        if (goto_entries[i].sym == nonterm)\n";
        out << "            return goto_entries[i].state;\n";
        out << "    }\n";
        out << "    return -1;\n";
        out << "}\n\n";

        out << "/* Production info: [production ID] = {left-hand non-terminal index, right-hand length} */\n";
        out << "static int prod_info[" << prod_info.size() << "][2] = {\n";
        for (size_t i = 0; i < prod_info.size(); i++) {
            out << "    {" << prod_info[i].left << "," << prod_info[i].right_len << "}";
            if (i != prod_info.size() - 1) out << ",";
            out << "\n";
        }
        out << "};\n\n";
    }

    /** 生成标准 LR 分析主循环：移进、归约（含各产生式语义动作）、接受、报错 */
    void generate_yyparse(ofstream& out) {
        out << "/* --- LR main control program --- */\n";
        out << "int yyparse(void) {\n";
        out << "    /* Stack definitions */\n";
        out << "    int state_stack[STACK_SIZE];\n";
        out << "    int sym_stack[STACK_SIZE];\n";
        out << "    YYSTYPE val_stack[STACK_SIZE];\n";
        out << "    int sp = 0; // Stack pointer\n";
        out << "    int yyerrstatus = 0;   /* 0=正常, >0=错误恢复中（抑制重复报错） */\n";
        out << "    int yyhad_error = 0;   /* 是否出现过语法错误 */\n";
        out << "    int token = 0;\n";
        out << "    int token_valid = 0;   /* 0 表示需读取新记号（yyclearin） */\n\n";

        out << "    /* Initialization */\n";
        out << "    state_stack[sp] = 0;\n";
        out << "    sym_stack[sp] = 0;\n";
        out << "    sp++;\n\n";

        out << "    /* Main loop */\n";
        out << "    while (1) {\n";
        out << "        if (sp >= STACK_SIZE) {\n";
        out << "            yyerror(\"Stack overflow\");\n";
        out << "            return 1;\n";
        out << "        }\n";
        out << "        if (!token_valid) {\n";
        out << "            token = yylex();\n";
        out << "            token_valid = 1;\n";
        out << "        }\n";
        out << "        int cur_state = state_stack[sp - 1];\n";
        out << "        if (token < 0) token = -token;\n";
        out << "        if (token >= 1000) {\n";
        out << "            yyerror(\"Token index out of range\");\n";
        out << "            return 1;\n";
        out << "        }\n";
        out << "        int act_type = ACTION_ERROR;\n";
        out << "        int act_num = -1;\n";
        out << "        lookup_action(cur_state, token, &act_type, &act_num);\n\n";

        out << "        switch (act_type) {\n";
        out << "            case ACTION_SHIFT:\n";
        out << "                if (yyerrstatus > 0) yyerrstatus--;\n";
        out << "                state_stack[sp] = act_num;\n";
        out << "                sym_stack[sp] = token;\n";
        out << "                val_stack[sp].ival = 0;\n";
        out << "                val_stack[sp].fval = 0.0;\n";
        out << "                val_stack[sp].sval = nullptr;\n";
        out << "                val_stack[sp].ptr = nullptr;\n";
        out << "                if (token == T_IDENTIFIER || token == T_STRING_LITERAL) {\n";
        out << "                    val_stack[sp].sval = yylval.sval;\n";
        out << "                } else if (token == T_CONSTANT) {\n";
        out << "                    val_stack[sp].ival = yylval.ival;\n";
        out << "                } else if (token == T_FLOAT_CONSTANT) {\n";
        out << "                    val_stack[sp].fval = yylval.fval;\n";
        out << "                }\n";
        out << "                sp++;\n";
        out << "                token_valid = 0;\n";
        out << "                break;\n\n";

        out << "            case ACTION_REDUCE:\n";
        out << "                {\n";
        out << "                    int prod_id = act_num;\n";
        out << "                    int left_idx = prod_info[prod_id][0];\n";
        out << "                    int right_len = prod_info[prod_id][1];\n\n";
        out << "                    /* Pop stack */\n";
        out << "                    sp -= right_len;\n";
        out << "                    if (sp < 1) {\n";
        out << "                        yyerror(\"Parser stack underflow\");\n";
        out << "                        return 1;\n";
        out << "                    }\n\n";
        out << "                    /* Semantic action */\n";
        out << "                    {\n";
        for (size_t i = 0; i < productions.size(); i++) {
            if (!productions[i].semantic_action.empty()) {
                out << "                        if (prod_id == " << i << ") {\n";
                string translated = translate_semantic_action(productions[i].semantic_action,
                                                               productions[i].right.size());
                istringstream iss(translated);
                string line;
                while (getline(iss, line)) {
                    out << "                            " << line << "\n";
                }
                out << "                        }\n";
            }
        }
        out << "                    }\n\n";
        out << "                    /* Look up Goto table */\n";
        out << "                    int prev_state = state_stack[sp - 1];\n";
        out << "                    int next_state = lookup_goto(prev_state, left_idx);\n";
        out << "                    if (next_state < 0) {\n";
        out << "                        yyerror(\"Goto table error\");\n";
        out << "                        return 1;\n";
        out << "                    }\n\n";
        out << "                    /* Push new state and non-terminal */\n";
        out << "                    state_stack[sp] = next_state;\n";
        out << "                    sym_stack[sp] = left_idx + " << NON_TERM_BASE << ";\n";
        out << "                    sp++;\n";
        out << "                }\n";
        out << "                break;\n\n";

        out << "            case ACTION_ACCEPT:\n";
        out << "                return 0;\n\n";
        out << "            case ACTION_ERROR:\n";
        out << "            default:\n";
        out << "                /* Panic-mode：弹栈直至可移进 error，再局部恢复 */\n";
        out << "                if (yyerrstatus == 0) {\n";
        out << "                    yyerror(\"Syntax error\");\n";
        out << "                    yyhad_error = 1;\n";
        out << "                }\n";
        out << "                yyerrstatus = 3;\n";
        out << "                {\n";
        out << "                    int target_sp = sp;\n";
        out << "                    int shifted_error = 0;\n";
        out << "                    while (target_sp > 1) {\n";
        out << "                        int err_state = state_stack[target_sp - 1];\n";
        out << "                        int err_act_type = ACTION_ERROR, err_act_num = -1;\n";
        out << "                        lookup_action(err_state, T_ERROR, &err_act_type, &err_act_num);\n";
        out << "                        if (err_act_type == ACTION_SHIFT) {\n";
        out << "                            shifted_error = 1;\n";
        out << "                            break;\n";
        out << "                        }\n";
        out << "                        target_sp--;\n";
        out << "                    }\n";
        out << "                    if (!shifted_error) {\n";
        out << "                        if (token == 0) return 1;\n";
        out << "                        token_valid = 0;\n";
        out << "                        break;\n";
        out << "                    }\n";
        out << "                    while (sp > target_sp) sp--;\n";
        out << "                    {\n";
        out << "                        int err_state = state_stack[sp - 1];\n";
        out << "                        int err_act_type = ACTION_ERROR, next_err = -1;\n";
        out << "                        lookup_action(err_state, T_ERROR, &err_act_type, &next_err);\n";
        out << "                        state_stack[sp] = next_err;\n";
        out << "                        sym_stack[sp] = T_ERROR;\n";
        out << "                        val_stack[sp].ival = 0;\n";
        out << "                        val_stack[sp].sval = nullptr;\n";
        out << "                        val_stack[sp].ptr = nullptr;\n";
        out << "                        sp++;\n";
        out << "                    }\n";
        out << "                    /* 丢弃输入直至当前状态可继续（同步到 error 产生式后续符号） */\n";
        out << "                    while (token_valid) {\n";
        out << "                        int sync_state = state_stack[sp - 1];\n";
        out << "                        if (token == 0) break;\n";
        out << "                        int sync_act = ACTION_ERROR, sync_num = -1;\n";
        out << "                        lookup_action(sync_state, token, &sync_act, &sync_num);\n";
        out << "                        if (sync_act == ACTION_SHIFT || sync_act == ACTION_REDUCE\n";
        out << "                            || sync_act == ACTION_ACCEPT) {\n";
        out << "                            break;\n";
        out << "                        }\n";
        out << "                        token_valid = 0;\n";
        out << "                    }\n";
        out << "                }\n";
        out << "                /* 保留当前输入记号，等待 error 产生式归约同步 */\n";
        out << "                break;\n";
        out << "        }\n";
        out << "    }\n";
        out << "}\n\n";
    }

    /** 若用户未在 %% 后提供 yyerror，则生成默认实现 */
    void generate_yyerror(ofstream& out) {
        if (user_sub_code.find("yyerror") == string::npos) {
            out << "/* --- Default error handling function --- */\n";
            out << "void yyerror(const char* msg) {\n";
            out << "    fprintf(stderr, \"Syntax error at line %d: %s\\n\", yylineno, msg);\n";
            out << "}\n\n";
        }
    }

    /** 追加第二个 %% 之后的用户子程序代码 */
    void generate_footer(ofstream& out) {
        out << "/* --- User subroutine code --- */\n";
        if (!user_sub_code.empty()) {
            out << user_sub_code << "\n";
        }
        out << "\n";
    }

    string escape_string(const string& s) {
        string res;
        for (char c : s) {
            switch (c) {
                case '\\': res += "\\\\"; break;
                case '\"': res += "\\\""; break;
                case '\n': res += "\\n"; break;
                case '\t': res += "\\t"; break;
                default: res += c;
            }
        }
        return res;
    }
}