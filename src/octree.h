/*
	Octree color quantiser interface

	Based on an article written by Nils Pipenbrinck
	C++ version and modifications by Mic, 2009
*/

#ifndef __OCTREE_H__
#define __OCTREE_H__

#include <stdlib.h>
#include <vector>
#include "dib.h"

class Octree
{
private:
	class Node
	{
	public:
		Node()
		{
			refCount = weight = 0;
			r = g = b = 0;
			parent = NULL;
			leaf = true;
			for (int i=0; i<8; i++) children[i] = NULL;
		}

        Node(unsigned char red, unsigned char green, unsigned char blue)
		{
			refCount = 1;
			weight = 0;
			parent = NULL;
			leaf = true;
			r = red; g = green; b = blue;
			for (int i=0; i<8; i++) children[i] = NULL;
		}

		// When deleting a node we also recursively delete all its children
		//~Node() { for (int i=0; i<8; i++) if (children[i]) delete children[i]; }
		
		int refCount, weight;
		int pos;
		unsigned char r,g,b;
		bool leaf;
		Node *parent;
		Node *children[8];
	};
	
	Node *root;
	std::vector<Node*> nodes;
	int numLeaves;

	void addColor(unsigned char r, unsigned char g, unsigned char b);
	//void traverse(Node *node, void (*traverseFunc)(Node*));

public:
	Octree()
	{
		nodes.push_back(new Node);
		root = nodes[0];
		root->pos = 0;
		numLeaves = 0;
	}

	~Octree() {	nodes.clear(); }

	DIB *quantise(DIB *srcImage, int numColors);

	void reduce(Node *node);
};


#endif
