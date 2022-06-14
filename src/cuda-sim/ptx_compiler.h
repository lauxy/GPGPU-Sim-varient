#ifndef ptx_compiler_h_INCLUDED
#define ptx_compiler_h_INCLUDED

#include <vector>
#include <set>
#include <map>

#include "compiler_comp/Liveness.h"

class Temp;
class operand_info;
class ptx_instruction;
class AsmFlowGraph;

typedef std::map<int, std::vector<operand_info*>> _RetType;

class ptx_compiler_components {
private:
    AsmFlowGraph* flow;
    Liveness* liveness;
    _RetType liveSet;
    void gen_liveness(std::vector<ptx_instruction*>& instrs);
public:
    ptx_compiler_components(){}
    _RetType get_liveness(std::vector<ptx_instruction*>& instrs);
};

#endif