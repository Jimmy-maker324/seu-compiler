/*
 * lex_main.cpp — SeuLex 词法生成器可执行程序入口
 *
 * 用法：SeuLex <input.l>
 * 读取 Flex 格式的 .l 文件，经正则解析、NFA/DFA 构造与最小化后，
 * 输出 lex.yy.cpp 与 lex.yy.h 供编译器前端链接。
 */
#include "LexFileParser.h"
#include "lex_common.h"
#include <iostream>
using namespace std;

/**
 * 程序入口：校验命令行参数后调用 LexFileParser 完成整条流水线。
 * @param argc 参数个数，须为 2（程序名 + 一个 .l 文件路径）
 * @param argv 参数列表，argv[1] 为输入的 Flex 规格文件
 * @return 0 成功；1 参数错误；2 解析或生成过程抛出异常
 */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input.l file>" << endl;
        cerr << "Example: " << argv[0] << " scanner.l" << endl;
        return 1;
    }
    try {
        LexFileParser parser;
        parser.parse(argv[1]);  /* 驱动：读文件 → NFA → DFA → 代码生成 */
    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return 2;
    } catch (...) {
        cerr << "Fatal error: unknown exception" << endl;
        return 2;
    }
    return 0;
}
