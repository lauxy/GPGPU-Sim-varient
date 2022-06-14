#include "AsmFlowGraph.h"

using namespace std;

AsmFlowGraph::AsmFlowGraph(std::vector<ptx_instruction*> &instrs)
{
	
	ptx_instruction* prevInst = NULL;
	for (ptx_instruction* inst : instrs) {
		if (inst->is_label() || (inst->is_jmp()&&!inst->has_pred())) continue;

		Node* node = new InstNode(this, inst);
		node->id = inst->get_PC()/8;
		Graph::addNode(node); // pack each instr as a node
	}

	const NodeList &nodes = Graph::getNodes();
	Node *prevNode = NULL;
	for (Node* node : nodes) {
		if (prevNode) {
			addEdge(prevNode, node);
		}
		prevNode = node;
	}
}

// AsmFlowGraph::AsmFlowGraph(std::vector<ptx_instruction*> &instrs)
// {
// 	Node *prevNode = NULL;
// 	ptx_instruction* prevInst = NULL;
// 	for (ptx_instruction* inst : instrs) {
// 		Node* node = new InstNode(this, inst);
// 		Graph::addNode(node); // pack each instr as a node

// 		if (prevNode && !prevInst->is_jmp()) {
// 			// add edge between adjecent nodes based on instr sequence order
// 			// jump instr node cannot build an edge 
// 			addEdge(prevNode, node);
// 		}
// 		prevInst = inst;
// 		prevNode = node;
// 	}

// 	// get all nodes of the graph, including all the instrs
// 	const NodeList &nodes = Graph::getNodes();
// 	for (Node* fn : nodes) {
// 		// get 'from' node
// 		InstNode *fromNode = (InstNode*)(fn);
// 		ptx_instruction* from_inst = fromNode->getInst(); // get instr of the node
// 		if (from_inst->is_jmp()) {
// 			std::string label;
// 			// get from node (instr)'s label
// 			switch (from_inst->get_opcode())
// 			{
// 			case opcode_t::BRA_OP:
// 			case opcode_t::BRX_OP:
// 			case opcode_t::CALLP_OP:
// 				assert(from_inst->get_num_operands());
// 				label = from_inst->dst().name();
// 				break;
// 			case opcode_t::CALL_OP:
// 				label = from_inst->func_addr().name();
// 				break;
// 			default:
// 				break;
// 			}
// 			for (Node* tn : nodes) {
// 				// find 'to' node [based on the label]
// 				InstNode* toNode = (InstNode*)(tn);
// 				ptx_instruction* to_inst = toNode->getInst();
// 				if(to_inst->is_label() && to_inst->get_label()->name() 
// 					== label) {
// 					addEdge(fromNode, toNode);
// 				}
// 			}
// 		}
// 	}
// 	Graph::dfs();
// }