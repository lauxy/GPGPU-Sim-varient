#ifndef GRAPH_H
#define GRAPH_H

// #include <set>
#include <string>
#include <vector>
#include <assert.h>
#include "Node.h"


// namespace graph {

class Node;
typedef std::vector<Node*> NodeList;


class Graph {
public:
	Graph();
	Graph(const Graph &);
	Graph &operator=(const Graph &);

	const NodeList &getNodes() const;
	void addNode(Node *n);
	void addEdge(Node *from, Node *to);
	void rmEdge(Node *from, Node *to);
	void sortNodesAccordId();
	void dfs();
	void dfs(int i);

	size_t size() const;
	virtual void rmNode(Node *n);
protected:
	NodeList nodes;
	int cnt;
};

// }//namespace graph

#endif //GRAPH_H
