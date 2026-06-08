/**
 * @file ir_opt.h
 * @brief 经典 IR 优化：常量传播/折叠、死代码消除、循环不变式外提
 */

#ifndef IR_OPT_H
#define IR_OPT_H

#include "ir.h"
#include <vector>

struct IROptStats {
    int constFold = 0;
    int constProp = 0;
    int deadRemoved = 0;
    int hoisted = 0;
};

class IROptimizer {
public:
    /** @brief 依次运行各优化 pass，直至基本不动或达到 maxRound */
    IROptStats run(std::vector<IRQuad>& code, int maxRound = 3);

private:
    bool constantPropagationAndFolding(std::vector<IRQuad>& code, IROptStats& stats);
    bool deadCodeElimination(std::vector<IRQuad>& code, IROptStats& stats);
    bool loopInvariantCodeMotion(std::vector<IRQuad>& code, IROptStats& stats);
};

#endif
