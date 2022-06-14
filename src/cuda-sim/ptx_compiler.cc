#include "ptx_compiler.h"
#include "compiler_comp/graph/AsmFlowGraph.h"

void ptx_compiler_components::gen_liveness(
    std::vector<ptx_instruction*>& instrs) {
    flow = new AsmFlowGraph(instrs);
    // liveness analysis
    liveness = new Liveness(*flow);
    std::map<int, std::vector<Temp>> tlist = liveness->getLiveSet();
    for (std::map<int, std::vector<Temp>>::iterator it=tlist.begin();
        it!=tlist.end(); ++it) {
        std::vector<operand_info*> tlive;
        for (int j = 0; j < it->second.size(); ++j) {
            tlive.push_back(it->second[j].oprd);
        }
        liveSet.insert(std::make_pair(it->first, tlive));
    }
}

_RetType ptx_compiler_components::get_liveness(
    std::vector<ptx_instruction*>& instrs) {
    if (!liveSet.size()) {
        gen_liveness(instrs);
    }
    return liveSet;
}