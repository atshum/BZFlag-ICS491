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

#ifndef	BZF_ROBOT_PLAYER_H
#define	BZF_ROBOT_PLAYER_H

// for A* search
#include "yagsbpl/GraphFunctionContainerUnified.h"
#include "yagsbpl/A_star.h"


#include "common.h"

/* system interface headers */
#include <vector>

/* interface header */
#include "LocalPlayer.h"

/* local interface headers */
#include "Region.h"
#include "RegionPriorityQueue.h"
#include "ServerLink.h"

class RobotPlayer: public LocalPlayer {
public:
  RobotPlayer(const PlayerId&, const char* name, ServerLink*, const char* _motto);

  float getTargetPriority(const Player*) const;
  const Player* getTarget() const;
  void setTarget(const Player*);
  static void setObstacleList(std::vector<BzfRegion*>*);

  void restart(const float* pos, float azimuth);
  void explodeTank();

// ========== MY CODE (begin) ==========
  bool tankIsAlive(float dt);
  bool isGuardingFlag(float dt);
  bool hasMyTeamFlag(float dt);
  void doNothing(float dt);

// ---------- setTarget, assign roles to tanks ----------
  bool hasLowestId(float dt);
  bool hasHighestId(float dt);
  void setRoleGuardFlag(float dt);
  void setRoleCaptureFlags(float dt);
  void setRoleKillEnemies(float dt);

// ---------- doUpdateMotion ----------
  bool shotComing(float dt);
  bool isAtTeamBase(float dt);
  void evade(float dt);
  void aimAtClosestEnemy(float dt);
  void followPath(float dt);

// ---------- doUpdate, shoot ----------
  bool readyToFire(float dt);
  bool shotTimerElapsed(float dt);
  bool willBarelyMiss(float dt);
  bool buildingInTheWay(float dt);
  bool teammateInTheWay(float dt);
  void postponeShot(float dt);
  void shoot(float dt);


// ---------- doUpdate, drop flag ----------
  bool isHoldingFlag(float dt);
  bool isSomeTeamFlag(float dt);
  void dropFlag(float dt);
  //bool flagIsSticky(float dt);


private:
  // the angle that the incoming shot is traveling in
  float incomingShotAngle;

  // the distance from the tank to the shooting target
  float distanceToTarget;

// ---------- role stuff ----------
  static PlayerId teamLowestId[CtfTeams];
  static PlayerId teamHighestId[CtfTeams];

  static MyNode guardGoal[CtfTeams];
  static std::vector<MyNode> guardPath[CtfTeams];

  static MyNode captureGoal[CtfTeams];
  static std::vector<MyNode> capturePath[CtfTeams];

//  static MyNode killGoal[CtfTeams];
//  static std::vector<MyNode> killPath[CtfTeams];

  MyNode myGoal;  // the tank's individual goal, to be compared with the role goal
  // TODO: do I need to initialize this in the constructor?

  MyNode prevGoal;

  void assignRole(MyNode roleGoal, std::vector<MyNode> rolePath);

// ---------- role helpers ----------
  std::vector<Flag> findAllEnemyFlags();

// ---------- path-finding helpers ----------
  void findPath(std::vector<MyNode> &myPath, MyNode start, MyNode goal);
  std::vector<MyNode> smoothPath(std::vector<MyNode> inputPath);
  bool obstructedLineOfSight(const float *fromPos, const float *toPos);

// ---------- doUpdateMotion helpers ----------
  bool findClosestEnemy(float enemyPos[3]);
  void checkLineOfSight();
  void getSeparation(float v[3]);


// ---------- currently unused ----------
  // static MyNode teamGoal[CtfTeams];  // stores the goal that each team wants to reach
  // static std::vector<MyNode> teamPaths[CtfTeams];  // stores each team's A* path to the team goal
  // void setTeamGoalNode();
  // MyNode getTeamStartNode();
  void getCenterOfMass(float centerOfMass[3]);
  bool teamHasEnemyFlag();



// ========== MY CODE (end) ==========


private:
  void doUpdate(float dt);
  void doUpdateMotion(float dt);
  BzfRegion* findRegion(const float p[2], float nearest[2]) const;
  float getRegionExitPoint(const float p1[2], const float p2[2], const float a[2],
      const float targetPoint[2], float mid[2], float& priority);
  void findPath(RegionPriorityQueue& queue, BzfRegion* region, BzfRegion* targetRegion,
      const float targetPoint[2], int mailbox);
  void projectPosition(const Player *targ, const float t, float &x, float &y, float &z) const;
  void getProjectedPosition(const Player *targ, float *projpos) const;

private:
  const Player* target;
  std::vector<RegionPoint> path;
  int pathIndex;
  float timerForShot;
  bool drivingForward;
  static std::vector<BzfRegion*>* obstacleList;

};

#endif // BZF_ROBOT_PLAYER_H
// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

