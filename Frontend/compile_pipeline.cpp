/**
 * @file compile_pipeline.cpp
 * @brief 编译管线：TypeChecker 构建符号表后，IRGenerator 在同一 AST 上生成四元式
 *
 * 语句/函数定义的 AST 子节点布局由 ast_walk.h 统一；完整单遍合并需持久化
 * 作用域符号（当前 leaveScope 会释放局部符号，IR 仍须第二遍重建符号表）。
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
