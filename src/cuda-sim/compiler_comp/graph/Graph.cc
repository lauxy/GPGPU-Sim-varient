#include "Graph.h"

// namespace graph {

Graph::Graph()
{
	cnt = 0;
}

Graph::Graph(const Graph &other)
{
	for (Node* n : other.nodes) {
		n->graph = this;//replace node's parent
		nodes.push_back(n);
	}
}

Graph &
Graph::operator=(const Graph &other)
{
	nodes.clear();
	for (Node* n : other.nodes) {
		n->graph = this; //replace node's parent
		nodes.push_back(n);
	}
	return *this;
}


const NodeList & 
Graph::getNodes() const
{
	return nodes;
}

void
Graph::addNode(Node *n)
{
	assert(n->graph == this);
	nodes.push_back(n);
}

void
Graph::sortNodesAccordId() {
	std::sort(nodes.begin(), nodes.end(), [](Node* a, Node* b){
		return a->id <= b->id;
	});
}

void
Graph::rmNode(Node *n)
{
	assert(n->graph == this);
	const NodeList &succ = n->succ();
	for (Node* to : succ) {
		rmEdge(n, to);
	}
	const NodeList &pred = n->pred();
	for (Node* from : pred) {
		rmEdge(from, n);
	}

	NodeList::iterator it = std::find(nodes.begin(), nodes.end(), n);
	if (it != nodes.end())
		nodes.erase(it);
}

void 
Graph::addEdge(Node *from, Node *to)
{
	assert(from->graph == this);
	assert(to->graph == this);
	assert(from != to);
	if (std::find(from->successors.begin(), from->successors.end(), to) 
		== from->successors.end()) {
		if(std::find(to->successors.begin(), to->successors.end(), from) 
			!= to->successors.end()) {
			return;
		}
		from->addSuccessor(to);
	}
	if (std::find(to->successors.begin(), to->successors.end(), from) 
		== to->successors.end()) {
		if(std::find(from->successors.begin(), from->successors.end(), to) 
			!= from->successors.end()) {
			return;
		}
		to->addPredecessor(from);
	}
}

void 
Graph::rmEdge(Node *from, Node *to)
{
	assert(from->graph == this);
	assert(to->graph == this);
	assert(from != to);
	from->removeSuccessor(to);
	to->removePredecessor(from);
}

size_t 
Graph::size() const
{
	return nodes.size();
}

void
Graph::dfs(int i) {
	nodes[i]->visited = true;
	nodes[i]->id = cnt++;
	const NodeList& succ = nodes[i]->succ();
	for(Node* n : succ) {
		if (!n->visited) {
			int succ_id = std::find(nodes.begin(), nodes.end(), n) - nodes.begin();
			dfs(succ_id);
		}
	}
}

void
Graph::dfs() {
	for (int i=0; i<nodes.size(); ++i) {
		Node* vtx = nodes[i];
		if (!vtx->visited) {
			dfs(i);
		}
	}
}


// }//namespace graph
