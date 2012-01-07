/* GraphFunctionContainerUnified.h
 *
 * Represents the game level as a tile graph.
 *
 * Based on YAGSBPL example code.
 * See: http://subhrajit.net/index.php?WPage=yagsbpl
 *
 * NOTE 1: isAccessible() is commented out
 * I decided to check whether or not a node is accessible in getSuccessors() instead.
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

#include "playing.h"

#define WORLD_SIZE (BZDBCache::worldSize)
#define SCALE (0.5f * BZDBCache::tankRadius)
#define ACCESSIBILITY_THRESHOLD (0.5f * BZDBCache::tankRadius)
#define INVALID -9999

/* Converts from the game's coordinate system to the graph's coordinate system.
 * @param gameCoord A coordinate in the game's coordinate system.
 * @return A coordinate in the graph's coordinate system.
 */
inline int convertToGraphCoord(float gameCoord) {
  return (int) (gameCoord / SCALE);
  // return (int) floor(fmodf(gameCoord, SCALE);
}

/* Converts from the graph's coordinate system to the game's coordinate system.
 * @param graphCoord A coordinate in the graph's coordinate system.
 * @return A coordinate in the game's coordinate system.
 */
inline float convertToGameCoord(int graphCoord) {
  return graphCoord * SCALE;
}



/* -----------------------------------------------
 * Defines graph nodes in terms of points in the game level.  Used to help represent the game level as
 * a graph.
 */
class MyNode {
public:
  // The coordinates of the node, in graph coordinates.
  int x, y;

  /* Constructor.  Initialize with invalid values.
   */
  MyNode() {
    x = INVALID;
    y = INVALID;
  }

  /* Constructor.  Maps a point in the game level to this node.
   * @param gameCoordX The game point's x-coordinate to map to this node.
   * @param gameCoordY The game point's y-coordinate to map to this node.
   */
  MyNode(float gameCoordX, float gameCoordY) {
    x = convertToGraphCoord(gameCoordX);
    y = convertToGraphCoord(gameCoordY);
    if (MyNode::isAccessible(x, y)) return;
    MyNode::getNearbyAccessibleNodeCoords(x, y, 2);
  }

  /* Override == to define how nodes should be compared for equality.
   * @param n The node to compare with this node.
   */
  bool operator==(const MyNode& n) {
    return (x==n.x && y==n.y);
  }

  /* Checks whether or not a node is accessible.
   * @param x A node's x-coordinate in graph coordinates.
   * @param y A node's y-coordinate in graph coordinates.
   */
  static bool isAccessible(int x, int y) {
    float gamePos[3] = {convertToGameCoord(x), convertToGameCoord(y), 0.0f};
    int posBound = convertToGameCoord( ((int)( 0.5f * BZDBCache::worldSize)) );
    int negBound = convertToGameCoord( ((int)(-0.5f * BZDBCache::worldSize)) );

    if (x < negBound || y < negBound || x > posBound || y > posBound)
      return false;

    return (World::getWorld()->inBuilding(gamePos, ACCESSIBILITY_THRESHOLD, BZDBCache::tankHeight) == NULL);
  }

  /* Searches for accessible nodes within a given radius.
   * @param xRef If an accessible node is found, writes the x-coordinate here, in graph coordinates.
   * @param yRef If an accessible node is found, writes the y-coordinate here, in graph coordinates.
   * @return True if an accessible node within the radius could be found, false otherwise.
   */
  static bool getNearbyAccessibleNodeCoords(int &xRef, int &yRef, int radius) {
    int x = xRef;
    int y = yRef;
    if (MyNode::isAccessible(x, y)) {
      return true;
    }
    radius = (radius < 1) ? 1 : radius;
    int k = 1;
    while (k <= radius) {
      for (int i=-k; i<=k; i++) {
        for (int j=-k; j<=k; j++) {
          if (i == 0 & j == 0) continue;
          x += i;
          y += j;
          if (MyNode::isAccessible(x, y)) {
            xRef = x;
            yRef = y;
            return true;
          }
        } // for j
      } // for i
      k++;
    } // while k
    // WARNING: no accessible node could be found within the given radius
    return false;
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
    float gamePos[3] = {convertToGameCoord(n.x), convertToGameCoord(n.y), 0.0f};
    if (gamePos[0] < xmin || gamePos[1] < ymin || gamePos[0] > xmax || gamePos[1] > ymax ||
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

        float gamePos[3] = {convertToGameCoord(n.x + i), convertToGameCoord(n.y + j), 0.0f};
        if (gamePos[0] < xmin || gamePos[1] < ymin || gamePos[0] > xmax || gamePos[1] > ymax ||
            World::getWorld()->inBuilding(gamePos, ACCESSIBILITY_THRESHOLD, BZDBCache::tankHeight)) {
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
