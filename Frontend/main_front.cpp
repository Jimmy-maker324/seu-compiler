/**
 * @file main_front.cpp
 * @brief 编译器前端主程序入口
 *
 * 【整体流程】
 *   read_file → run_lexical_analysis（逐 Token 写入 out 文件）
 *            → set_input + yyparse（LR 分析建 AST）
 *            → ASTPrinter / ASTDotExporter
 *            → TypeChecker::check
 *            → IRGenerator::generate → IROptimizer → output/output.ir
 *
 * 【输出约定】
 *   output/out.txt（默认）：词法分析、语法分析、语法树详情
 *   output/ast.dot、output/ast.png、output/output.ir
 *   控制台：各阶段简要进度
 *
 * @context 东南大学（SEU）编译原理专题实践 — Seu 编译器项目
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#ifdef _WIN32
#include <direct.h>
#endif

#include "seulex.h"
#include "token_names.h"
#include "ast.h"
#include "typecheck.h"
#include "irgen.h"
#include "ir_opt.h"
#include "ast_printer.h"
#include "ast_dot.h"

/** @brief 语法分析完成后由 yyparse 设置的全局 AST 根节点 */
extern ASTNode* astRoot;
/** @brief 语法错误计数（yacc.y 中 yyerror 递增） */
extern int parseErrorCount;

/** @brief Bison/Flex 生成的语法分析入口 */
int yyparse(void);
/** @brief 语法错误回调，由语法分析器调用 */
void yyerror(const char* msg);

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
extern "C" int __stdcall SetConsoleOutputCP(unsigned int);
extern "C" int __stdcall SetConsoleCP(unsigned int);
#endif

/** @brief 控制台 UTF-8 输出（Windows） */
static void init_console_encoding(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

/** @brief 打开 UTF-8 详情报告文件（含 BOM） */
static std::ofstream open_detail_file(const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::perror(path);
        exit(1);
    }
    out.write("\xEF\xBB\xBF", 3);
    return out;
}

/**
 * @brief 以二进制方式读取源文件到堆缓冲区
 * @return 以 '\\0' 结尾的字符串；若末尾无换行则自动补 '\\n'
 */
static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror(filename); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)size + 2);
    size_t nread = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[nread] = '\0';
    if (nread == 0 || buf[nread - 1] != '\n') {
        buf[nread] = '\n';
        buf[nread + 1] = '\0';
    }
    return buf;
}

/** @brief 格式化输出单个 Token 到详情流 */
static void print_token(int token, std::ostream& out) {
    out << "  line " << std::setw(4) << yylineno << " "
        << std::setw(16) << std::left << tokenDisplayName(token) << std::right;
    if (token == T_IDENTIFIER || token == T_STRING_LITERAL) {
        out << "  " << (yylval.sval ? yylval.sval : "(null)");
    } else if (token == T_CONSTANT) {
        out << "  " << yylval.ival;
    } else if (token == T_FLOAT_CONSTANT) {
        out << "  " << yylval.fval;
    } else if (yytext && yytext[0]) {
        out << "  \"" << yytext << "\"";
    }
    out << "\n";
}

/** @brief 词法分析，Token 列表写入详情流，返回 Token 总数 */
static int run_lexical_analysis(const char* source, std::ostream& detail) {
    detail << "========== 词法分析 ==========\n";
    set_input(source);
    int count = 0;
    int tok;
    while ((tok = yylex()) != 0) {
        print_token(tok, detail);
        count++;
        if (tok == T_IDENTIFIER || tok == T_STRING_LITERAL) {
            free(yylval.sval);
            yylval.sval = nullptr;
        }
    }
    detail << "  (共 " << count << " 个 Token，文件结束)\n\n";
    return count;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(true);
    init_console_encoding();

    const char* sourcePath = nullptr;
    const char* detailPath = "output/out.txt";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            detailPath = argv[++i];
        } else if (!sourcePath) {
            sourcePath = argv[i];
        }
    }

    if (!sourcePath) {
        fprintf(stderr, "Usage: %s <source> [-o report.txt]\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    _mkdir("output");
#endif

    std::ofstream detailOut = open_detail_file(detailPath);
    char* input = read_file(sourcePath);

    int tokenCount = run_lexical_analysis(input, detailOut);
    printf("词法分析完成（%d tokens，详见 %s）\n", tokenCount, detailPath);

    detailOut << "========== 语法分析 ==========\n";
    set_input(input);
    parseErrorCount = 0;
    int result = yyparse();

    if (result != 0 || parseErrorCount > 0) {
        if (result == 0 && parseErrorCount > 0) {
            detailOut << "  语法分析完成但存在 " << parseErrorCount
                      << " 个错误（已部分恢复，AST/IR 不可靠）\n\n";
            printf("语法分析失败（%d 个语法错误）\n", parseErrorCount);
        } else {
            detailOut << "  语法分析失败\n\n";
            printf("语法分析失败\n");
        }
        detailOut.close();
        free(input);
        return result != 0 ? result : 1;
    }

    detailOut << "  语法分析成功（LR 归约完成，已构建 AST 根节点）\n\n";
    printf("语法分析成功\n");

    free(input);

    if (astRoot) {
        detailOut << "========== 语法树 (多叉树) ==========\n";
        ASTPrinter printer;
        printer.print(astRoot, detailOut);
        detailOut << "\n";

        ASTDotExporter dotter;
        dotter.exportToFile(astRoot, "output/ast.dot");
        detailOut << "========== 语法树 (Graphviz) ==========\n";
        detailOut << "  已导出 output/ast.dot\n";
        if (renderDotToPng("output/ast.dot", "output/ast.png")) {
            detailOut << "  已生成 output/ast.png\n\n";
            printf("语法树已写入 %s，output/ast.dot / output/ast.png 已生成\n", detailPath);
        } else {
            detailOut << "  未生成 output/ast.png（请安装 Graphviz 并将 dot 加入 PATH）\n\n";
            printf("语法树已写入 %s，output/ast.dot 已导出（未找到 dot，跳过 ast.png）\n", detailPath);
        }

        printf("语义分析... ");
        TypeChecker checker;
        checker.check(astRoot, &detailOut);
        if (checker.hasErrors()) {
            printf("失败（%d 个类型错误），跳过 IR 生成\n", checker.errorCount());
            detailOut.close();
            return 2;
        }
        printf("通过\n");

        detailOut.close();

        IRGenerator irgen("output/output.ir");
        irgen.generate(astRoot);
        irgen.dumpTo("output/output_raw.ir");

        printf("代码优化... ");
        IROptimizer opt;
        IROptStats optStats = opt.run(irgen.getCode());
        printf("常量折叠 %d, 传播 %d, 消除 %d, 外提 %d\n",
               optStats.constFold, optStats.constProp,
               optStats.deadRemoved, optStats.hoisted);
        irgen.dump();

        printf("全部阶段完成。\n");
    } else {
        detailOut.close();
    }

    return 0;
}

