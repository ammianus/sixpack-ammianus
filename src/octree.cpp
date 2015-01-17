#include "octree.h"

bool debug=true;

void Octree::reduce(Octree::Node *node)
{
	if (debug)
		printf("Octree: Reducing the node with weight %d and pos %d\n", node->refCount, node->pos);
	if (node->pos == 0)
		debug = false;

	for (int i=0; i<8; i++)
	{
		if (node->children[i])
		{
			node->refCount += node->children[i]->refCount;
			node->r += node->children[i]->r;
			node->g += node->children[i]->g;
			node->b += node->children[i]->b;
			nodes[node->children[i]->pos] = NULL; //->dead = true;
			delete node->children[i];
			numLeaves--;
			node->children[i] = NULL;
		}
	}
	node->leaf = true;
	numLeaves++;
	if (debug)
		printf("Octree: The reduced node now has a refcount of %d\n", node->refCount);

}


void Octree::addColor(unsigned char r, unsigned char g, unsigned char b)
{
	Octree::Node *node = root;

	for (int shift=7; shift>=0; shift--)
	{
		int idx = (r >> shift) & 1;
		idx |= ((g >> shift) & 1) << 1;
		idx |= ((b >> shift) & 1) << 2;
		node->refCount++;
		if (node->children[idx] == NULL)
		{
			node->leaf = false;
			nodes.push_back(new Node);
			node->children[idx] = nodes[nodes.size() - 1];
			node->children[idx]->pos = nodes.size() - 1;
			node->children[idx]->parent = node;
		}
		node = node->children[idx];
	}
	if (node->refCount == 0)
		numLeaves++;
	node->refCount++;
	node->r = r; 
	node->g = g;
	node->b = b;
}




DIB *Octree::quantise(DIB *srcImage, int numColors)
{
	DIB *destImage = new DIB(srcImage->width, srcImage->height, 8);
	unsigned char *pPalette = (unsigned char*)malloc(768);
  
	destImage->palette = pPalette; 

	for (int y=0; y<srcImage->height; y++)
	{
		for (int x=0; x<srcImage->width; x++)
		{
			unsigned char r,g,b;
			r = srcImage->bits[y * srcImage->pitch + x * 3];
			g = srcImage->bits[y * srcImage->pitch + x * 3 + 1];
			b = srcImage->bits[y * srcImage->pitch + x * 3 + 2];
			addColor(r, g, b);
		}
	}
	
	//while (true)
	{
		/*
		std::vector<Node*>::const_iterator iter;
		for (iter=nodes.begin(); iter!=nodes.end(); iter++)
		{
			if ((*iter)->dead == false)
			{
				if ((*iter)->refCount > 0)
					numLeaves++;
			}
		}*/

		printf("Octree: found %d leaves\n", numLeaves);

		std::vector<Node*>::const_iterator iter;

		if (numLeaves > numColors)
		{
			// Assign weights to all nodes (the sum of the reference counts of all its children)
			/*for (iter=nodes.begin(); iter!=nodes.end(); iter++)
			{
				if ((*iter) != NULL)
				{
					// Have we found a leaf node?
					if ((*iter)->refCount > 0)
					{
						Node *node = *iter;
						node->weight = node->refCount;
						// Move up towards the root
						while (node->parent)
						{
							//printf("parent=%d, node=%d, weight=%d\n", node->parent->pos, node->pos, node->weight);
							node->parent->weight += (*iter)->weight;
							node = node->parent;
						}
					}
				}
			}*/

			// Reduce all nodes with a reference count of 1
			bool reduce1 = true;
			while ((numLeaves > numColors) && (reduce1))
			{
				reduce1 = false;
				for (iter=nodes.begin(); ((iter!=nodes.end()) && (numLeaves>numColors)); iter++)
				{
					if ((*iter) != NULL)
					{
						// Only non-leaf nodes are candidates for reduction
						if ((*iter)->leaf == false)
						{
							if ((*iter)->refCount == 1)
							{
								reduce(*iter);
								reduce1 = true;
								break;
							}
						}
					}
				}
			}

			while (numLeaves > numColors)
			{
				if (debug)
					printf("Octree: %d leaves remaining\n", numLeaves);
				Node *minNode = root;
				for (iter=nodes.begin(); iter!=nodes.end(); iter++)
				{
					if ((*iter) != NULL)
					{
						// Only non-leaf nodes are candidates for reduction
						if ((*iter)->leaf == false)
						{
							if ((*iter)->refCount < minNode->refCount)
								minNode = *iter;
						}
					}
				}
				reduce(minNode);
			}
		}

		int color = 0;
		for (iter=nodes.begin(); iter!=nodes.end(); iter++)
		{
			if ((*iter) != NULL)
			{
				// Have we found a leaf node?
				if ((*iter)->leaf)
				{
					pPalette[color * 3 + 0] = (*iter)->r / (*iter)->refCount;
					pPalette[color * 3 + 1] = (*iter)->g / (*iter)->refCount;
					pPalette[color * 3 + 2] = (*iter)->b / (*iter)->refCount;
					color++;
				}
			}
		}
	}

	
	return destImage;
}
