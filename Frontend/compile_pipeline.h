/**
 * @file compile_pipeline.h
 * @brief 语义分析 + IR 生成的统一入口（共用符号表规则，顺序执行）
 */

#ifndef COMPILE_PIPELINE_H
#define COMPILE_PIPELINE_H

class ASTNode;
class IRGenerator;
class IROptStats;

#include <iosfwd>

/** @brief 语义分析通过后生成 IR；失败时不写 IR。返回是否语义通过 */
bool compileSemanticAndIR(ASTNode* root, std::ostream* report, IRGenerator& ir);

/** @brief 对 IR 运行优化并 dump；stats 可为 nullptr */
void finalizeIR(IRGenerator& ir, bool noOpt, IROptStats* stats = nullptr);

#endif
