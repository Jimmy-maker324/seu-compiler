/**
 * @file compile_pipeline.cpp
 * @brief 编译管线：TypeChecker 构建符号表后，IRGenerator 在同一 AST 上生成四元式
 */

#include "compile_pipeline.h"
#include "typecheck.h"
#include "irgen.h"
#include "ir_opt.h"

bool compileSemanticAndIR(ASTNode* root, std::ostream* report, IRGenerator& ir) {
    TypeChecker checker;
    checker.check(root, report);
    if (checker.hasErrors())
        return false;
    ir.generate(root);
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
