/**
 * @file compile_pipeline.cpp
 * @brief 编译管线：TypeChecker 构建符号表后，IRGenerator 在同一 AST 上生成四元式
 *
 * 语句/函数定义的 AST 子节点布局由 ast_walk.h 统一；TypeChecker 单遍遍历 AST，
 * 每条语句语义检查通过后立即由 IRGenerator 生成四元式（不再第二遍 generate）。
 */

#include "compile_pipeline.h"
#include "typecheck.h"
#include "irgen.h"
#include "ir_opt.h"

bool compileSemanticAndIR(ASTNode* root, std::ostream* report, IRGenerator& ir) {
    TypeChecker checker;
    checker.check(root, report, ir);
    if (checker.hasErrors())
        return false;
    return true;
}

void finalizeIR(IRGenerator& ir, bool noOpt, IROptStats* stats) {
    if (noOpt) {
        ir.dump();
        return;
    }
    IROptimizer opt;
    IROptStats local;
    IROptStats& s = stats ? *stats : local;
    s = opt.run(ir.getCode());
    ir.dump();
}
