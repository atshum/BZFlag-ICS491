/* GraphFunctionContainerUnified.h
 *
 * Represents the game level as a tile graph.
 *
 * Based on YAGSBPL example code.
 * See: http://subhrajit.net/index.php?WPage=yagsbpl
 *
 * NOTE 1: isAccessible() is commented out
 * Even when I used isAccessible(), I noticed that, sometimes,
 * inaccessible nodes somehow still ended up in my path.
 * Now I check for the node's accessibility in getSuccessors() instead,
 * and it seems to work properly.
 *
 * NOTE 2:
 * I tried to split this code into a source and header file, but when I did,
 * I kept getting an error:
 * Location: getSuccessors(), s->push_back(connectedNode);
 * Error: Invalid arguments ' Candidates are void push_back(const MyNode &)'
 *
 */

#ifndef GRAPHFUNCTIONCONTAINERUNIFIED_H_
#define GRAPHFUNCTIONCONTAINERUNIFIED_H_

#include "BZDBCache.h"
#include "World.h"
#include "yagsbpl_base.h"


/* Converts from the game's coordinate system to the graph's coordinate system.
 * @param gameCoord A coordinate in the game's coordinate system.
 * @return A coordinate in the graph's coordinate system.
 */
inline int convertToGraphCoord(float gameCoord) {
  return (int) (gameCoord / (0.5 * BZDBCache::tankRadius));
}

/* Converts from the graph's coordinate system to the game's coordinate system.
 * @param graphCoord A coordinate in the graph's coordinate system.
 * @return A coordinate in the game's coordinate system.
 */
inline float convertToGameCoord(int graphCoord) {
  return graphCoord * 0.5 * BZDBCache::tankRadius;
}

/* -----------------------------------------------
 * Defines graph nodes in terms of points in the game level.  Used to help represent the game level as
 * a graph.
 */
class MyNode {
public:
  // The coordinates of the node, in graph coordinates.
  int x, y;

  /* Constructor.  Initializes with fake values.
   */
  MyNode() {
    x = -9999;
    y = -9999;
  }

  /* Constructor.  Maps points in the game level to nodes.
   * @param gameCoordX The x-coordinate of the point to map to this node.
   * @param gameCoordY The y-coordinate of the point to map to this node.
   */
  MyNode(float gameCoordX, float gameCoordY) {
    x = convertToGraphCoord(gameCoordX);
    y = convertToGraphCoord(gameCoordY);
    // x = (int) floor(fmodf(coordX, 0.5 * BZDBCache::tankRadius));
    // y = (int) floor(fmodf(coordY, 0.5 * BZDBCache::tankRadius));
  }

  /* Override == to define how nodes should be compared for equality.
   * @param n The node to compare with this node.
   */
  bool operator==(const MyNode& n) {
    return (x==n.x && y==n.y);
  }
};


/* -----------------------------------------------
 * Implements (some of) the virtual functions in the SearchGraphDescriptorFunctionContainer template.
 */
class GraphFunctionContainer : public SearchGraphDescriptorFunctionContainer<MyNode, double> {
public:
  /* Constructor
   * @param _xmin The minimum x-coordinates of the game level.
   * @param _ymin The minimum y-coordinates of the game level.
   * @param _xmax The maximum x-coordinates of the game level.
   * @param _ymax The maximum y-coordinates of the game level.
   */
  GraphFunctionContainer(int _xmin, int _ymin, int _xmax, int _ymax) {
    xmin = _xmin;
    ymin = _ymin;
    xmax = _xmax;
    ymax = _ymax;
  }

  /* Maps nodes to bins in the hash table.
   * @param n The node to map.
   * @return Integer between 0 and hashTableSize-1, inclusive.
   */
  int getHashBin(MyNode& n) {
    return abs(2*n.x + 3*n.y);
  }

/* SEE NOTE AT TOP
   *
   * Indicates whether or not a given node is accessible.
   * @param n The node whose accessibility we are checking.
   * @return True if node is accessible, false otherwise.
  bool isAccessible(MyNode& n) {
    float gamePos[2] = {convertToGameCoord(n.x), convertToGameCoord(n.y)};
    if (gamePos[0] < xmin || gamePos[1] < ymin || gamePos[2] > xmax || gamePos[3] > ymax ||
        World::getWorld()->inBuilding(gamePos, 2 * BZDBCache::tankRadius, BZDBCache::tankHeight))
      return false;
    return true;
  }
*/

  /* For a given node, defines which nodes can be reached and the corresponding costs.
   * @param n The node from which we want to move.
   * @param s A vector of nodes which can be reached from node n.
   * @param c A vector of costs for traveling to the corresponding nodes in vector s.
   */
  void getSuccessors(MyNode& n, std::vector<MyNode>* s, std::vector<double>* c) {
    MyNode connectedNode;
    s->clear();
    c->clear();
    for (int i=-1; i<=1; i++) {
      for (int j=-1; j<=1; j++) {
        if (i==0 && j==0) {
          continue;
        }

        float gamePos[2] = {convertToGameCoord(n.x + i), convertToGameCoord(n.y + j)};
        if (gamePos[0] < xmin || gamePos[1] < ymin || gamePos[2] > xmax || gamePos[3] > ymax ||
            World::getWorld()->inBuilding(gamePos, 0.5 * BZDBCache::tankRadius, BZDBCache::tankHeight)) {
          continue;
        }

        connectedNode.x = n.x + i;
        connectedNode.y = n.y + j;
        s->push_back(connectedNode);
        c->push_back(sqrt((double)(i*i+j*j)));
      }
    }
  }

  /* Heuristic function that estimates the cost between two nodes.
   * @param n1 A node.
   * @param n2 Another node.
   * @return The estimated cost.
   */
  double getHeuristics(MyNode& n1, MyNode& n2) {
    return hypot(n2.x - n1.x, n2.y - n1.y);
  }

private:
  // The bounds of the game level, in game coordinates.
  int xmin, xmax, ymin, ymax;

};


#endif /* GRAPHFUNCTIONCONTAINERUNIFIED_H_ */
