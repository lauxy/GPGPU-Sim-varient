#ifndef LIVENESS_H
#define LIVENESS_H

#include <vector>
#include <algorithm>
#include <stdio.h>
#include <map>

#include "graph/Node.h"
#include "graph/Graph.h"
#include "graph/AsmFlowGraph.h"

class operand_info;
class Temp;

typedef std::vector<Temp>::iterator _InIterator;

class LiveInfo {
public:
	std::vector<Temp> def;
	std::vector<Temp> use;
	std::vector<Temp> in;
	std::vector<Temp> out;
	Node* node;
	int inst_order;
	bool operator<(const LiveInfo& other) {
		node->id < other.node->id;
	}
	void copy(std::vector<Temp>& tlist, Temp::TempOprdType type) {
		if (type == Temp::_use) {
			for (int i=0; i<tlist.size(); ++i) {
				use.push_back(tlist[i]);
			}
		} else {
			// type is _def
			for (int i=0; i<tlist.size(); ++i) {
				def.push_back(tlist[i]);
			}
		}
	}
};

class Liveness
{
public:
	Liveness(const Graph &flow);
	// graph::InterferenceGraph *getInterferenceGraph() const {
	// 	return igraph;
	// }
	std::map<int, std::vector<Temp>> getLiveSet() const {
		return liveSet;
	}

private:
	LiveInfo* node2live_info(Node* n);
	std::vector<Temp> getAllLiveinsAtSuccessors(int n);
	std::vector<LiveInfo*> info; // liveness info of each instr
	std::map<int, std::vector<Temp>> liveSet;
	// graph::InterferenceGraph *igraph;

	std::vector<Temp> set_union(_InIterator v1_begin, _InIterator v1_end,
		_InIterator v2_begin, _InIterator v2_end);
	std::vector<Temp> set_difference(_InIterator v1_begin, _InIterator v1_end,
		_InIterator v2_begin, _InIterator v2_end);
};

#endif
