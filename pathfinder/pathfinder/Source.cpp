#include <iostream>
#include <string>
#include <algorithm>
using namespace std;

#include "GridEngine.h"

class PathFinding : public GridEngine
{
public:
	PathFinding()
	{
		m_sAppName = L"Path Finding";
	}

private:

	struct sNode
	{
		bool bObstacle = false;			
		bool bVisited = false;			
		float fGlobalGoal;				
		float fLocalGoal;				
		int x;						
		int y;
		vector<sNode*> vecNeighbours;	// Connections to neighbours
		sNode* parent;					// Node connecting to this node that offers shortest parent
	};

	sNode *nodes = nullptr;
	int nMapWidth = 16;
	int nMapHeight = 16;

	sNode *nodeStart = nullptr;
	sNode *nodeEnd = nullptr;


protected:
	virtual bool OnUserCreate()
	{
		// Create a 2D array of nodes 
	
		nodes = new sNode[nMapWidth * nMapHeight];
		for (int x = 0; x < nMapWidth; x++)
			for (int y = 0; y < nMapHeight; y++)
			{
				nodes[y * nMapWidth + x].x = x; // 
				nodes[y * nMapWidth + x].y = y;
				nodes[y * nMapWidth + x].bObstacle = false;
				nodes[y * nMapWidth + x].parent = nullptr;
				nodes[y * nMapWidth + x].bVisited = false;
			}

		// Create connections - in this case nodes are on a regular grid
		for (int x = 0; x < nMapWidth; x++)
			for (int y = 0; y < nMapHeight; y++)
			{
				if (y>0)
					nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y - 1) * nMapWidth + (x + 0)]);
				if (y<nMapHeight - 1)
					nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y + 1) * nMapWidth + (x + 0)]);
				if (x>0)
					nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y + 0) * nMapWidth + (x - 1)]);
				if (x<nMapWidth - 1)
					nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y + 0) * nMapWidth + (x + 1)]);

				 //connect diagonally
				if (y>0 && x>0)
				nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y - 1) * nMapWidth + (x - 1)]);
				if (y<nMapHeight-1 && x>0)
				nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y + 1) * nMapWidth + (x - 1)]);
				if (y>0 && x<nMapWidth-1)
				nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y - 1) * nMapWidth + (x + 1)]);
				if (y<nMapHeight - 1 && x<nMapWidth-1)
				nodes[y*nMapWidth + x].vecNeighbours.push_back(&nodes[(y + 1) * nMapWidth + (x + 1)]);
				
			}

		// Manually positio the start and end markers so they are not nullptr
		nodeStart = &nodes[(nMapHeight / 2) * nMapWidth + 1];
		nodeEnd = &nodes[(nMapHeight / 2) * nMapWidth + nMapWidth - 2];
		return true;
	}

	bool Solve_AStar()
	{
		// Reset Navigation Graph - default all node states
		for (int x = 0; x < nMapWidth; x++)
			for (int y = 0; y < nMapHeight; y++)
			{
				nodes[y*nMapWidth + x].bVisited = false;
				nodes[y*nMapWidth + x].fGlobalGoal = INFINITY;
				nodes[y*nMapWidth + x].fLocalGoal = INFINITY;
				nodes[y*nMapWidth + x].parent = nullptr;	// No parents
			}

		auto distance = [](sNode* a, sNode* b) 
		{
			return sqrtf((a->x - b->x)*(a->x - b->x) + (a->y - b->y)*(a->y - b->y));
		};

		auto heuristic = [distance](sNode* a, sNode* b) // So we can experiment with heuristic
		{
			return distance(a, b);
		};

		// Setup starting conditions
		sNode *nodeCurrent = nodeStart;
		nodeStart->fLocalGoal = 0.0f;
		nodeStart->fGlobalGoal = heuristic(nodeStart, nodeEnd);

		// Add start node to not tested list
		// As the algorithm progresses, newly discovered nodes get added to this list
	
		list<sNode*> listNotTestedNodes;
		listNotTestedNodes.push_back(nodeStart);

		// if the not tested list contains nodes, there could be better paths
	   
		// will stop  searching when reaches the target - there may well be better
		// paths but this one will do - it wont be the longest.
		while (!listNotTestedNodes.empty() && nodeCurrent != nodeEnd)// Find absolutely shortest path // && nodeCurrent != nodeEnd)
		{
			// Sort Untested nodes by global goal, so lowest is first
			listNotTestedNodes.sort([](const sNode* lhs, const sNode* rhs) { return lhs->fGlobalGoal < rhs->fGlobalGoal; });

			// Front of listNotTestedNodes is potentially the lowest distance node.
			// list may also contain nodes that have been visited
			while (!listNotTestedNodes.empty() && listNotTestedNodes.front()->bVisited)
				listNotTestedNodes.pop_front();

			// ...or abort because there are no valid nodes left to test
			if (listNotTestedNodes.empty())
				break;

			nodeCurrent = listNotTestedNodes.front();
			nodeCurrent->bVisited = true;

             // Check each of this node's neighbours
			for (auto nodeNeighbour : nodeCurrent->vecNeighbours)
			{
				// and only if the neighbour is not visited and is 
				// not an obstacle, add it to NotTested List
				if (!nodeNeighbour->bVisited && nodeNeighbour->bObstacle == 0)
					listNotTestedNodes.push_back(nodeNeighbour);

				// Calculate the neighbours potential lowest parent distance
				float fPossiblyLowerGoal = nodeCurrent->fLocalGoal + distance(nodeCurrent, nodeNeighbour);

				// If choosing to path through this node is a lower distance than what 
				// the neighbour currently has set, update the neighbour to use this node
				// as the path source, and set its distance scores as necessary
				if (fPossiblyLowerGoal < nodeNeighbour->fLocalGoal)
				{
					nodeNeighbour->parent = nodeCurrent;
					nodeNeighbour->fLocalGoal = fPossiblyLowerGoal;

					// The best path length to the neighbour being tested has changed
					// update the neighbour's score.
					nodeNeighbour->fGlobalGoal = nodeNeighbour->fLocalGoal + heuristic(nodeNeighbour, nodeEnd);
				}
			}
		}

		return true;
	}

	virtual bool OnUserUpdate(float fElapsedTime)
	{
		int nNodeSize = 9;
		int nNodeBorder = 2;

		// Use integer division to nicely get cursor position in node space
		int nSelectedNodeX = m_mousePosX / nNodeSize;
		int nSelectedNodeY = m_mousePosY / nNodeSize;

		if (m_mouse[0].bReleased) // Use mouse to draw maze, shift and ctrl to place start and end
		{
			if (nSelectedNodeX >= 0 && nSelectedNodeX < nMapWidth)
				if (nSelectedNodeY >= 0 && nSelectedNodeY < nMapHeight)
				{
					if (m_keys[VK_SHIFT].bHeld)
						nodeStart = &nodes[nSelectedNodeY * nMapWidth + nSelectedNodeX];
					else if (m_keys[VK_CONTROL].bHeld)
						nodeEnd = &nodes[nSelectedNodeY * nMapWidth + nSelectedNodeX];
					else
						nodes[nSelectedNodeY * nMapWidth + nSelectedNodeX].bObstacle = !nodes[nSelectedNodeY * nMapWidth + nSelectedNodeX].bObstacle;

					Solve_AStar(); 
				}
		}

		// Draw Connections First - lines from this nodes position to its
		// connected neighbour node positions
		Fill(0, 0, ScreenWidth(), ScreenHeight(), L' ');
		for (int x = 0; x < nMapWidth; x++)
			for (int y = 0; y < nMapHeight; y++)
			{
				for (auto n : nodes[y*nMapWidth + x].vecNeighbours)
				{
					DrawLine(x*nNodeSize + nNodeSize / 2, y*nNodeSize + nNodeSize / 2,
						n->x*nNodeSize + nNodeSize / 2, n->y*nNodeSize + nNodeSize / 2, PIXEL_SOLID, FG_DARK_BLUE);
				}
			}

		// Draw Nodes on top
		for (int x = 0; x < nMapWidth; x++)
			for (int y = 0; y < nMapHeight; y++)
			{

				Fill(x*nNodeSize + nNodeBorder, y*nNodeSize + nNodeBorder,
					(x + 1)*nNodeSize - nNodeBorder, (y + 1)*nNodeSize - nNodeBorder,
					PIXEL_HALF, nodes[y * nMapWidth + x].bObstacle ? FG_WHITE : FG_DARK_RED);

				if (nodes[y * nMapWidth + x].bVisited)
					Fill(x*nNodeSize + nNodeBorder, y*nNodeSize + nNodeBorder, (x + 1)*nNodeSize - nNodeBorder, (y + 1)*nNodeSize - nNodeBorder, PIXEL_SOLID, FG_BLUE);

				if (&nodes[y * nMapWidth + x] == nodeStart)
					Fill(x*nNodeSize + nNodeBorder, y*nNodeSize + nNodeBorder, (x + 1)*nNodeSize - nNodeBorder, (y + 1)*nNodeSize - nNodeBorder, PIXEL_SOLID, FG_GREEN);

				if (&nodes[y * nMapWidth + x] == nodeEnd)
					Fill(x*nNodeSize + nNodeBorder, y*nNodeSize + nNodeBorder, (x + 1)*nNodeSize - nNodeBorder, (y + 1)*nNodeSize - nNodeBorder, PIXEL_SOLID, FG_RED);

			}


		// Draw Path by starting ath the end, and following the parent node trail
		// back to the start - the start node will not have a parent path to follow
		if (nodeEnd != nullptr)
		{
			sNode *p = nodeEnd;
			while (p->parent != nullptr)
			{
				DrawLine(p->x*nNodeSize + nNodeSize / 2, p->y*nNodeSize + nNodeSize / 2,
					p->parent->x*nNodeSize + nNodeSize / 2, p->parent->y*nNodeSize + nNodeSize / 2, PIXEL_SOLID, FG_YELLOW);

				// Set next node to this node's parent
				p = p->parent;
			}
		}

		return true;
	}

};

int main()
{
	PathFinding game;
	game.ConstructConsole(160, 160, 6, 6);
	game.Start();
	return 0;
}