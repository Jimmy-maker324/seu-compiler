/*
 * ============================================================================
 * grammar.cpp — Yacc 文法文件解析（SeuYacc 第 1 阶段）
 * ============================================================================
 *
 * 【输入】.y 文件（Flex/Bison 兼容子集）
 *
 * 【解析产物（全局变量）】
 *   productions[]  — 产生式列表（含语义动作 C++ 源码字符串）
 *   token_map / non_term_map / symbol_name_map — 符号↔ID 双向表
 *   precedence_map — %left/%right/%nonassoc 优先级
 *   decl_user_code — %{ %} 中用户声明代码
 *
 * 【增广文法】自动插入 S' -> start_symbol，prod_id=0 供接受判定
 *
 * 【extract_action】从规则行 `{ ... }` 中提取语义动作，支持跨行与嵌套括号
 * ============================================================================
 */
#include "yacc_common.h"
#include <cctype>
#include <sstream>

namespace {
    string trim(const string& s);
    vector<string> split(const string& s);
    string extract_action(const string& str, size_t& pos, int& line);
    
    /** 预置与 common_defs.h / 示例 yacc.y 一致的终结符名与 ID 映射 */
    void init_builtin_tokens() {
        token_map["T_EOF"] = T_EOF;
        symbol_name_map[T_EOF] = "T_EOF";

        token_map["T_BREAK"] = T_BREAK;
        token_map["T_CASE"] = T_CASE;
        token_map["T_CHAR"] = T_CHAR;
        token_map["T_CONTINUE"] = T_CONTINUE;
        token_map["T_DEFAULT"] = T_DEFAULT;
        token_map["T_DOUBLE"] = T_DOUBLE;
        token_map["T_ELSE"] = T_ELSE;
        token_map["T_FLOAT"] = T_FLOAT;
        token_map["T_FOR"] = T_FOR;
        token_map["T_IF"] = T_IF;
        token_map["T_INT"] = T_INT;
        token_map["T_RETURN"] = T_RETURN;
        token_map["T_STRUCT"] = T_STRUCT;
        token_map["T_SWITCH"] = T_SWITCH;
        token_map["T_UNION"] = T_UNION;
        token_map["T_VOID"] = T_VOID;
        token_map["T_WHILE"] = T_WHILE;

        token_map["T_IDENT"] = T_IDENTIFIER;
        token_map["T_IDENTIFIER"] = T_IDENTIFIER;
        token_map["T_INTEGER"] = T_CONSTANT;
        token_map["T_CONSTANT"] = T_CONSTANT;
        token_map["T_FLOAT_CONSTANT"] = T_FLOAT_CONSTANT;
        token_map["T_STRING"] = T_STRING_LITERAL;
        token_map["T_STRING_LITERAL"] = T_STRING_LITERAL;

        token_map["T_PTR_OP"] = T_PTR_OP;
        token_map["T_AND_OP"] = T_AND_OP;
        token_map["T_OR_OP"] = T_OR_OP;
        token_map["T_LE_OP"] = T_LE_OP;
        token_map["T_GE_OP"] = T_GE_OP;
        token_map["T_EQ_OP"] = T_EQ_OP;
        token_map["T_NE_OP"] = T_NE_OP;

        token_map["error"] = T_ERROR;
        
        // 单字符运算符映射到 ASCII
        token_map["T_PLUS"] = '+';
        token_map["T_MINUS"] = '-';
        token_map["T_MULT"] = '*';
        token_map["T_DIV"] = '/';
        token_map["T_ASSIGN"] = '=';
        token_map["T_LT"] = '<';
        token_map["T_GT"] = '>';
        token_map["T_NOT"] = '!';
        token_map["T_LPAREN"] = '(';
        token_map["T_RPAREN"] = ')';
        token_map["T_LBRACE"] = '{';
        token_map["T_RBRACE"] = '}';
        token_map["T_LBRACKET"] = '[';
        token_map["T_RBRACKET"] = ']';
        token_map["T_SEMI"] = ';';
        token_map["T_COMMA"] = ',';
        token_map["T_COLON"] = ':';
        token_map["T_DOT"] = '.';
        token_map["T_AMP"] = '&';

        token_map["T_EQ"] = T_EQ_OP;
        token_map["T_NE"] = T_NE_OP;
        token_map["T_LE"] = T_LE_OP;
        token_map["T_GE"] = T_GE_OP;
        token_map["T_AND"] = T_AND_OP;
        token_map["T_OR"] = T_OR_OP;
        
        // 直接字符写法（便于规则中直接写 '+' 而不是 T_PLUS）
        token_map["+"] = '+';
        token_map["-"] = '-';
        token_map["*"] = '*';
        token_map["/"] = '/';
        token_map["="] = '=';
        token_map["<"] = '<';
        token_map[">"] = '>';
        token_map["!"] = '!';
        token_map["("] = '(';
        token_map[")"] = ')';
        token_map["{"] = '{';
        token_map["}"] = '}';
        token_map["["] = '[';
        token_map["]"] = ']';
        token_map[";"] = ';';
        token_map[","] = ',';
        token_map[":"] = ':';
        token_map["."] = '.';
        token_map["&"] = '&';

        for (auto& p : token_map) {
            symbol_name_map[p.second] = p.first;
        }
        // 为直接字符添加更友好的显示名称
        symbol_name_map['+'] = "'+'";
        symbol_name_map['-'] = "'-'";
        symbol_name_map['*'] = "'*'";
        symbol_name_map['/'] = "'/'";
        symbol_name_map['='] = "'='";
        symbol_name_map['<'] = "'<'";
        symbol_name_map['>'] = "'>'";
        symbol_name_map['!'] = "'!'";
        symbol_name_map['('] = "'('";
        symbol_name_map[')'] = "')'";
        symbol_name_map['{'] = "'{'";
        symbol_name_map['}'] = "'}'";
        symbol_name_map['['] = "'['";
        symbol_name_map[']'] = "']'";
        symbol_name_map[';'] = "';'";
        symbol_name_map[','] = "','";
        symbol_name_map[':'] = "':'";
        symbol_name_map['.'] = "'.'";
        symbol_name_map['&'] = "'&'";
    }
    
    /** 逐行解析声明段（%token、优先级、%start 等） */
    bool parse_declarations(const string& decl_section);
    void parse_token_line(const string& line);
    void parse_precedence_line(const string& line, AssocType assoc);
    void parse_start_line(const string& line);
    void parse_union_line(const string& line);
    
    /** 校验右部符号名，防止未正确提取的 { 动作块被当作符号 */
    bool is_valid_symbol_name(const string& name) {
        if (name.empty()) return false;
        // 允许单字符终结符（如 '+', ';', '{', ')'）
        if (name.size() == 1 && ispunct((unsigned char)name[0]))
            return true;
        // 必须以字母或下划线开头，后续可以是字母、数字、下划线
        if (!isalpha(name[0]) && name[0] != '_')
            return false;
        for (char c : name) {
            if (!isalnum(c) && c != '_')
                return false;
        }
        return true;
    }

    /**
     * 从规则段读取一个符号；支持单引号字面量 ')'，避免与产生式结束符 ';' 混淆
     */
    string read_grammar_symbol(const string& rules_str, size_t& pos) {
        string sym;
        if (pos >= rules_str.size()) return sym;
        if (rules_str[pos] == '\'') {
            sym += rules_str[pos++];
            while (pos < rules_str.size()) {
                sym += rules_str[pos];
                if (rules_str[pos] == '\'') {
                    pos++;
                    break;
                }
                pos++;
            }
            return sym;
        }
        while (pos < rules_str.size() && !isspace(rules_str[pos]) &&
               rules_str[pos] != ';' && rules_str[pos] != '|') {
            sym += rules_str[pos];
            pos++;
        }
        return sym;
    }

    /** 去掉 Yacc 单引号字面量外层引号，如 ';' → ; */
    string strip_yacc_quotes(const string& sym) {
        if (sym.size() >= 2 && sym.front() == '\'' && sym.back() == '\'')
            return sym.substr(1, sym.size() - 2);
        return sym;
    }
}

/**
 * 解析完整 .y 文件：声明段 + 规则段 + 用户子程序段，并添加增广文法
 * @return 成功 true，无法打开或缺少 %% 等为 false
 */
bool parse_yacc_file(const string& filename) {
    init_builtin_tokens();
    
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Cannot open file: " << filename << endl;
        return false;
    }
    
    // 读取文件内容（二进制模式，后续手动处理编码）
    string buffer;
    char ch;
    while (file.get(ch)) buffer += ch;
    file.close();
    
    // 去除 UTF-8 BOM（EF BB BF）
    if (buffer.size() >= 3 && (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
        buffer.erase(0, 3);
        cout << "Info: Removed UTF-8 BOM from input file." << endl;
    }
    
    /* 定位 Yacc 三段式分隔符 %% */
    size_t first_percent = buffer.find("%%");
    if (first_percent == string::npos) {
        cerr << "Cannot find first %%" << endl;
        return false;
    }
    size_t second_percent = buffer.find("%%", first_percent + 2);
    if (second_percent == string::npos) {
        cerr << "Cannot find second %%" << endl;
        return false;
    }
    
    // 提取声明段（第一个 %% 之前的内容）
    string decl_section = buffer.substr(0, first_percent);
    
    // 收集用户代码段 %{ ... %}
    decl_user_code.clear();
    size_t decl_pos = 0;
    while (decl_pos < decl_section.size()) {
        if (decl_section[decl_pos] == '%' && decl_pos+1 < decl_section.size() && decl_section[decl_pos+1] == '{') {
            size_t end = decl_section.find("%}", decl_pos);
            if (end != string::npos) {
                decl_user_code += decl_section.substr(decl_pos+2, end-decl_pos-2) + "\n";
                decl_pos = end + 2;
                continue;
            }
        }
        decl_pos++;
    }
    
    if (!parse_declarations(decl_section)) {
        cerr << "Failed to parse declaration section" << endl;
        return false;
    }
    
    // 提取规则段（两个 %% 之间）
    string rules_str = buffer.substr(first_percent + 2, second_percent - first_percent - 2);
    DEBUG_PRINT("Rules section length: " << rules_str.size());
    
    // 解析规则
    size_t pos = 0;
    string current_left;
    int line = 1;
    
    auto skip = [&]() {
        while (pos < rules_str.size()) {
            char c = rules_str[pos];
            if (c == '\n') line++;
            if (isspace(c)) { pos++; continue; }
            if (c == '/' && pos+1 < rules_str.size() && rules_str[pos+1] == '/') {
                while (pos < rules_str.size() && rules_str[pos] != '\n') pos++;
                continue;
            }
            if (c == '/' && pos+1 < rules_str.size() && rules_str[pos+1] == '*') {
                pos += 2;
                while (pos+1 < rules_str.size() && !(rules_str[pos] == '*' && rules_str[pos+1] == '/')) {
                    if (rules_str[pos] == '\n') line++;
                    pos++;
                }
                pos += 2;
                continue;
            }
            break;
        }
    };
    
    while (pos < rules_str.size()) {
        skip();
        if (pos >= rules_str.size()) break;
        
        // 跳过以 % 开头的行（如 %prec, %type, %left, %right, %nonassoc）
        if (rules_str[pos] == '%') {
            while (pos < rules_str.size() && rules_str[pos] != '\n') pos++;
            if (pos < rules_str.size()) pos++; // 跳过换行
            continue;
        }
        
        // 读取左部（直到遇到 ':'）
        string left;
        while (pos < rules_str.size() && rules_str[pos] != ':') {
            if (!isspace(rules_str[pos])) left += rules_str[pos];
            pos++;
            skip();
            if (pos >= rules_str.size()) break;
        }
        if (pos >= rules_str.size()) break;
        if (rules_str[pos] == ':') pos++; // 跳过 ':'
        
        left = trim(left);
        if (left.empty()) left = current_left;
        else {
            current_left = left;
            if (origin_start_symbol == -1) {
                origin_start_symbol = add_non_term(left);
                DEBUG_PRINT("Start symbol: " << left);
            }
        }
        
        // 解析该左部的所有候选式（直到遇到 ';'）
        while (true) {
            skip();
            if (pos >= rules_str.size() || rules_str[pos] == ';') {
                if (pos < rules_str.size() && rules_str[pos] == ';') pos++;
                break;
            }
            
            string right_part;
            string semantic_action;
            int pending_prec_token = -1;
            semantic_action.clear();
            
            // 读取右部符号或动作
            while (pos < rules_str.size() && rules_str[pos] != ';' && rules_str[pos] != '|') {
                skip();
                if (pos >= rules_str.size() || rules_str[pos] == ';' || rules_str[pos] == '|') break;
                
                // 检测语义动作（'{' 作为动作开始）
                if (rules_str[pos] == '{') {
                    semantic_action = extract_action(rules_str, pos, line);
                    if (semantic_action.empty()) {
                        // 提取失败，跳过本候选式剩余部分，直到遇到 ';' 或 '|'
                        while (pos < rules_str.size() && rules_str[pos] != ';' && rules_str[pos] != '|')
                            pos++;
                        if (pos < rules_str.size() && rules_str[pos] == '|') pos++;
                        semantic_action.clear();
                        break; // 跳出内部 while，进入下一候选式处理
                    }
                    skip();
                    if (pos >= rules_str.size() || rules_str[pos] == ';' || rules_str[pos] == '|') break;
                    continue;
                }
                
                // 读取一个符号（可能是终结符或非终结符）
                string sym = read_grammar_symbol(rules_str, pos);
                if (!sym.empty()) {
                    if (sym == "%prec") {
                        skip();
                        string prec_sym = read_grammar_symbol(rules_str, pos);
                        string prec_name = strip_yacc_quotes(prec_sym);
                        if (token_map.count(prec_name))
                            pending_prec_token = token_map[prec_name];
                        continue;
                    }
                    string sym_name = strip_yacc_quotes(sym);
                    // 合法性检查：如果符号名以 '{' 开头或包含非法字符，则报错并忽略（说明动作提取失败）
                    if (!is_valid_symbol_name(sym_name)) {
                        cerr << "Error: Invalid symbol name '" << sym << "' at line " << line 
                             << " (possibly unextracted semantic action). Skipping this alternative." << endl;
                        // 跳过整个候选式
                        while (pos < rules_str.size() && rules_str[pos] != ';' && rules_str[pos] != '|')
                            pos++;
                        semantic_action.clear();
                        break;
                    }
                    if (!right_part.empty()) right_part += " ";
                    right_part += sym_name;
                }
            }
            
            // 构建产生式
            if (!left.empty() && (!right_part.empty() || !semantic_action.empty())) {
                Production prod;
                prod.left = add_non_term(left);
                vector<string> syms = split(right_part);
                for (const string& s : syms) {
                    if (s.empty()) continue;
                    int id = -1;
                    if (token_map.count(s)) {
                        id = token_map[s];
                    } else {
                        // 未识别的符号 -> 非终结符
                        id = add_non_term(s);
                    }
                    if (id != -1) prod.right.push_back(id);
                    DEBUG_PRINT("Symbol: " << s << " -> ID=" << id << " (Terminal=" << IS_TERMINAL(id) << ")");
                }
                prod.semantic_action = semantic_action;
                prod.prec_token = pending_prec_token;
                if (pending_prec_token >= 0) {
                    auto it = precedence_map.find(pending_prec_token);
                    if (it != precedence_map.end())
                        prod.prec_level = it->second.level;
                }
                pending_prec_token = -1;
                productions.push_back(prod);
                DEBUG_PRINT("Add prod: " << left << " -> " << right_part);
            }
            
            // 检查下一个字符是 '|' 还是 ';'
            skip();
            if (pos < rules_str.size() && rules_str[pos] == '|') {
                pos++;
                continue;
            } else if (pos < rules_str.size() && rules_str[pos] == ';') {
                pos++;
                break;
            } else {
                break;
            }
        }
    }
    
    // 用户子程序段
    user_sub_code = buffer.substr(second_percent + 2);
    
    /* 在 productions 首部插入 S' -> start */
    string aug_name = "S'";
    aug_start_symbol = add_non_term(aug_name);
    Production aug_prod;
    aug_prod.left = aug_start_symbol;
    aug_prod.right.push_back(origin_start_symbol);
    productions.insert(productions.begin(), aug_prod);
    DEBUG_PRINT("Augmented grammar added, total productions: " << productions.size());
    
    // 打印所有产生式（调试）
    for (size_t i = 0; i < productions.size(); i++) {
        Production& p = productions[i];
        DEBUG_PRINT("Prod " << i << ": " << symbol_name_map[p.left] << " ->");
        for (int sym : p.right) {
            DEBUG_PRINT(" " << symbol_name_map[sym] << "(" << sym << (IS_TERMINAL(sym)?" T":" NT") << ")");
        }
        if (p.right.empty()) DEBUG_PRINT(" ε");
    }
    
    return true;
}

/* ========== 声明段解析 ========== */
namespace {
    bool parse_declarations(const string& decl_section) {
        istringstream iss(decl_section);
        string line;
        int line_num = 0;
        
        while (getline(iss, line)) {
            line_num++;

            size_t comment_pos = line.find("//");
            if (comment_pos != string::npos)
                line = line.substr(0, comment_pos);

            // 去除首尾空白
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            line = line.substr(start, end - start + 1);
            
            // 跳过 C 风格注释
            if (line.find("/*") != string::npos) continue;
            if (line.substr(0, 2) == "//") continue;
            
            if (line.substr(0, 6) == "%token") {
                parse_token_line(line);
            }
            else if (line.substr(0, 5) == "%left") {
                parse_precedence_line(line, ASSOC_LEFT);
            }
            else if (line.substr(0, 6) == "%right") {
                parse_precedence_line(line, ASSOC_RIGHT);
            }
            else if (line.substr(0, 9) == "%nonassoc") {
                parse_precedence_line(line, ASSOC_NONASSOC);
            }
            else if (line.substr(0, 6) == "%start") {
                parse_start_line(line);
            }
            else if (line.substr(0, 6) == "%union") {
                parse_union_line(line);
            }
            // 其他 % 开头的行忽略
        }
        return true;
    }
    
    /** 解析 %token [ <tag> ] name ...，动态分配未预置的记号 ID */
    void parse_token_line(const string& line) {
        stringstream ss(line);
        string tmp;
        ss >> tmp; // "%token"
        string type_tag;
        if (ss.peek() == '<') {
            ss >> type_tag;
        }
        string token_name;
        while (ss >> token_name) {
            if (token_name == "//" || token_name.substr(0,2) == "//")
                break;
            // 去除单引号（如 '[' -> [）
            if (token_name.size() >= 2 && token_name.front() == '\'' && token_name.back() == '\'') {
                token_name = token_name.substr(1, token_name.size() - 2);
            }
            // 允许任何非纯数字的 token 名（包括宏名和直接字符）
            if (token_name.size() == 1 && ispunct(token_name[0])) {
                // 单字符 token，已经在 init_builtin_tokens 中映射
                if (token_map.find(token_name) == token_map.end()) {
                    token_map[token_name] = (int)token_name[0];
                    symbol_name_map[(int)token_name[0]] = token_name;
                }
            } else if (token_name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == string::npos) {
                if (token_map.find(token_name) == token_map.end()) {
                    static int next_dynamic = 350;
                    int id = next_dynamic++;
                    token_map[token_name] = id;
                    symbol_name_map[id] = token_name;
                    DEBUG_PRINT("Added dynamic token: " + token_name << " = " << id);
                }
            }
        }
    }
    
    /** 解析 %left/%right/%nonassoc，每行递增优先级层次 */
    void parse_precedence_line(const string& line, AssocType assoc) {
        static int current_level = 0;
        current_level++;
        stringstream ss(line);
        string tmp;
        ss >> tmp; // "%left"/...
        if (ss.peek() == '<') {
            ss >> tmp; // 跳过 <...>
        }
        string token_name;
        while (ss >> token_name) {
            if (token_name == "//" || token_name.substr(0,2) == "//")
                break;
            if (token_name.size() == 1 && ispunct(token_name[0])) {
                if (token_map.find(token_name) == token_map.end()) {
                    token_map[token_name] = (int)token_name[0];
                    symbol_name_map[(int)token_name[0]] = token_name;
                }
            } else if (token_name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == string::npos) {
                if (token_map.find(token_name) == token_map.end()) {
                    static int next_dynamic = 350;
                    int id = next_dynamic++;
                    token_map[token_name] = id;
                    symbol_name_map[id] = token_name;
                    DEBUG_PRINT("Added dynamic token in precedence: " << token_name << " = " << id);
                }
            } else {
                continue;
            }
            int token_id = token_map[token_name];
            precedence_map[token_id] = {current_level, assoc};
        }
    }
    
    /** 解析 %start，覆盖默认的“第一条规则左部” */
    void parse_start_line(const string& line) {
        stringstream ss(line);
        string tmp, start_sym;
        ss >> tmp;
        ss >> start_sym;
        if (!start_sym.empty()) {
            int sym_id;
            if (non_term_map.count(start_sym)) {
                sym_id = non_term_map[start_sym];
            } else {
                sym_id = add_non_term(start_sym);
            }
            origin_start_symbol = sym_id;
            DEBUG_PRINT("Explicit start symbol: " << start_sym);
        }
    }
    
    /** %union 忽略：统一使用 common_defs.h 中的 YYSTYPE */
    void parse_union_line(const string& line) {
        DEBUG_PRINT("Ignoring %union definition (using predefined YYSTYPE)");
    }
}

// 辅助函数实现
namespace {
    string trim(const string& s) {
        if (s.empty()) return s;
        size_t st = s.find_first_not_of(" \t\n\r");
        size_t ed = s.find_last_not_of(" \t\n\r");
        return st == string::npos ? "" : s.substr(st, ed - st + 1);
    }
    
    vector<string> split(const string& s) {
        vector<string> res;
        stringstream ss(s);
        string w;
        while (ss >> w) res.push_back(w);
        return res;
    }
    
    /**
     * 提取 { ... } 语义动作原文：处理字符串、注释、嵌套花括号与 C++ 尖括号
     */
    string extract_action(const string& str, size_t& pos, int& line) {
        if (pos >= str.size() || str[pos] != '{')
            return "";
        size_t start = pos;
        int start_line = line;
        int brace = 1;
        int angle = 0;  // 用于处理尖括号 < >
        pos++; // 跳过 '{'
        bool in_string = false;
        bool in_char = false;
        bool in_block_comment = false;
        bool escape = false;

        while (pos < str.size() && brace > 0) {
            char c = str[pos];
            if (c == '\n') line++;
            
            if (escape) {
                escape = false;
                pos++;
                continue;
            }
            if (c == '\\') {
                escape = true;
                pos++;
                continue;
            }
            
            // 处理块注释
            if (!in_string && !in_char && !in_block_comment && c == '/' && pos+1 < str.size() && str[pos+1] == '*') {
                in_block_comment = true;
                pos += 2;
                continue;
            }
            if (in_block_comment) {
                if (c == '*' && pos+1 < str.size() && str[pos+1] == '/') {
                    in_block_comment = false;
                    pos += 2;
                } else {
                    pos++;
                }
                continue;
            }
            
            // 处理行注释（在动作内部，行注释会一直持续到换行）
            if (!in_string && !in_char && !in_block_comment && c == '/' && pos+1 < str.size() && str[pos+1] == '/') {
                while (pos < str.size() && str[pos] != '\n') pos++;
                if (pos < str.size() && str[pos] == '\n') line++;
                continue;
            }
            
            if (in_string) {
                if (c == '"') in_string = false;
                pos++;
                continue;
            }
            if (in_char) {
                if (c == '\'') in_char = false;
                pos++;
                continue;
            }
            if (c == '"') {
                in_string = true;
                pos++;
                continue;
            }
            if (c == '\'') {
                in_char = true;
                pos++;
                continue;
            }
            if (c == '<') {
                angle++;
                pos++;
                continue;
            }
            if (c == '>') {
                if (angle > 0) angle--;
                // C++11 嵌套模板右尖括号 >>
                if (angle > 0 && pos + 1 < str.size() && str[pos + 1] == '>')
                    angle--;
                pos++;
                continue;
            }
            if (c == '{' && angle == 0) {
                brace++;
                pos++;
                continue;
            }
            if (c == '}' && angle == 0) {
                brace--;
                pos++;
                continue;
            }
            pos++;
        }

        if (brace != 0) {
            cerr << "Error: unmatched braces in semantic action starting at line " << start_line << endl;
            // 尝试跳过错误的动作，将 pos 移动到下一个 ';' 或 '|'
            while (pos < str.size() && str[pos] != ';' && str[pos] != '|')
                pos++;
            return "";
        }

        return str.substr(start, pos - start);
    }
}