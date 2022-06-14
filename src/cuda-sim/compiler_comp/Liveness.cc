#include "Liveness.h"

Liveness::Liveness(const Graph &flow)
{
	const NodeList &flowNodes = flow.getNodes();
	for (Node* n : flowNodes) {
		AsmFlowGraph::InstNode *inode = (AsmFlowGraph::InstNode*)n;
		ptx_instruction* inst = inode->getInst();

		std::vector<Temp> v_def = inst->def();
		std::vector<Temp> v_use = inst->use();

												/* Show Def and Use set */
		// printf("\n(instr %d): ",inst->get_PC()/8);
		// inst->print_insn();
		// for(int i=0; i<v_def.size(); ++i) {
		// 	printf("<def oprd> %s (type: %d), addr_off: %d, mem_off: %u\n", v_def[i].oprd->name().c_str(), v_def[i].oprd->get_type(), v_def[i].oprd->get_addr_offset(), v_def[i].oprd->get_const_mem_offset());
		// }
		// if(v_use.size()) {
		// 	for(int i=0; i<v_use.size(); ++i) {
		// 		printf("<use oprd> %s (type: %d), addr_off: %d, mem_off: %u\n", v_use[i].oprd->name().c_str(), v_use[i].oprd->get_type(), v_use[i].oprd->get_addr_offset(), v_use[i].oprd->get_const_mem_offset());
		// 	}
		// }
    	
		LiveInfo *li = new LiveInfo();
		li->copy(v_def, Temp::_def); // li->def <-- v_def
		li->copy(v_use, Temp::_use); // li->use <-- v_use
		li->node = n;
		// li->inst_order = inst_order++;
		li->inst_order = inst->get_PC()/8;
		info.push_back(li);
	}

	// std::sort(info.begin(), info.end(), [](LiveInfo* a, LiveInfo* b) {
	// 	return a->inst_order <= b->inst_order;
	// });

	bool continuing = false;
	do{
		continuing = false;

		// for (int n = 0; n < info.size(); ++n) {
		for (int n = info.size()-1; n >= 0; --n) {
			std::vector<Temp> in_old = info[n]->in;
			std::vector<Temp> out_old = info[n]->out;
			
			info[n]->out = getAllLiveinsAtSuccessors(n);

			// AsmFlowGraph::InstNode *inode = (AsmFlowGraph::InstNode*)info[n]->node;
			// ptx_instruction* inst = inode->getInst();
			// printf("<inst id: %d, order: %d> ", info[n]->node->id, info[n]->inst_order);
			// inst->print_insn();
			// printf("out[] = { ");
			// for(std::vector<Temp>::iterator it=info[n]->out.begin(); it!=info[n]->out.end(); ++it) {
			// 	printf("%s ",it->oprd->name().c_str());
			// }
			// printf("}\ndef[] = { ");
			// for(std::vector<Temp>::iterator it=info[n]->def.begin(); it!=info[n]->def.end(); ++it) {
			// 	printf("%s ",it->oprd->name().c_str());
			// }
			// printf("}\n");

			std::vector<Temp> s_diff = set_difference(info[n]->out.begin(), 
				info[n]->out.end(), info[n]->def.begin(), info[n]->def.end());

			// printf("s_diff[n] = out[n] - def[n] = { ");
			// for(std::vector<Temp>::iterator it=s_diff.begin(); it!=s_diff.end(); ++it) {
			// 	printf("%s ",it->oprd->name().c_str());
			// }
			// printf("}\nuse[] = { ");
			// for(std::vector<Temp>::iterator it=info[n]->use.begin(); it!=info[n]->use.end(); ++it) {
			// 	printf("%s ",it->oprd->name().c_str());
			// }
			// printf("}\n");

			std::vector<Temp> s_union = set_union(s_diff.begin(), s_diff.end(),
				info[n]->use.begin(), info[n]->use.end());
			info[n]->in = s_union;

			// printf("in[n] = s_union[n] = s_diff[n] U use[n] = { ");
			// for(std::vector<Temp>::iterator it=s_union.begin(); it!=s_union.end(); ++it) {
			// 	printf("%s ",it->oprd->name().c_str());
			// }
			// printf("}\n\n");
			
			if ((info[n]->in != in_old) || 
				(info[n]->out != out_old) ) {
				continuing = true;
			}
		}
	} while(continuing);

	// liveness set[n] <-- in[n] U out[n]
	for(int i = 0; i < info.size(); ++i) {
		std::vector<Temp> liveList = set_union(info[i]->in.begin(), 
			info[i]->in.end(), info[i]->out.begin(), info[i]->out.end());
		liveSet.insert(std::make_pair(info[i]->inst_order, liveList));

		// AsmFlowGraph::InstNode *inode = (AsmFlowGraph::InstNode*)info[i]->node;
		// ptx_instruction* inst = inode->getInst();
		// printf("[inst %d] ",info[i]->inst_order);
		// inst->print_insn();
		// printf("<list regs> ");
		// for(int j=0;j<liveList.size();++j){
		// 	printf("%s ",liveList[j].oprd->name().c_str());
		// }
		// printf("\n");
	}
}

LiveInfo* 
Liveness::node2live_info(Node* n) {
	for (std::vector<LiveInfo*>::iterator it=info.begin(); 
		it!=info.end(); ++it) {
		if ((*it)->node == n) {
			return *it;
		}
	}
	return nullptr;
}

std::vector<Temp>
Liveness::getAllLiveinsAtSuccessors(int n)
{
	std::vector<Temp> livein;
	const NodeList &successors = info[n]->node->succ();
	for (Node* succ : successors) {
		LiveInfo* li = node2live_info(succ);
		if (li) {
			std::set_union(li->in.begin(), li->in.end(), 
				livein.begin(), livein.end(), std::inserter(
				livein, livein.begin()));
		}
	}
	return livein;
}

std::vector<Temp> Liveness::set_union(_InIterator v1_begin, _InIterator v1_end,
	_InIterator v2_begin, _InIterator v2_end) {
    std::vector<Temp> res;
    res.assign(v1_begin, v1_end);
    res.insert(res.end(), v2_begin, v2_end);
	std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

std::vector<Temp> Liveness::set_difference(_InIterator v1_begin, 
	_InIterator v1_end, _InIterator v2_begin, _InIterator v2_end) {
	std::vector<Temp> res;
    res.assign(v1_begin, v1_end);
    for (_InIterator it = v2_begin; it != v2_end; ++it) {
        _InIterator resit = std::find(res.begin(), res.end(), *it);
        if (resit != res.end()) {
            res.erase(resit);
        }
    }
    return res;
}

// void
// Liveness::makeInterferenceGraph()
// {
// 	igraph = new InterferenceGraph();

// 	std::vector<Temp> liveTemps;
// 	for (LiveInfo* li : info) {
// 		std::vector<Temp> tlist = bitmap2tempList(*(li->liveout));
// 		std::copy(tlist.begin(), tlist.end(), std::back_inserter(liveTemps));
// 	}
// 	std::sort(liveTemps.begin(), liveTemps.end(), [](Temp a, Temp b){
// 		return a.order >= b.order;
// 	});
// 	liveTemps.erase(std::unique(liveTemps.begin(), liveTemps.end()), liveTemps.end());

// 	//create nodes for oprd_vec,
// 	for (Temp t : liveTemps) {
// 		igraph->newNode(&t);
// 	}

// 	//we must assign node id to each node
// 	int nid = 0;
// 	const NodeList &nodes = igraph->getNodes(); 
// 	for (Node* n : nodes) {
// 		n->setTag(nid);
// 		++nid;
// 	}
// }

// }//namespace regalloc
