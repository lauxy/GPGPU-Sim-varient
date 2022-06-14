#include "Node.h"

// namespace graph {
Node::Node(Graph *g)
	: graph(g)
	, tag(0)
	, invalidAdj(true)
{
	visited = false;
}

const NodeList & 
Node::succ() const
{
	return successors;
}

const NodeList & 
Node::pred() const
{
	return predecessors;
}

const NodeList & 
Node::adj() const
{
	if (invalidAdj) {
		adjacencies.clear();
		for (NodeList::const_iterator it=successors.begin(); 
			it!=successors.end(); ++it) {
			adjacencies.push_back(*it);
		}
		for (NodeList::const_iterator it=predecessors.begin(); 
			it!=predecessors.end(); ++it) {
			adjacencies.push_back(*it);
		}
		// adjacencies.push_all(successors);
		// adjacencies.push_all(predecessors);
		std::sort(adjacencies.begin(), adjacencies.end());
		adjacencies.erase(std::unique(adjacencies.begin(), 
			adjacencies.end()), adjacencies.end());
		// adjacencies.unique();
		invalidAdj = false;
	}
	return adjacencies;
}

int 
Node::outDegree() const
{ 
	return successors.size();
}

int 
Node::inDegree() const
{
	return predecessors.size();
}

int 
Node::degree() const
{
	return outDegree() + inDegree();
}

bool 
Node::goesTo(Node *n) const 
{
	return std::find(successors.begin(), 
		successors.end(), n) != successors.end();
	// return successors.contain(n);
}

bool 
Node::comesFrom(Node *n) const
{
	return std::find(predecessors.begin(), 
		predecessors.end(), n) != predecessors.end();
	// return predecessors.contain(n);
}

bool 
Node::adj(Node *n) const
{
	return goesTo(n) || comesFrom(n);
}

void
Node::setTag(uintptr_t t)
{
	tag = t;
}

uintptr_t
Node::getTag() const
{
	return tag;
}

std::string 
Node::toString() const
{
	return "";
}

void 
Node::addSuccessor(Node *n)
{
	successors.push_back(n);
	invalidAdj = true;
}

void 
Node::addPredecessor(Node *n)
{
	predecessors.push_back(n);
	invalidAdj = true;
}

void 
Node::removeSuccessor(Node *n)
{
	NodeList::iterator it = 
		std::find(successors.begin(), successors.end(), n);
	if (it != successors.end()) {
		successors.erase(it);
	}
	// successors.remove(n);
	invalidAdj = true;
}

void 
Node::removePredecessor(Node *n)
{
	NodeList::iterator it = 
		std::find(predecessors.begin(), predecessors.end(), n);
	if (it != predecessors.end()) {
		predecessors.erase(it);
	}
	// predecessors.remove(n);
	invalidAdj = true;
}

// }//namespace graph
