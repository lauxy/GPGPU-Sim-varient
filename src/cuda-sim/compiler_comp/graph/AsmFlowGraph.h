#ifndef ASM_FLOW_GRAPH_H
#define ASM_FLOW_GRAPH_H

#include <vector>
#include <map>
#include "Graph.h"
#include "Node.h"
#include "../../ptx_ir.h"

// namespace graph {

class AsmFlowGraph : public Graph
{
public:
	AsmFlowGraph(std::vector<ptx_instruction*> &instrs);
	~AsmFlowGraph();
	
	class InstNode : public Node
	{
	public:
		InstNode(AsmFlowGraph *g, ptx_instruction *i) : Node(g), inst(i) {}
		ptx_instruction* getInst() const{
			return inst;
		}
	private:
		ptx_instruction* inst;
	};
};

// }//namespace graph
#endif
