/* bzflag
 * Copyright (c) 1993-2011 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// interface header
#include "RobotPlayer.h"

// common implementation headers
#include "BZDBCache.h"

// local implementation headers
#include "World.h"
#include "Intersect.h"
#include "TargetingUtils.h"

// ========== MY CODE (begin) ==========

// for ServerLink and Control Panel
#include "playing.h"

// for decision tree
#include "decisiontree/dectree.h"

// constants for flocking
#define SEPARATION_THRESHOLD (3 * BZDBCache::tankRadius)
#define ALIGNMENT_THRESHOLD 10
#define COHESION_WEIGHT 2.5
#define SEPARATION_WEIGHT 3
#define ALIGNMENT_WEIGHT 1
#define PATH_WEIGHT 7

// hash function: 2x + 3y
#define MAX_HASH_VAL (int)(5 * 0.5f * BZDBCache::worldSize)
// using a const float for MAX_HASH_VAL didn't work

//std::vector<MyNode> RobotPlayer::teamPaths[CtfTeams];  // initialized to default MyNode constructor values
//MyNode RobotPlayer::teamGoal[CtfTeams];                // initialized to default MyNode constructor values

PlayerId RobotPlayer::teamLowestId[CtfTeams];          // static variables auto initialized to 0
PlayerId RobotPlayer::teamHighestId[CtfTeams];         // static variables auto initialized to 0

MyNode RobotPlayer::guardGoal[CtfTeams];               // initialized to default MyNode constructor values
std::vector<MyNode> RobotPlayer::guardPath[CtfTeams];  // initialized to default MyNode constructor values

MyNode RobotPlayer::captureGoal[CtfTeams];               // initialized to default MyNode constructor values
std::vector<MyNode> RobotPlayer::capturePath[CtfTeams];  // initialized to default MyNode constructor values

//MyNode RobotPlayer::killGoal[CtfTeams];               // initialized to default MyNode constructor values
//std::vector<MyNode> RobotPlayer::killPath[CtfTeams];  // initialized to default MyNode constructor values

// ========== MY CODE (end) ==========


std::vector<BzfRegion*>* RobotPlayer::obstacleList = NULL;


RobotPlayer::RobotPlayer(const PlayerId& _id, const char* _name, ServerLink* _server,
    const char* _motto = "") :
    LocalPlayer(_id, _name, _motto), target(NULL), pathIndex(0), timerForShot(0.0f),
      drivingForward(true) {
  gettingSound = false;
  server = _server;
}

// estimate a player's position at now+t, similar to dead reckoning
void RobotPlayer::projectPosition(const Player *targ, const float t, float &x, float &y,
    float &z) const {
  double hisx = targ->getPosition()[0];
  double hisy = targ->getPosition()[1];
  double hisz = targ->getPosition()[2];
  double hisvx = targ->getVelocity()[0];
  double hisvy = targ->getVelocity()[1];
  double hisvz = targ->getVelocity()[2];
  double omega = fabs(targ->getAngularVelocity());
  double sx, sy;

  if ((targ->getStatus() & PlayerState::Falling) || fabs(omega) < 2 * M_PI / 360 * 0.5) {
    sx = t * hisvx;
    sy = t * hisvy;
  }
  else {
    double hisspeed = hypotf(hisvx, hisvy);
    double alfa = omega * t;
    double r = hisspeed / fabs(omega);
    double dx = r * sin(alfa);
    double dy2 = r * (1 - cos(alfa));
    double beta = atan2(dy2, dx) * (targ->getAngularVelocity() > 0 ? 1 : -1);
    double gamma = atan2(hisvy, hisvx);
    double rho = gamma + beta;
    sx = hisspeed * t * cos(rho);
    sy = hisspeed * t * sin(rho);
  }
  x = (float) hisx + (float) sx;
  y = (float) hisy + (float) sy;
  z = (float) hisz + (float) hisvz * t;
  if (targ->getStatus() & PlayerState::Falling)
    z += 0.5f * BZDBCache::gravity * t * t;
  if (z < 0)
    z = 0;
}

// get coordinates to aim at when shooting a player; steps:
// 1. estimate how long it will take shot to hit target
// 2. calc position of target at that point of time
// 3. jump to 1., using projected position, loop until result is stable
void RobotPlayer::getProjectedPosition(const Player *targ, float *projpos) const {
  double myx = getPosition()[0];
  double myy = getPosition()[1];
  double hisx = targ->getPosition()[0];
  double hisy = targ->getPosition()[1];
  double deltax = hisx - myx;
  double deltay = hisy - myy;
  double distance = hypotf(deltax, deltay) - BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT)
      - BZDBCache::tankRadius;
  if (distance <= 0)
    distance = 0;
  double shotspeed = BZDB.eval(StateDatabase::BZDB_SHOTSPEED)
      * (getFlag() == Flags::Laser ? BZDB.eval(StateDatabase::BZDB_LASERADVEL) :
         getFlag() == Flags::RapidFire ? BZDB.eval(StateDatabase::BZDB_RFIREADVEL) :
         getFlag() == Flags::MachineGun ? BZDB.eval(StateDatabase::BZDB_MGUNADVEL) : 1)
      + hypotf(getVelocity()[0], getVelocity()[1]);

  double errdistance = 1.0;
  float tx, ty, tz;
  for (int tries = 0; errdistance > 0.05 && tries < 4; tries++) {
    float t = (float) distance / (float) shotspeed;
    projectPosition(targ, t + 0.05f, tx, ty, tz);  // add 50ms for lag
    double distance2 = hypotf(tx - myx, ty - myy);
    errdistance = fabs(distance2 - distance) / (distance + ZERO_TOLERANCE);
    distance = distance2;
  }
  projpos[0] = tx;
  projpos[1] = ty;
  projpos[2] = tz;

  // projected pos in building -> use current pos
  if (World::getWorld()->inBuilding(projpos, 0.0f, BZDBCache::tankHeight)) {
    projpos[0] = targ->getPosition()[0];
    projpos[1] = targ->getPosition()[1];
    projpos[2] = targ->getPosition()[2];
  }
}

/* ========== ORIGINAL doUpdate ==========
void RobotPlayer::doUpdate(float dt) {
  LocalPlayer::doUpdate(dt);

  float tankRadius = BZDBCache::tankRadius;
  const float shotRange = BZDB.eval(StateDatabase::BZDB_SHOTRANGE);
  const float shotRadius = BZDB.eval(StateDatabase::BZDB_SHOTRADIUS);

  // fire shot if any available
  timerForShot -= dt;
  if (timerForShot < 0.0f)
    timerForShot = 0.0f;

  if (getFiringStatus() != Ready)
    return;

  bool shoot = false;
  const float azimuth = getAngle();
  // Allow shooting only if angle is near and timer has elapsed
  if ((int) path.size() != 0 && timerForShot <= 0.0f) {
    float p1[3];
    getProjectedPosition(target, p1);
    const float* p2 = getPosition();
    float shootingAngle = atan2f(p1[1] - p2[1], p1[0] - p2[0]);
    if (shootingAngle < 0.0f)
      shootingAngle += (float) (2.0 * M_PI);
    float azimuthDiff = shootingAngle - azimuth;
    if (azimuthDiff > M_PI)
      azimuthDiff -= (float) (2.0 * M_PI);
    else if (azimuthDiff < -M_PI)
      azimuthDiff += (float) (2.0 * M_PI);

    const float targetdistance = hypotf(p1[0] - p2[0], p1[1] - p2[1])
        - BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT) - tankRadius;

    const float missby = fabs(azimuthDiff) * (targetdistance - BZDBCache::tankLength);
    // only shoot if we miss by less than half a tanklength and no building inbetween
    if (missby < 0.5f * BZDBCache::tankLength && p1[2] < shotRadius) {
      float pos[3] = { getPosition()[0], getPosition()[1], getPosition()[2]
          + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT) };
      float dir[3] = { cosf(azimuth), sinf(azimuth), 0.0f };
      Ray tankRay(pos, dir);
      float maxdistance = targetdistance;
      if (!ShotStrategy::getFirstBuilding(tankRay, -0.5f, maxdistance)) {
        shoot = true;
        // try to not aim at teammates
        for (int i = 0; i <= World::getWorld()->getCurMaxPlayers(); i++) {
          Player *p = 0;
          if (i < World::getWorld()->getCurMaxPlayers())
            p = World::getWorld()->getPlayer(i);
          else
            p = LocalPlayer::getMyTank();
          if (!p || p->getId() == getId() || validTeamTarget(p) || !p->isAlive())
            continue;
          float relpos[3] = { getPosition()[0] - p->getPosition()[0], getPosition()[1]
              - p->getPosition()[1], getPosition()[2] - p->getPosition()[2] };
          Ray ray(relpos, dir);
          float impact = rayAtDistanceFromOrigin(ray, 5 * BZDBCache::tankRadius);
          if (impact > 0 && impact < shotRange) {
            shoot = false;
            timerForShot = 0.1f;
            break;
          }
        }
        if (shoot && fireShot()) {
          // separate shot by 0.2 - 0.8 sec (experimental value)
          timerForShot = float(bzfrand()) * 0.6f + 0.2f;
        }
      }
    }
  }
}
*/

/* ========== ORIGINAL doUpdateMotion ==========
void RobotPlayer::doUpdateMotion(float dt) {
  if (isAlive()) {
    bool evading = false;
    // record previous position
    const float oldAzimuth = getAngle();
    const float* oldPosition = getPosition();
    float position[3];
    position[0] = oldPosition[0];
    position[1] = oldPosition[1];
    position[2] = oldPosition[2];
    float azimuth = oldAzimuth;
    float tankAngVel = BZDB.eval(StateDatabase::BZDB_TANKANGVEL);
    float tankSpeed = BZDBCache::tankSpeed;

    // basically a clone of Roger's evasive code
    for (int t = 0; t <= World::getWorld()->getCurMaxPlayers(); t++) {
      Player *p = 0;
      if (t < World::getWorld()->getCurMaxPlayers())
        p = World::getWorld()->getPlayer(t);
      else
        p = LocalPlayer::getMyTank();
      if (!p || p->getId() == getId())
        continue;
      const int maxShots = p->getMaxShots();
      for (int s = 0; s < maxShots; s++) {
        ShotPath* shot = p->getShot(s);
        if (!shot || shot->isExpired())
          continue;
        // ignore invisible bullets completely for now (even when visible)
        if (shot->getFlag() == Flags::InvisibleBullet)
          continue;

        const float* shotPos = shot->getPosition();
        if ((fabs(shotPos[2] - position[2]) > BZDBCache::tankHeight)
            && (shot->getFlag() != Flags::GuidedMissile))
          continue;
        const float dist = TargetingUtils::getTargetDistance(position, shotPos);
        if (dist < 150.0f) {
          const float *shotVel = shot->getVelocity();
          float shotAngle = atan2f(shotVel[1], shotVel[0]);
          float shotUnitVec[2] = { cosf(shotAngle), sinf(shotAngle) };

          float trueVec[2] =
              { (position[0] - shotPos[0]) / dist, (position[1] - shotPos[1]) / dist };
          float dotProd = trueVec[0] * shotUnitVec[0] + trueVec[1] * shotUnitVec[1];

          if (dotProd > 0.97f) {
            float rotation;
            float rotation1 = (float) ((shotAngle + M_PI / 2.0) - azimuth);
            if (rotation1 < -1.0f * M_PI)
              rotation1 += (float) (2.0 * M_PI);
            if (rotation1 > 1.0f * M_PI)
              rotation1 -= (float) (2.0 * M_PI);

            float rotation2 = (float) ((shotAngle - M_PI / 2.0) - azimuth);
            if (rotation2 < -1.0f * M_PI)
              rotation2 += (float) (2.0 * M_PI);
            if (rotation2 > 1.0f * M_PI)
              rotation2 -= (float) (2.0 * M_PI);

            if (fabs(rotation1) < fabs(rotation2))
              rotation = rotation1;
            else
              rotation = rotation2;
            setDesiredSpeed(1.0f);
            setDesiredAngVel(rotation);
            evading = true;
          }
        }
      }  // for s
    }  // for t

    // when we are not evading, follow the path
    if (!evading && dt > 0.0 && pathIndex < (int) path.size()) {
      float distance;
      float v[2];
      const float* endPoint = path[pathIndex].get();
      // find how long it will take to get to next path segment
      v[0] = endPoint[0] - position[0];
      v[1] = endPoint[1] - position[1];
      distance = hypotf(v[0], v[1]);
      float tankRadius = BZDBCache::tankRadius;
      // smooth path a little by turning early at corners, might get us stuck, though
      if (distance <= 2.5f * tankRadius)
        pathIndex++;

      float segmentAzimuth = atan2f(v[1], v[0]);
      float azimuthDiff = segmentAzimuth - azimuth;
      if (azimuthDiff > M_PI)
        azimuthDiff -= (float) (2.0 * M_PI);
      else if (azimuthDiff < -M_PI)
        azimuthDiff += (float) (2.0 * M_PI);
      if (fabs(azimuthDiff) > 0.01f) {
        // drive backward when target is behind, try to stick to last direction
        if (drivingForward)
          drivingForward = fabs(azimuthDiff) < M_PI / 2 * 0.9 ? true : false;
        else
          drivingForward = fabs(azimuthDiff) < M_PI / 2 * 0.3 ? true : false;
        setDesiredSpeed(drivingForward ? 1.0f : -1.0f);
        // set desired turn speed
        if (azimuthDiff >= dt * tankAngVel) {
          setDesiredAngVel(1.0f);
        }
        else if (azimuthDiff <= -dt * tankAngVel) {
          setDesiredAngVel(-1.0f);
        }
        else {
          setDesiredAngVel(azimuthDiff / dt / tankAngVel);
        }
      }
      else {
        drivingForward = true;
        // tank doesn't turn while moving forward
        setDesiredAngVel(0.0f);
        // find how long it will take to get to next path segment
        if (distance <= dt * tankSpeed) {
          pathIndex++;
          // set desired speed
          setDesiredSpeed(distance / dt / tankSpeed);
        }
        else {
          setDesiredSpeed(1.0f);
        }
      }
    }  // if !evading
  }  // if isAlive()
  LocalPlayer::doUpdateMotion(dt);
}
*/

void RobotPlayer::explodeTank() {
  LocalPlayer::explodeTank();
  target = NULL;
  path.clear();
}

void RobotPlayer::restart(const float* pos, float _azimuth) {
  LocalPlayer::restart(pos, _azimuth);
  // no target
  path.clear();
  target = NULL;
  pathIndex = 0;

}

float RobotPlayer::getTargetPriority(const Player* _target) const {
  // don't target teammates or myself
  if (!this->validTeamTarget(_target))
    return 0.0f;

  // go after closest player
  // FIXME -- this is a pretty stupid heuristic
  const float worldSize = BZDBCache::worldSize;
  const float* p1 = getPosition();
  const float* p2 = _target->getPosition();

  float basePriority = 1.0f;
  // give bonus to non-paused player
  if (!_target->isPaused())
    basePriority += 2.0f;
  // give bonus to non-deadzone targets
  if (obstacleList) {
    float nearest[2];
    const BzfRegion* targetRegion = findRegion(p2, nearest);
    if (targetRegion && targetRegion->isInside(p2))
      basePriority += 1.0f;
  }
  return basePriority - 0.5f * hypotf(p2[0] - p1[0], p2[1] - p1[1]) / worldSize;
}

void RobotPlayer::setObstacleList(std::vector<BzfRegion*>* _obstacleList) {
  obstacleList = _obstacleList;
  aicore::DecisionTrees::init();  // MY CODE
}

const Player* RobotPlayer::getTarget() const {
  return target;
}

/* ========== ORIGINAL setTarget ==========
void RobotPlayer::setTarget(const Player* _target) {
  static int mailbox = 0;

  path.clear();
  target = _target;
  if (!target)
    return;

  // work backwards (from target to me)
  float proj[3];
  getProjectedPosition(target, proj);
  const float *p1 = proj;
  const float* p2 = getPosition();
  float q1[2], q2[2];
  BzfRegion* headRegion = findRegion(p1, q1);
  BzfRegion* tailRegion = findRegion(p2, q2);
  if (!headRegion || !tailRegion) {
    // if can't reach target then forget it
    return;
  }

  mailbox++;
  headRegion->setPathStuff(0.0f, NULL, q1, mailbox);
  RegionPriorityQueue queue;
  queue.insert(headRegion, 0.0f);
  BzfRegion* next;
  while (!queue.isEmpty() && (next = queue.remove()) != tailRegion)
    findPath(queue, next, tailRegion, q2, mailbox);

  // get list of points to go through to reach the target
  next = tailRegion;
  do {
    p1 = next->getA();
    path.push_back(p1);
    next = next->getTarget();
  } while (next && next != headRegion);
  if (next || tailRegion == headRegion)
    path.push_back(q1);
  else
    path.clear();
  pathIndex = 0;
}
*/

BzfRegion* RobotPlayer::findRegion(const float p[2], float nearest[2]) const {
  nearest[0] = p[0];
  nearest[1] = p[1];
  const int count = obstacleList->size();
  for (int o = 0; o < count; o++)
    if ((*obstacleList)[o]->isInside(p))
      return (*obstacleList)[o];

  // point is outside: find nearest region
  float distance = maxDistance;
  BzfRegion* nearestRegion = NULL;
  for (int i = 0; i < count; i++) {
    float currNearest[2];
    float currDistance = (*obstacleList)[i]->getDistance(p, currNearest);
    if (currDistance < distance) {
      nearestRegion = (*obstacleList)[i];
      distance = currDistance;
      nearest[0] = currNearest[0];
      nearest[1] = currNearest[1];
    }
  }
  return nearestRegion;
}

float RobotPlayer::getRegionExitPoint(const float p1[2], const float p2[2], const float a[2],
    const float targetPoint[2], float mid[2], float& priority) {
  float b[2];
  b[0] = targetPoint[0] - a[0];
  b[1] = targetPoint[1] - a[1];
  float d[2];
  d[0] = p2[0] - p1[0];
  d[1] = p2[1] - p1[1];

  float vect = d[0] * b[1] - d[1] * b[0];
  float t = 0.0f;  // safe value
  if (fabs(vect) > ZERO_TOLERANCE) {
    // compute intersection along (p1,d) with (a,b)
    t = (a[0] * b[1] - a[1] * b[0] - p1[0] * b[1] + p1[1] * b[0]) / vect;
    if (t > 1.0f)
      t = 1.0f;
    else if (t < 0.0f)
      t = 0.0f;
  }

  mid[0] = p1[0] + t * d[0];
  mid[1] = p1[1] + t * d[1];

  const float distance = hypotf(a[0] - mid[0], a[1] - mid[1]);
  // priority is to minimize distance traveled and distance left to go
  priority = distance + hypotf(targetPoint[0] - mid[0], targetPoint[1] - mid[1]);
  // return distance traveled
  return distance;
}

void RobotPlayer::findPath(RegionPriorityQueue& queue, BzfRegion* region, BzfRegion* targetRegion,
    const float targetPoint[2], int mailbox) {
  const int numEdges = region->getNumSides();
  for (int i = 0; i < numEdges; i++) {
    BzfRegion* neighbor = region->getNeighbor(i);
    if (!neighbor)
      continue;

    const float* p1 = region->getCorner(i).get();
    const float* p2 = region->getCorner((i + 1) % numEdges).get();
    float mid[2], priority;
    float total = getRegionExitPoint(p1, p2, region->getA(), targetPoint, mid, priority);
    priority += region->getDistance();
    if (neighbor == targetRegion)
      total += hypotf(targetPoint[0] - mid[0], targetPoint[1] - mid[1]);
    total += region->getDistance();
    if (neighbor->test(mailbox) || total < neighbor->getDistance()) {
      neighbor->setPathStuff(total, region, mid, mailbox);
      queue.insert(neighbor, priority);
    }
  }
}

// ========== MY CODE BELOW ==========

void RobotPlayer::setTarget(const Player* _target) {
  target = _target;

// this seems to help it shoot the user better
//  if (!target) {
//    target = LocalPlayer::getMyTank();
//  }

  TeamColor myTeam = this->getTeam();

  // figure out each team's lowest and highest player ID
  // store in static array, so we don't have to keep doing it
  if (0 == teamLowestId[myTeam] && 0 == teamHighestId[myTeam]) {
    World *world = World::getWorld();
    Player *p = 0;
    // start at i=1 b/c don't need to worry about users
    for (int i=1; i < world->getCurMaxPlayers(); i++) {
      p = world->getPlayer(i);
      // if player exists and is on my team
      if (p && p->getTeam() == myTeam) {
        if (0 == teamLowestId[myTeam] || p->getId() < teamLowestId[myTeam])
          teamLowestId[myTeam]  = p->getId();
        if (p->getId() > teamHighestId[myTeam])
          teamHighestId[myTeam] = p->getId();
      }
    }  // for
  }

  float dt = 0;
  aicore::DecisionTreeNode *node = aicore::DecisionTrees::assignRoleDecisions[0].makeDecision(this, dt);
  (this->*(((aicore::ActionPtr*)node)->actFuncPtr))(dt);
}


void RobotPlayer::doUpdateMotion(float dt) {

  aicore::DecisionTreeNode *node = aicore::DecisionTrees::doUpdateMotionDecisions[0].makeDecision(this, dt);
  (this->*(((aicore::ActionPtr*)node)->actFuncPtr))(dt);

  LocalPlayer::doUpdateMotion(dt);
}


void RobotPlayer::doUpdate(float dt) {
  LocalPlayer::doUpdate(dt);

  aicore::DecisionTreeNode *node = aicore::DecisionTrees::shootDecisions[0].makeDecision(this, dt);
  (this->*(((aicore::ActionPtr*)node)->actFuncPtr))(dt);

  node = aicore::DecisionTrees::dropFlagDecisions[0].makeDecision(this, dt);
  (this->*(((aicore::ActionPtr*)node)->actFuncPtr))(dt);
}


// -------------------- path-finding helpers --------------------

/* Uses A* search to find a path from the start node to the goal node.  ALSO smooths the path.
 * WARNING: will crash if the goal (or start?) node is inaccessible.
 * @param myPath Writes the path to this vector of nodes.
 * @param start The node from which to start the A* search.
 * @param goal The A* search will try to find a path to this node.
 */
void RobotPlayer::findPath(std::vector<MyNode> &myPath, MyNode start, MyNode goal) {
  // set up the graph function container
  int halfWorldSize = (int)(0.5f * BZDBCache::worldSize);
  GraphFunctionContainer container(halfWorldSize);

  // create the graph
  GenericSearchGraphDescriptor<MyNode, double> graph;
  graph.func_container = &container;
  graph.hashTableSize = MAX_HASH_VAL + 1;
  graph.SeedNode = start;
  graph.TargetNode = goal;

  // find the path
  A_star_planner<MyNode,double> planner;
  planner.init(graph);
  planner.plan();
  std::vector< std::vector<MyNode> > paths = planner.getPlannedPaths();

  // reverse paths[0]
  // paths[0] contains the path, but backwards
  //   paths[0][0]       == goal node
  //   paths[0].size()-1 == start node
  //   paths[0].size()-2 == first node to go to
  myPath.clear();
  for (int i=paths[0].size()-1; i >=0; i--) {
    myPath.push_back(paths[0][i]);
  }
  // smooth the path
  myPath = this->smoothPath(myPath);
}


/* If an earlier node and a later node in the input path have a clear line of sight, then store
 * these nodes (and skip the nodes in-between) in the output path as the smoothed path.
 * @param inputPath The path to smooth.
 * @return The smoothed path.
 */
std::vector<MyNode> RobotPlayer::smoothPath(std::vector<MyNode> inputPath) {
  if (inputPath.size() <= 2) {
    // nothing to do
    return inputPath;
  }

  std::vector<MyNode> outputPath;
  outputPath.push_back(inputPath[0]);
  // start at 2 b/c there is already a connection btwn 0 and 1
  // stop at size-1 b/c last node must be added anyway, so no need to check
  for (int i=2; i<inputPath.size()-1; i++) {
    MyNode fromNode = outputPath[outputPath.size()-1];
    MyNode toNode = inputPath[i];
    float fromPos[2] = {convertToGameCoord(fromNode.x), convertToGameCoord(fromNode.y)};
    float toPos[2]   = {convertToGameCoord(toNode.x),   convertToGameCoord(toNode.y)};
    if (this->obstructedLineOfSight(fromPos, toPos)) {
      outputPath.push_back(inputPath[i-1]);
    }
  }
  outputPath.push_back(inputPath[inputPath.size()-1]);
  return outputPath;
}


/* Checks the line of sight between the given positions to see if any obstacles are in the way.
 * @param fromPos2D The position that we are checking the line-of-sight from.
 * @param toPos2D The position that we are looking toward.
 * @return True if the line of sight is obstructed, false otherwise
 */
bool RobotPlayer::obstructedLineOfSight(const float *fromPos2D, const float *toPos2D) {
  float fromPos[3] = {fromPos2D[0], fromPos2D[1], 0.0f};
  float toPos[3]   = {toPos2D[0],   toPos2D[1],   0.0f};

  float dist = hypotf(toPos[0]-fromPos[0], toPos[1]-fromPos[1]);
  float direction[3] = {(toPos[0]-fromPos[0])/dist, (toPos[1]-fromPos[1])/dist, 0.0f};
  Ray myRay(fromPos, direction);

  return (ShotStrategy::getFirstBuilding(myRay, 0.0f, dist) != NULL);
}



// -------------------- decision tree, general functions --------------------

/* Indicates whether or not the tank is alive.
 * @param dt The time since the last frame.
 * @return True if the tank is alive, false otherwise.
 */
bool RobotPlayer::tankIsAlive(float dt) {
  return Player::isAlive();
}


// TODO: will this still work if it's not capture the flag mode?
/* Indicates whether or not the tank is holding the tank's team flag.
 * @param dt The time since the last frame.
 * @return True if the tank is holding the tank's team flag, false otherwise.
 */
bool RobotPlayer::hasMyTeamFlag(float dt) {
  FlagType* myFlag = this->getFlag();
  return (myFlag->flagTeam == this->getTeam());
}


/* Indicates whether or not the tank's role is to guard the flag.
 * @param dt The time since the last frame.
 * @return True if the tank's role is to guard the flag, false otherwise.
 */
bool RobotPlayer::isGuardingFlag(float dt) {
  return this->hasLowestId(dt);
}


/* Does nothing.  Needed because the ActionPtrs need a function to point to.
 * @param dt The time since the last frame.
 */
void RobotPlayer::doNothing(float dt) {
  //controlPanel->addMessage("do nothing");
}


// -------------------- setTarget, assign roles to tanks --------------------

/* Indicates whether or not the tank has the lowest ID on its team.
 * I use this to help assign different roles to different tanks.
 * @param dt The time since the last frame.
 * @return True if the tank has the lowest ID on its team, false otherwise.
 */
bool RobotPlayer::hasLowestId(float dt) {
  TeamColor myTeam = this->getTeam();
  return (this->getId() == RobotPlayer::teamLowestId[myTeam]);
}


/* Indicates whether or not the tank has the highest ID on its team.
 * I use this to help assign different roles to different tanks.
 * @param dt The time since the last frame.
 * @return True if the tank has the highest ID on its team, false otherwise.
 */
bool RobotPlayer::hasHighestId(float dt) {
  TeamColor myTeam = this->getTeam();
  return (this->getId() == RobotPlayer::teamHighestId[myTeam]);
}


/* Sets the goal and path for the guard-team-flag role.
 * Only one tank has this role.
 * If this tank has its team flag, find a path to its team base.
 * If it doesn't have its team flag, find a path to the team flag.
 * @param dt The time since the last frame.
 */
void RobotPlayer::setRoleGuardFlag(float dt) {
  const float* myPos  = this->getPosition();
  TeamColor    myTeam = this->getTeam();
  World *world = World::getWorld();

  if (this->hasMyTeamFlag(dt)) {
    // return to base
    const float* basePos = world->getBase(myTeam, 0);
    MyNode tempNode(basePos[0], basePos[1]);
    RobotPlayer::guardGoal[myTeam] = tempNode;
  }
  else {

    // not capture the flag mode
    // this tank has nothing to do
    // guardGoal will have default value and guardPath will be empty
    // ?this tank's myGoal will have default value?
    // ?this tank's path will be empty?
    // when doUpdateMotion() is called, nothing should happen
    if (!(world->allowTeamFlags())) {
      return;
    }

    Flag flag;
    for (int i=0; i<world->getMaxFlags(); i++) {
      flag = world->getFlag(i);
      TeamColor flagTeam = flag.type->flagTeam;
      if (flagTeam == myTeam)
        break;
    }
    MyNode tempNode(flag.position[0], flag.position[1]);
    RobotPlayer::guardGoal[myTeam] = tempNode;
  }

  // if a path doesn't exist yet, or if the goal changed
  // use A* search to find a path
  if (guardPath[myTeam].empty() || !(guardPath[myTeam].back() == guardGoal[myTeam])) {
    MyNode startNode(myPos[0], myPos[1]);
    this->findPath(guardPath[myTeam], startNode, guardGoal[myTeam]);
  }

  this->assignRole(guardGoal[myTeam], guardPath[myTeam]);
}


/* Sets the goal and path for the capture-enemy-flags role.
 * Only one tank has this role.
 * If this tank doesn't have an enemy flag, find a path to the enemy flag that is closest to
 * this tank (rather than the center of mass, since only one tank is seeking the flag).
 * Otherwise, return to base.
 * Note that if this tank has its own team flag, it will return to base.
 * Should work even if not capture-the-flag mode.
 * @param dt The time since the last frame.
 */
void RobotPlayer::setRoleCaptureFlags(float dt) {
  const float *myPos  = this->getPosition();
  TeamColor    myTeam = this->getTeam();
  FlagType    *myFlag = this->getFlag();

  // if our team doesn't have an enemy flag (and enemy flags exist)
  std::vector<Flag> enemyFlags = this->findAllEnemyFlags();
  if (enemyFlags.size() && ((myFlag == Flags::Null) || (myFlag->flagTeam == NoTeam))) {
    // find the enemy flag that is closest to this tank
    Flag goalFlag = enemyFlags[0];
    float goalDistance = hypotf(goalFlag.position[0] - myPos[0], goalFlag.position[1] - myPos[1]);
    for (std::vector<Flag>::iterator i = enemyFlags.begin(); i<enemyFlags.end(); i++) {
      Flag temp = *i;
      float tempDistance = hypotf(temp.position[0] - myPos[0], temp.position[1] - myPos[1]);
      if (tempDistance < goalDistance) {
        goalFlag = temp;
        goalDistance = tempDistance;
      }
    } // for

    MyNode tempNode(goalFlag.position[0], goalFlag.position[1]);
    RobotPlayer::captureGoal[myTeam] = tempNode;
  }
  // go back to base
  else {
    const float* basePos = World::getWorld()->getBase(myTeam, 0);
    MyNode tempNode(basePos[0], basePos[1]);
    RobotPlayer::captureGoal[myTeam] = tempNode;
  }

  // if a path doesn't exist yet, or if the goal changed
  // use A* search to find a path
  if (capturePath[myTeam].empty() || !(capturePath[myTeam].back() == captureGoal[myTeam])) {
    MyNode startNode(myPos[0], myPos[1]);
    this->findPath(capturePath[myTeam], startNode, captureGoal[myTeam]);
  }

  this->assignRole(captureGoal[myTeam], capturePath[myTeam]);
}


// TODO: this works, but not well.  Figure out something better.
/* Finds a path toward the enemy target.
 * Multiple tanks can have this role.
 * The target is determined by the default targeting code.
 * Each tank might have a different target, so each tank will find its own path.
 * @param dt The time since the last frame.
 */
void RobotPlayer::setRoleKillEnemies(float dt) {
  const float *myPos  = this->getPosition();
  TeamColor    myTeam = this->getTeam();

  if (!(this->target))
    return;

  /* pick the first enemy we find
  World *world = World::getWorld();
  Player *p = 0;
  // start at i=1 b/c player 0 is the local player, which doesn't apply here
  for (int i=1; i < world->getCurMaxPlayers(); i++) {
    p = world->getPlayer(i);
    // if player is exists and is not on my team
    if (p && p->getTeam() != this->getTeam()) {
      break;
    }
  }  // for
  */

  const char* targetname = target->getCallSign();
  const char* tankname   = this->getCallSign();
  char buffer[1234];
  sprintf(buffer, "%s -> %s", tankname, targetname);
  controlPanel->addMessage(buffer);

  const float *enemyPos = this->target->getPosition();
  MyNode tempNode(enemyPos[0], enemyPos[1]);
  this->myGoal = tempNode;

  // if the goal changed, force an update to the path
  if (!(this->prevGoal == this->myGoal)) {
    this->prevGoal = this->myGoal;
    this->path.clear();
    RegionPoint rp(convertToGameCoord(this->myGoal.x), convertToGameCoord(this->myGoal.y));
    this->path.push_back(rp);
    // tank will find it's own path to the goal when it calls checkLineOfSight()
  }
}


/* Copies the role goal and role path into the tank's individual goal and path.
 * @param roleGoal The role's goal node.
 * @param rolePath The role's path to the goal node.
 */
void RobotPlayer::assignRole(MyNode roleGoal, std::vector<MyNode> rolePath) {
  // if the tank's goal is not the same as the role goal
  // (this would be true initially when the tank doesn't have a path yet)
  // (or maybe the role goal changed)
  if (!(this->myGoal == roleGoal)) {
    this->path.clear();
    this->pathIndex = 0;

    // copy the role path into tank's path, and set tank's goal to be role goal
    for (int i=0; i<rolePath.size(); i++) {
      MyNode node = rolePath[i];
      RegionPoint rp(convertToGameCoord(node.x), convertToGameCoord(node.y));
      this->path.push_back(rp);
    }
    this->myGoal = roleGoal;
  }

  // tank goal == role goal, but the tank doesn't have a path there
  // this should only be true if the tank exploded
  // set path = role's goal node
  // tank will find its own path there when it calls checkLineOfSight()
  if (this->path.empty()) {
    RegionPoint rp(convertToGameCoord(roleGoal.x), convertToGameCoord(roleGoal.y));
    this->path.push_back(rp);
  }
}


// -------------------- role helpers --------------------

/* Identifies all enemy flags, and stores them into a vector.
 * @return A vector containing the enemy flags.
 */
std::vector<Flag> RobotPlayer::findAllEnemyFlags() {
  std::vector<Flag> enemyFlags;
  World *world = World::getWorld();
  // team flags exist (capture the flag mode)
  if (world->allowTeamFlags()) {
    for (int i=0; i<world->getMaxFlags(); i++) {
      Flag flag = world->getFlag(i);
      TeamColor flagTeam = flag.type->flagTeam;
      // if the flag is on some team, but not on our team
      if (flagTeam != NoTeam && flagTeam != this->getTeam())
        enemyFlags.push_back(flag);
    } // for
  }
  return enemyFlags;
}


// -------------------- doUpdateMotion --------------------

/* Indicates whether or not a shot is coming near the tank.
 * @param dt The time since the last frame.
 * @return True if a shot is coming near the tank, false otherwise.
 */
bool RobotPlayer::shotComing(float dt) {
  const float* position = this->getPosition();
  const float  azimuth  = this->getAngle();

  for (int t = 0; t <= World::getWorld()->getCurMaxPlayers(); t++) {
    Player *p = 0;
    if (t < World::getWorld()->getCurMaxPlayers())
      p = World::getWorld()->getPlayer(t);
    else
      p = LocalPlayer::getMyTank();
    if (!p || p->getId() == getId())
      continue;
    const int maxShots = p->getMaxShots();
    for (int s = 0; s < maxShots; s++) {
      ShotPath* shot = p->getShot(s);
      if (!shot || shot->isExpired())
        continue;
      // ignore invisible bullets completely for now (even when visible)
      if (shot->getFlag() == Flags::InvisibleBullet)
        continue;

      const float* shotPos = shot->getPosition();
      if ((fabs(shotPos[2] - position[2]) > BZDBCache::tankHeight)
          && (shot->getFlag() != Flags::GuidedMissile))
        continue;
      const float dist = TargetingUtils::getTargetDistance(position, shotPos);
      if (dist < 150.0f) {
        const float *shotVel = shot->getVelocity();
        this->incomingShotAngle = atan2f(shotVel[1], shotVel[0]);
        float shotUnitVec[2] = { cosf(this->incomingShotAngle), sinf(this->incomingShotAngle) };

        float trueVec[2] =
            { (position[0] - shotPos[0]) / dist, (position[1] - shotPos[1]) / dist };
        float dotProd = trueVec[0] * shotUnitVec[0] + trueVec[1] * shotUnitVec[1];

        if (dotProd > 0.97f) {
          return true;
        }
      }
    }  // for s
  }  // for t
  return false;
}


// TODO: magic number!
/* Indicates whether or not the tank is at its team base.
 * @param dt The time since the last frame.
 * @return True if the tank is at its team base, false otherwise.
 */
bool RobotPlayer::isAtTeamBase(float dt) {
  TeamColor    myTeam  = this->getTeam();
  const float* myPos   = this->getPosition();
  const float* basePos = World::getWorld()->getBase(myTeam, 0);

  return (hypotf(basePos[0] - myPos[0], basePos[1] - myPos[1]) < 10.0f);
}


/* Turn and move away from the incoming shot.
 * @param dt The time since the last frame.
 */
void RobotPlayer::evade(float dt) {
  const float azimuth = this->getAngle();

  float rotation1 = (float) ((this->incomingShotAngle + M_PI / 2.0) - azimuth);
  if (rotation1 < -1.0f * M_PI)
    rotation1 += (float) (2.0 * M_PI);
  if (rotation1 > 1.0f * M_PI)
    rotation1 -= (float) (2.0 * M_PI);

  float rotation2 = (float) ((this->incomingShotAngle - M_PI / 2.0) - azimuth);
  if (rotation2 < -1.0f * M_PI)
    rotation2 += (float) (2.0 * M_PI);
  if (rotation2 > 1.0f * M_PI)
    rotation2 -= (float) (2.0 * M_PI);

  float rotation;
  if (fabs(rotation1) < fabs(rotation2))
    rotation = rotation1;
  else
    rotation = rotation2;
  setDesiredSpeed(1.0f);
  setDesiredAngVel(rotation);
}


/* Rotates the tank to face in the direction of its closest enemy.
 * Does nothing if enemies don't exist.
 * @param dt The time since the last frame.
 */
void RobotPlayer::aimAtClosestEnemy(float dt) {
  TeamColor    myTeam  = this->getTeam();
  const float* myPos   = this->getPosition();
  const float  azimuth = this->getAngle();
  const float  tankAngVel = BZDB.eval(StateDatabase::BZDB_TANKANGVEL);

  float enemyPos[3];
  bool enemiesExist = this->findClosestEnemy(enemyPos);
  if (!enemiesExist)
    return;

  float segmentAzimuth = atan2f(enemyPos[1] - myPos[1] , enemyPos[0] - myPos[0]);
  float azimuthDiff = segmentAzimuth - azimuth;
  if (azimuthDiff > M_PI)
    azimuthDiff -= (float) (2.0 * M_PI);
  else if (azimuthDiff < -M_PI)
    azimuthDiff += (float) (2.0 * M_PI);
  if (fabs(azimuthDiff) > 0.01f) {
    if (azimuthDiff >= dt * tankAngVel) {
      setDesiredAngVel(1.0f);
    }
    else if (azimuthDiff <= -dt * tankAngVel) {
      setDesiredAngVel(-1.0f);
    }
    else {
      setDesiredAngVel(azimuthDiff / dt / tankAngVel);
    }
  }
  else {
    setDesiredAngVel(0.0f);
  }
  setDesiredSpeed(0.0f);
}


/* Finds the position of the enemy that is closest to this tank.
 * @param enemyPos Writes the position of the closest enemy to this float array.
 * @return True if an enemy could be found, false otherwise.
 */
bool RobotPlayer::findClosestEnemy(float enemyPos[3]) {
  const float* myPos = this->getPosition();

  float minDist = 2 * BZDBCache::worldSize;
  Player* closestEnemy = 0;

  World* world = World::getWorld();
  // start at i=1 b/c player 0 is the local player, which doesn't apply here
  for (int i=1; i < world->getCurMaxPlayers(); i++) {
    Player* p = world->getPlayer(i);
    // if enemy exists
    if (p && p->getTeam() != this->getTeam()) {
      const float* pos = p->getPosition();
      float dist = hypotf(pos[0]-myPos[0], pos[1]-myPos[1]);
      if (dist < minDist) {
        minDist = dist;
        closestEnemy = p;
      }
    }
  }  // for

  // no enemies found!
  if (0 == closestEnemy)
    return false;

  const float* closestEnemyPos = closestEnemy->getPosition();
  enemyPos[0] = closestEnemyPos[0];
  enemyPos[1] = closestEnemyPos[1];
  enemyPos[2] = closestEnemyPos[2];
  return true;
}


/* Follow the tank's path toward the team goal.
 * @param dt The time since the last frame.
 */
void RobotPlayer::followPath(float dt) {
  const float tankRadius = BZDBCache::tankRadius;
  const float tankSpeed  = BZDBCache::tankSpeed;
  const float tankAngVel = BZDB.eval(StateDatabase::BZDB_TANKANGVEL);

  const float* position = this->getPosition();
  const float  azimuth  = this->getAngle();

  if (dt > 0.0 && pathIndex < (int)path.size()) {

    this->checkLineOfSight();

    // find how long it will take to get to next path segment
    const float* nextPoint = path[pathIndex].get();
    float relPosToNextNode[2] = {nextPoint[0] - position[0], nextPoint[1] - position[1]};
    float distanceToNextNode = hypotf(relPosToNextNode[0], relPosToNextNode[1]);
    // smooth path a little by turning early at corners, might get us stuck, though
    //if (distance <= 2.5f * tankRadius)
    if (distanceToNextNode <= dt * tankSpeed + tankRadius) {
        pathIndex++;
    }

    float separationV[3] = {0};
    this->getSeparation(separationV);

    // v is a relative position, according to the original implementation
    float v[2];
    v[0] = PATH_WEIGHT * relPosToNextNode[0] + SEPARATION_WEIGHT * separationV[0];
    v[1] = PATH_WEIGHT * relPosToNextNode[1] + SEPARATION_WEIGHT * separationV[1];
    // normalize v
// QUESTION: is there a point to normalizing v?
    int weight = PATH_WEIGHT + SEPARATION_WEIGHT;
    v[0] /= weight;
    v[1] /= weight;

    float segmentAzimuth = atan2f(v[1], v[0]);
    float azimuthDiff = segmentAzimuth - azimuth;
    if (azimuthDiff > M_PI)
      azimuthDiff -= (float) (2.0 * M_PI);
    else if (azimuthDiff < -M_PI)
      azimuthDiff += (float) (2.0 * M_PI);
    if (fabs(azimuthDiff) > 0.01f) {
      // drive backward when target is behind, try to stick to last direction
      if (drivingForward)
        drivingForward = fabs(azimuthDiff) < M_PI / 2 * 0.9 ? true : false;
      else
        drivingForward = fabs(azimuthDiff) < M_PI / 2 * 0.3 ? true : false;
      setDesiredSpeed(drivingForward ? 1.0f : -1.0f);
      // set desired turn speed
      if (azimuthDiff >= dt * tankAngVel) {
        setDesiredAngVel(1.0f);
      }
      else if (azimuthDiff <= -dt * tankAngVel) {
        setDesiredAngVel(-1.0f);
      }
      else {
        setDesiredAngVel(azimuthDiff / dt / tankAngVel);
      }
    }
    else {
      drivingForward = true;
      // QUESTION: do i need this?
      //setDesiredSpeed(1.0f);
      // tank doesn't turn while moving forward
      setDesiredAngVel(0.0f);
    }
  } // if (dt > 0.0 && pathIndex < (int) path.size())
/*
  // to prevent overshooting the target
  else if (dt > 0.0 && pathIndex >= (int) path.size()) {
    pathIndex = ((int)path.size()) - 1;
  }
*/
}
// TODO: expand code to see question


/* If the line of sight from the tank's current position to the next node is blocked,
 * then use A* search to find a path to the next node, and insert it into the tank's path
 */
void RobotPlayer::checkLineOfSight() {
  if (path.empty()) {
    return;
  }

  const float *fromPos = this->getPosition();
  const float *toPos   = path[pathIndex].get();
  if (this->obstructedLineOfSight(fromPos, toPos)) {
    std::vector<MyNode> prePath;
    MyNode fromNode(fromPos[0], fromPos[1]);
    MyNode toNode  (toPos[0],   toPos[1]);

    this->findPath(prePath, fromNode, toNode);
    if (prePath.size() > 2) {
      // don't insert the first node because it is the tank's current position
      // don't insert the last  node because it is already in the path
      for (int i=prePath.size()-2; i>0; i--) {
        MyNode node = prePath[i];
        RegionPoint rp(convertToGameCoord(node.x), convertToGameCoord(node.y));
        path.insert(path.begin(), rp);
      }
    }  // if prePath.size > 2
  }  // if obstructedLineOfSight
}


/* Calculates the separation vector.
 * @param separationVel Writes the separation vector to this array.
 */
void RobotPlayer::getSeparation(float separationV[3]) {
  separationV[0] = separationV[1] = separationV[2] = 0.0f;
  float sumCoordsNearby[2] = {0};
  int   numPlayersNearby   = 0;

  World *world = World::getWorld();
  const float *myPos = this->getPosition();
  Player *p = 0;

  // sum up the coordinates of tanks within the separation threshold radius
  // excludes self
  for (int i=0; i < world->getCurMaxPlayers(); i++) {
    // player 0 is reserved for the user, and getPlayer(0) will always be null
    if (0 == i)
      p = LocalPlayer::getMyTank();
    else
      p = world->getPlayer(i);
    // if player is exists, is not self, and is on same team
    if (p && p->getId() != this->getId() && p->getTeam() == this->getTeam()) {
      const float *pos = p->getPosition();
      float distance = hypotf(pos[0] - myPos[0], pos[1] - myPos[1]);
      if (distance <= SEPARATION_THRESHOLD) {
        sumCoordsNearby[0] += pos[0];
        sumCoordsNearby[1] += pos[1];
        numPlayersNearby++;
      }
    } // if
  } // for

  // calculate the center of mass of tanks within the threshold radius, excluding self
  // calculate the vector away from this local center of mass
  if (numPlayersNearby) {
    float nearbyCOM[2] = {sumCoordsNearby[0]/numPlayersNearby, sumCoordsNearby[1]/numPlayersNearby};
    // position - center of mass, because we want to move AWAY from this local center of mass
    float relativePosNearbyCOM[2] = {myPos[0] - nearbyCOM[0], myPos[1] - nearbyCOM[1]};
    separationV[0] = relativePosNearbyCOM[0];
    separationV[1] = relativePosNearbyCOM[1];

    // velocity vector = unit direction vector * speed
    //float distanceNearbyCOM = hypotf(relativePosNearbyCOM[0], relativePosNearbyCOM[1]);
    //separationV[0] = (relativePosNearbyCOM[0] / distanceNearbyCOM) * BZDBCache::tankSpeed;
    //separationV[1] = (relativePosNearbyCOM[1] / distanceNearbyCOM) * BZDBCache::tankSpeed;
  }
}



// -------------------- doUpdate, shoot --------------------

/* Indicates whether or not the tank's firing status is ready.
 * @param dt The time since the last frame.
 * @return True if the tank's firing status is ready, false otherwise.
 */
bool RobotPlayer::readyToFire(float dt) {
  return (LocalPlayer::getFiringStatus() == LocalPlayer::Ready);
}


/* Indicates whether or not the tank's timer for shooting has elapsed.
 * @param dt The time since the last frame.
 * @return True if the tank's timer for shooting has elapsed, false otherwise.
 */
bool RobotPlayer::shotTimerElapsed(float dt) {
  this->timerForShot -= dt;
  if (this->timerForShot < 0.0f)
    this->timerForShot = 0.0f;

  return (this->timerForShot <= 0.0f);
}


/* Indicates whether or not the predicted shot will miss by less than half a tank length.
 * @param dt The time since the last frame.
 * @return True if the predicted shot will miss by less than half a tank length, false otherwise.
 */
bool RobotPlayer::willBarelyMiss(float dt) {
  // if there is no target, then there is nothing to shoot
  if (!this->target) {
    return false;
  }

  const float shotRadius = BZDB.eval(StateDatabase::BZDB_SHOTRADIUS);

  float p1[3];
  getProjectedPosition(this->target, p1);
  const float* p2 = getPosition();
  const float azimuth = getAngle();

  float shootingAngle = atan2f(p1[1] - p2[1], p1[0] - p2[0]);
  if (shootingAngle < 0.0f)
    shootingAngle += (float) (2.0 * M_PI);

  float azimuthDiff = shootingAngle - azimuth;
  if (azimuthDiff > M_PI)
    azimuthDiff -= (float) (2.0 * M_PI);
  else if (azimuthDiff < -M_PI)
    azimuthDiff += (float) (2.0 * M_PI);

  this->distanceToTarget =
      hypotf(p1[0]-p2[0], p1[1]-p2[1]) - BZDB.eval(StateDatabase::BZDB_MUZZLEFRONT) - BZDBCache::tankRadius;

  const float missby = fabs(azimuthDiff) * (this->distanceToTarget - BZDBCache::tankLength);

  return ((missby < 0.5f * BZDBCache::tankLength) && (p1[2] < shotRadius));
}


/* Indicates whether or not there is a building between the tank and the shooting target.
 * @param dt The time since the last frame.
 * @return True if there is a building between the tank and the shooting target, false otherwise.
 */
bool RobotPlayer::buildingInTheWay(float dt) {
  const float azimuth = this->getAngle();

  float pos[3] = { getPosition()[0], getPosition()[1],
      getPosition()[2] + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT) };
  float dir[3] = { cosf(azimuth), sinf(azimuth), 0.0f };

  Ray tankRay(pos, dir);
  float maxdistance = this->distanceToTarget;

  return (ShotStrategy::getFirstBuilding(tankRay, -0.5f, maxdistance) != NULL);
}


/* Indicates whether or not there is a teammate between the tank and the shooting target.
 * @param dt The time since the last frame.
 * @return True if there is a teammate between the tank and the shooting target, false otherwise.
 */
bool RobotPlayer::teammateInTheWay(float dt) {
  const float shotRange = BZDB.eval(StateDatabase::BZDB_SHOTRANGE);

  const float azimuth = getAngle();
  float dir[3] = { cosf(azimuth), sinf(azimuth), 0.0f };

  for (int i = 0; i <= World::getWorld()->getCurMaxPlayers(); i++) {
    Player *p = 0;
    if (i < World::getWorld()->getCurMaxPlayers())
      p = World::getWorld()->getPlayer(i);
    else
      p = LocalPlayer::getMyTank();
    if (!p || p->getId() == getId() || validTeamTarget(p) || !p->isAlive())
      continue;
    float relpos[3] = { getPosition()[0] - p->getPosition()[0],
        getPosition()[1] - p->getPosition()[1], getPosition()[2] - p->getPosition()[2] };
    Ray ray(relpos, dir);
    float impact = rayAtDistanceFromOrigin(ray, 5 * BZDBCache::tankRadius);

    return (impact > 0 && impact < shotRange);
  }
  return false;
}


/* Postpone shooting for now.
 * @param dt The time since the last frame.
 */
void RobotPlayer::postponeShot(float dt) {
  //controlPanel->addMessage("postpone shot");
  this->timerForShot = 0.1f;
}


/* Shoot, and reset the timer for the next shot to a random amount.
 * @param dt The time since the last frame.
 */
void RobotPlayer::shoot(float dt) {
  if (this->fireShot()) {
    // separate shot by 0.2 - 0.8 sec (experimental value)
    this->timerForShot = float(bzfrand()) * 0.6f + 0.2f;
  }
}


// -------------------- doUpdate, drop flag --------------------

/* Indicates whether or not the tank is holding a flag.
 * @param dt The time since the last frame.
 * @return True if the tank is holding a flag, false otherwise.
 */
bool RobotPlayer::isHoldingFlag(float dt) {
  FlagType *myFlag = this->getFlag();
  return (myFlag != Flags::Null);
}


/* Indicates whether or not the flag belongs to some team
 * @param dt The time since the last frame.
 * @return True if the flag belongs to some team, false otherwise.
 */
bool RobotPlayer::isSomeTeamFlag(float dt) {
  FlagType *myFlag = this->getFlag();
  return (myFlag->flagTeam != NoTeam);
}


/* Indicates whether or not the flag can be dropped.
 * @param dt The time since the last frame.
 * @return True if the flag can be dropped, false otherwise.
 */
//bool RobotPlayer::flagIsSticky(float dt) {
//  FlagType *myFlag = this->getFlag();
//  return (myFlag->endurance == FlagSticky);
//}


/* Drops the flag at the tank's present location.
 * @param dt The time since the last frame.
 */
void RobotPlayer::dropFlag(float dt) {
  //controlPanel->addMessage("drop flag");
  serverLink->sendDropFlag(this->getId(), this->getPosition());
}




// -------------------- Currently Not Used --------------------

/* Sets the team's goal node. The A* search will try to find a path to this node.
 * If enemy flags exist and our team doesn't have an enemy flag,
 * then the goal will be the location of one of the enemy flags.
 * Otherwise, the goal will be the location of the team's base
 * WARNING: if the goal node is inaccessible then the game will crash!
 */
//void RobotPlayer::setTeamGoalNode() {
//  TeamColor myTeam = this->getTeam();
//  std::vector<Flag> enemyFlags = this->getAllEnemyFlags();
//  if (enemyFlags.size() && !this->teamHasEnemyFlag()) {
//    Flag someFlag = enemyFlags[0];
//    MyNode tempNode(someFlag.position[0], someFlag.position[1]);
//    teamGoal[myTeam] = tempNode;
//  }
//  else {
//    const float* basePos = World::getWorld()->getBase(myTeam, 0);
//    MyNode tempNode(basePos[0], basePos[1]);
//    teamGoal[myTeam] = tempNode;
//  }
//}


/* Gets the team's start node. The A* search will start searching for a path from this node.
 * The start node will be the location of the team's center of mass.
 * If this node is inaccessible, it will try to look for a nearby node that is accessible.
 * If no such node can be found, the start node will be
 * the location of the first tank to call this method.
 * @return The node from which to start the A* search.
 */
//MyNode RobotPlayer::getTeamStartNode() {
//  float centerOfMass[3];
//  this->getCenterOfMass(centerOfMass);
//  MyNode comNode(centerOfMass[0], centerOfMass[1]);
//  if (MyNode::getNearbyAccessibleNodeCoords(comNode.x, comNode.y, 4)) {
//    return comNode;
//  }
//  const float *firstTankPos = this->getPosition();
//  MyNode firstTankNode(firstTankPos[0], firstTankPos[1]);
//  return firstTankNode;
//}


/* Calculates the location of the center of mass for this tank's team.
 * @param centerOfMass Writes the location of the center of mass to this array.
 */
void RobotPlayer::getCenterOfMass(float centerOfMass[3]) {
  World *world = World::getWorld();
  const float *position = this->getPosition();

  float sumCoords[3] = {position[0], position[1], 0.0f};
  int teamSize       = 1;

  Player *p = 0;
  for (int i=0; i < world->getCurMaxPlayers(); i++) {
    // player 0 is reserved for the user, and getPlayer(0) will always be null
    if (0 == i)
      p = LocalPlayer::getMyTank();
    else
      p = world->getPlayer(i);
    // if player is exists, is not self, and is on same team
    if (p && p->getId() != this->getId() && p->getTeam() == this->getTeam()) {
      const float *pos = p->getPosition();
      sumCoords[0] += pos[0];
      sumCoords[1] += pos[1];
      teamSize++;
    }
  }  // for

  centerOfMass[0] = sumCoords[0]/teamSize;
  centerOfMass[1] = sumCoords[1]/teamSize;
  centerOfMass[2] = 0.0f;

//  float relativePosCOM[2] = {centerOfMass[0] - position[0], centerOfMass[1] - position[1]};
//  float distanceCOM = hypotf(relativePosCOM[0], relativePosCOM[1]);
}


/* Checks if this tank's team has an enemy flag.
 * @return True if this tanks's team has an enemy flag, false otherwise.
 */
bool RobotPlayer::teamHasEnemyFlag() {
  TeamColor myTeam = this->getTeam();
  World *world = World::getWorld();
  Player *tank = 0;
  // team flags exist (capture the flag mode)
  if (world->allowTeamFlags()) {
    for (int i=1; i<world->getCurMaxPlayers(); i++) {
      tank = world->getPlayer(i);
      // if the tank exists, is on our team, and has a flag
      if (tank && tank->getTeam() == myTeam && tank->getFlag() != Flags::Null) {
        TeamColor flagTeam = tank->getFlag()->flagTeam;
        if (flagTeam != myTeam && flagTeam != NoTeam)
          return true;
      }
    } // for
  }
  return false;
}




/*
// ========== Assignment 1    ==========
    World *world = World::getWorld();
    TeamColor myTeam = this->getTeam();

    float sumCoords[2]       = {0};
    int   numOtherPlayers    = 0;
    float sumCoordsNearby[2] = {0};
    int   numPlayersNearby   = 0;
    float sumVelocities[2]   = {0};

    // loop through all the tanks to save some data into the variables above
    Player *p = 0;
    for (int i=0; i < world->getCurMaxPlayers(); i++) {
      // player 0 is reserved for the user, and getPlayer(0) will always be null
      if (0 == i)
        p = LocalPlayer::getMyTank();
      else
        p = world->getPlayer(i);
      // if player is exists, is not self, and is on same team
      if (p && p->getId() != this->getId() && p->getTeam() == myTeam) {
        const float *pos = p->getPosition();

        // for Cohesion
        // sum up all the position coordinates, for calculating the center of mass
        sumCoords[0] += pos[0];
        sumCoords[1] += pos[1];
        numOtherPlayers++;

        // for Separation
        // sum up the position coordinates of tanks within the separation threshold radius
        float relativePos[2] = {pos[0] - position[0], pos[1] - position[1]};
        float distance = hypotf(relativePos[0], relativePos[1]);
        if (distance <= SEPARATION_THRESHOLD * BZDBCache::tankRadius) {
          sumCoordsNearby[0] += pos[0];
          sumCoordsNearby[1] += pos[1];
          numPlayersNearby++;
        }
        // for Alignment
        // sum up the velocity components of tanks within the alignment threshold radius
        // for calculating the average velocity
        else if (distance <= ALIGNMENT_THRESHOLD * BZDBCache::tankRadius) {
          sumVelocities[0] += p->getVelocity()[0];
          sumVelocities[1] += p->getVelocity()[1];
        }

      }
    } // for

    // Cohesion
    // calculate the center of mass
    // calculate the velocity vector toward the center of mass
    // scale the speed based on the distance to the center of mass
    float cohesionVel[2] = {0};
    if (numOtherPlayers) {
      float centerOfMass[2] = {sumCoords[0]/numOtherPlayers, sumCoords[1]/numOtherPlayers};
      float relativePosCOM[2] = {centerOfMass[0] - position[0], centerOfMass[1] - position[1]};
      float distanceCOM = hypotf(relativePosCOM[0], relativePosCOM[1]);
      float speed = (distanceCOM < dt * tankSpeed ? distanceCOM / dt : tankSpeed);
      cohesionVel[0] = (relativePosCOM[0] / distanceCOM) * speed;
      cohesionVel[1] = (relativePosCOM[1] / distanceCOM) * speed;
    }

    // Separation
    // calculate the center of mass of tanks within the threshold radius
    // calculate the velocity vector away from this local center of mass
    // move away at maximum speed, to help avoid collisions
    float separationVel[2] = {0};
    if (numPlayersNearby) {
      float nearbyCOM[2] = {sumCoordsNearby[0]/numPlayersNearby, sumCoordsNearby[1]/numPlayersNearby};
      // position - center of mass, because we want to move AWAY from this local center of mass
      float relativePosNearbyCOM[2] = {position[0] - nearbyCOM[0], position[1] - nearbyCOM[1]};
      float distanceNearbyCOM = hypotf(relativePosNearbyCOM[0], relativePosNearbyCOM[1]);
      separationVel[0] = (relativePosNearbyCOM[0] / distanceNearbyCOM) * tankSpeed;
      separationVel[1] = (relativePosNearbyCOM[1] / distanceNearbyCOM) * tankSpeed;
    }

    // Alignment
    // calculate the average velocity
    float alignmentVel[2] = {0};
    if (numOtherPlayers) {
      alignmentVel[0] = sumVelocities[0] / numOtherPlayers;
      alignmentVel[1] = sumVelocities[1] / numOtherPlayers;
    }

// ========== Assignment 2 ==========

    // velocity vector for capture the flag (CTF)
    float ctfVel[2] = {0};

    // if team flags exist (capture the flag mode)
    if (world->allowTeamFlags()) {
      std::vector<Flag> enemyFlags = this->identifyEnemyFlags();
      // if enemy flags exist
      if (enemyFlags.size()) {
        // the position target for CTF
        // either the position of an enemy flag, or the position of our base
        float ctfPositionTarget[2] = {0};

        // if our team doesn't have an enemy flag, the CTF position target is the position of some enemy flag
        if (!this->teamHasEnemyFlag()) {
          // choose closest enemy flag
          Flag targetFlag = enemyFlags[0];
          float targetDistance = hypotf(targetFlag.position[0] - position[0], targetFlag.position[1] - position[1]);
          for (std::vector<Flag>::iterator i = enemyFlags.begin(); i<enemyFlags.end(); i++) {
            Flag temp = *i;
            float tempDistance = hypotf(temp.position[0] - position[0], temp.position[1] - position[1]);
            if (tempDistance < targetDistance) {
              targetFlag = temp;
              targetDistance = tempDistance;
            }
          } // for
          ctfPositionTarget[0] = targetFlag.position[0];
          ctfPositionTarget[1] = targetFlag.position[1];
        }
        // we have an enemy flag, so the CTF position target is the position of our base
        else {
          const float* basePos = world->getBase(myTeam, 0);
          ctfPositionTarget[0] = basePos[0];
          ctfPositionTarget[1] = basePos[1];
        }

        // calculate velocity vector to the CTF position target
        float ctfRelativePos[2] = {ctfPositionTarget[0] - position[0], ctfPositionTarget[1] - position[1]};
        float ctfDistance = hypotf(ctfRelativePos[0], ctfRelativePos[1]);
        ctfVel[0] = (ctfRelativePos[0] / ctfDistance) * tankSpeed;
        ctfVel[1] = (ctfRelativePos[1] / ctfDistance) * tankSpeed;
      } // if enemyFlags.size()
    } // if world->allowTeamFlags
    // NOTE: if team flags are allowed, but enemy flags do not exist, then
    // currently this implementation will not drop flags (but I think that doesn't really matter at this point)


    // weighted blending of cohesion, separation, alignment, capture the flag
    float v[2] = {0};
    v[0] = COHESION_WEIGHT*cohesionVel[0] + SEPARATION_WEIGHT*separationVel[0] + ALIGNMENT_WEIGHT*alignmentVel[0] +
           CTF_WEIGHT*ctfVel[0];
    v[1] = COHESION_WEIGHT*cohesionVel[1] + SEPARATION_WEIGHT*separationVel[1] + ALIGNMENT_WEIGHT*alignmentVel[1] +
           CTF_WEIGHT*ctfVel[1];
    float distance = hypotf(v[0], v[1]);
*/




// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

