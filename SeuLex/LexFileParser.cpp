/*
 * ============================================================================
 * LexFileParser.cpp — Flex .l 文件解析与 SeuLex 完整流水线
 * ============================================================================
 *
 * 【.l 文件三段式结构】
 *   定义区（第一个 %% 前）：%{ %} 用户 C 代码 + 命名宏 name regex
 *   规则区（两个 %% 之间）：pattern { action } 词法规则
 *   用户区（第二个 %% 后）：辅助函数（comment、unescape 等）
 *
 * 【parseLexFile 规则行解析算法】
 *   1. 扫描至空白（不在 [] 或 "..." 内）分割 pattern | action
 *   2. readActionCode 括号匹配读取完整 { ... }（可跨行）
 *   3. expandDefinitions 将 {宏名} 替换为定义区正则
 *
 * 【processRules 流水线】
 *   每条规则: RegExpParser → NFAConstructor::build
 *   → mergeRules → DFAConverter::convert → DFAMinimizer::minimize
 *   → CodeGenerator::generate
 *
 * 【限制】规则区仅支持整行块注释，pattern/action 行尾不可加注释
 * ============================================================================
 */
#include "LexFileParser.h"
#include "RegExpParser.h"
#include "NFAConstructor.h"
#include "DFAConverter.h"
#include "DFAMinimizer.h"
#include "CodeGenerator.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <unordered_map>
#include <algorithm>
using namespace std;

/** 将整个 .l 文件读入内存字符串 */
static string readFile(const string& filename) {
    ifstream ifs(filename);
    if (!ifs) { cerr << "无法打开文件: " << filename << endl; exit(1); }
    stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

/** 去除字符串首尾空白字符 */
static string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * 将模式中的 {宏名} 替换为 definitions 中的正则片段（迭代直至稳定）。
 * 字符串字面量内的花括号、以及反斜杠转义序列不参与替换，避免误展开。
 */
static string expandDefinitions(const string& pattern, const unordered_map<string, string>& defs) {
    string result = pattern;
    bool changed;
    int maxIterations = 100;
    int iterations = 0;
    
    do {
        changed = false;
        string newResult;
        size_t i = 0;
        while (i < result.size()) {
            /* 引号内原样复制，不解析 {name} */
            if (result[i] == '"') {
                newResult += result[i++];
                while (i < result.size() && result[i] != '"') {
                    if (result[i] == '\\' && i + 1 < result.size()) {
                        newResult += result[i++];
                        newResult += result[i++];
                    } else {
                        newResult += result[i++];
                    }
                }
                if (i < result.size()) newResult += result[i++];
                continue;
            }
            
            /* 反斜杠与下一字符作为整体保留 */
            if (result[i] == '\\' && i + 1 < result.size()) {
                newResult += result[i++];
                newResult += result[i++];
                continue;
            }
            
            if (result[i] == '{') {
                size_t j = result.find('}', i);
                if (j == string::npos) {
                    cerr << "Error: unmatched '{' in pattern: " << result << endl;
                    exit(1);
                }
                string name = result.substr(i + 1, j - i - 1);
                auto it = defs.find(name);
                if (it == defs.end()) {
                    cerr << "undefined definition: {" << name << "}" << endl;
                    exit(1);
                }
                newResult += it->second;
                i = j + 1;
                changed = true;
            }
            else {
                newResult += result[i];
                i++;
            }
        }
        if (changed) result = newResult;
        iterations++;
    } while (changed && iterations < maxIterations);
    
    if (iterations >= maxIterations) {
        cerr << "Error: possible circular definition in pattern" << endl;
        exit(1);
    }
    
    return result;
}

/**
 * 从规则行剩余部分读取完整动作代码块（可能跨多行）。
 * 通过花括号计数 balance 匹配成对的 { }。
 */
static string readActionCode(const string& line, istream& iss) {
    size_t bracePos = line.find('{');
    if (bracePos == string::npos) return "";
    string action = line.substr(bracePos);
    int balance = 0;
    for (char c : action) {
        if (c == '{') balance++;
        else if (c == '}') balance--;
    }
    if (balance == 0) return action;
    string nextLine;
    while (getline(iss, nextLine)) {
        action += "\n" + nextLine;
        for (char c : nextLine) {
            if (c == '{') balance++;
            else if (c == '}') balance--;
        }
        if (balance == 0) break;
    }
    return action;
}

/**
 * 按 Flex 三段式布局解析 .l 内容：
 *   [定义区] %% [规则区] %% [用户 C 代码]
 * 定义区提取 %{ %} 为 prologue、name regex 为 definitions；
 * 规则区逐行解析 pattern 与 { action }。
 */
static void parseLexFile(const string& content,
    string& prologue,
    unordered_map<string, string>& definitions,
    vector<Rule>& rules,
    string& epilogue) {
    size_t firstSep = content.find("%%");
    if (firstSep == string::npos) { cerr << "缺少第一个 %% 分隔符" << endl; exit(1); }

    string defSection = content.substr(0, firstSep);  /* 第一个 %% 之前：宏定义与 %{ %} */
    size_t secondSep = content.find("%%", firstSep + 2);
    string ruleSection, epiSection;
    if (secondSep == string::npos) {
        ruleSection = content.substr(firstSep + 2);
        epiSection = "";
    }
    else {
        ruleSection = content.substr(firstSep + 2, secondSep - firstSep - 2);
        epiSection = content.substr(secondSep + 2);
    }
    epilogue = epiSection;

    istringstream defStream(defSection);
    string line;
    bool inCBlock = false;
    string cBlock;
    while (getline(defStream, line)) {
        string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed == "%{") {  /* Flex 用户 C 代码块开始 → 并入 prologue */
            inCBlock = true;
            cBlock.clear();
            continue;
        }
        if (inCBlock) {
            if (trimmed == "%}") {
                inCBlock = false;
                prologue += cBlock + "\n";
                cBlock.clear();
            }
            else {
                cBlock += line + "\n";
            }
            continue;
        }
        if (isalpha(trimmed[0]) || trimmed[0] == '_') {
            size_t spacePos = trimmed.find_first_of(" \t");
            if (spacePos != string::npos) {
                string name = trimmed.substr(0, spacePos);
                string regex = trim(trimmed.substr(spacePos));
                definitions[name] = regex;
            }
        }
    }

    /* 规则区：在空白处分割 pattern 与 action，但 [] 与 "..." 内空白不算分隔 */
    istringstream ruleStream(ruleSection);
    int ruleId = 1;
    while (getline(ruleStream, line)) {
        string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed.find("/*") == 0) continue;
        if (trimmed[0] == '%') continue;

        string pattern;
        string rest;
        size_t i = 0;
        bool inCharClass = false;
        bool inString = false;
        bool escape = false;
        
        while (i < trimmed.size()) {
            char c = trimmed[i];
            if (escape) {
                pattern += c;
                escape = false;
                i++;
                continue;
            }
            if (c == '\\') {
                pattern += c;
                escape = true;
                i++;
                continue;
            }
            if (c == '[' && !inString) {
                inCharClass = true;
                pattern += c;
                i++;
                continue;
            }
            if (c == ']' && inCharClass && !inString) {
                inCharClass = false;
                pattern += c;
                i++;
                continue;
            }
            if (c == '"' && !inCharClass) {
                inString = !inString;
                pattern += c;
                i++;
                continue;
            }
            if (isspace((unsigned char)c) && !inCharClass && !inString) {
                /* pattern 结束，其后为动作代码 */
                i++;
                /* 跳过 pattern 与 action 之间的空白 */
                while (i < trimmed.size() && isspace((unsigned char)trimmed[i])) i++;
                rest = trimmed.substr(i);
                break;
            }
            pattern += c;
            i++;
        }
        if (pattern.empty()) continue;

        if (rest.empty()) {
            cerr << "Invalid rule line: " << line << endl;
            continue;
        }
        /* 每条规则的动作须以 '{' 开始 */
        size_t bracePos = rest.find('{');
        if (bracePos == string::npos) {
            cerr << "Missing action code for pattern: " << pattern << endl;
            exit(1);
        }
        string actionCode = readActionCode(rest, ruleStream);
        if (actionCode.empty() || actionCode.back() != '}') {
            cerr << "invalid action code: " << trimmed << endl;
            exit(1);
        }

        string expandedPattern = expandDefinitions(pattern, definitions);
        rules.push_back({ expandedPattern, actionCode, ruleId++ });
    }
    if (rules.empty()) {
        cerr << "no valid rules found" << endl;
        exit(1);
    }
}

/**
 * 对全部规则执行：正则解析 → 单条 NFA → 合并 → DFA 转换 → 最小化 → 写文件。
 */
static void processRules(const vector<Rule>& rawRules,
    const string& prologue,
    const string& epilogue) {
    vector<NFA> ruleNfas;
    for (const auto& rule : rawRules) {
        RegexNode* ast = RegExpParser::parse(rule.regexStr);
        if (!ast) {
            cerr << "failed to parse regex: " << rule.regexStr << endl;
            exit(1);
        }
        NFA nfa = NFAConstructor::build(ast);
        delete ast;
        ruleNfas.push_back(nfa);
    }
    NFA mergedNFA = NFAConstructor::mergeRules(ruleNfas);
    vector<DFAState> dfa = DFAConverter::convert(mergedNFA);
    dfa = DFAMinimizer::minimize(dfa);
    CodeGenerator gen(dfa, rawRules, prologue, epilogue);
    gen.generate("generated/lex.yy.cpp", "generated/lex.yy.h");
}

/** 对外接口：读文件、解析、编译生成 lex.yy.cpp / lex.yy.h */
void LexFileParser::parse(const string& filename) {
    string content = readFile(filename);
    string prologue;
    unordered_map<string, string> definitions;
    vector<Rule> rules;
    string epilogue;
    parseLexFile(content, prologue, definitions, rules, epilogue);
    processRules(rules, prologue, epilogue);
    cout << "SeuLex: " << rules.size() << " rules -> generated/lex.yy.cpp, generated/lex.yy.h" << endl;
}