#ifndef NODE_H
#define NODE_H

#include <string>
#include <stdint.h>
#include <vector>
#include <algorithm>

// namespace graph {

class Graph;
class Node;
typedef std::vector<Node*> NodeList;

// Node is setting at instr level
class Node {
public:
	bool visited;
	unsigned id; // node id in CFG, namely order
	virtual ~Node() {}

	Node(Graph *g);
	const NodeList &succ() const;
	const NodeList &pred() const;
	const NodeList &adj() const;
	int outDegree() const;
	int inDegree() const;
	int degree() const;
	bool goesTo(Node *n) const;
	bool comesFrom(Node *n) const;
	bool adj(Node *n) const;

	void setTag(uintptr_t tag);
	uintptr_t getTag() const;

	operator const char*() {
		return toString().c_str();
	}
	virtual std::string toString() const;

private:
	void addSuccessor(Node *n);
	void addPredecessor(Node *n);
	void removeSuccessor(Node *n);
	void removePredecessor(Node *n);

	Graph *graph;
	NodeList successors;
	NodeList predecessors;
	uintptr_t tag;
	mutable NodeList adjacencies;
	mutable bool invalidAdj;
	friend class Graph;
};

// }//namespace graph

#endif
