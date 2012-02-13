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

/*
 *
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

  /* Indicates whether or not the tank is alive.
   * @param dt The time since the last frame.
   * @return True if the tank is alive, false otherwise.
   */
  bool tankIsAlive(float dt);

  /* Does nothing.  Needed because the ActionPtrs need a function to point to.
   * @param dt The time since the last frame.
   */
  void doNothing(float dt);


// ---------- doUpdateMotion ----------
  /* Indicates whether or not a shot is coming near the tank.
   * @param dt The time since the last frame.
   * @return True if a shot is coming near the tank, false otherwise.
   */
  bool shotComing(float dt);

  /* Turn and move away from the incoming shot.
   * @param dt The time since the last frame.
   */
  void evade(float dt);

  /* Follow the tank's path toward the team goal.
   * @param dt The time since the last frame.
   */
  void followPath(float dt);


// ---------- doUpdate, shoot ----------
  /* Indicates whether or not the tank's firing status is ready.
   * @param dt The time since the last frame.
   * @return True if the tank's firing status is ready, false otherwise.
   */
  bool readyToFire(float dt);

  /* Indicates whether or not the tank's timer for shooting has elapsed.
   * @param dt The time since the last frame.
   * @return True if the tank's timer for shooting has elapsed, false otherwise.
   */
  bool shotTimerElapsed(float dt);

  /* Indicates whether or not the predicted shot will miss by less than half a tank length.
   * @param dt The time since the last frame.
   * @return True if the predicted shot will miss by less than half a tank length, false otherwise.
   */
  bool willBarelyMiss(float dt);

  /* Indicates whether or not there is a building between the tank and the shooting target.
   * @param dt The time since the last frame.
   * @return True if there is a building between the tank and the shooting target, false otherwise.
   */
  bool buildingInTheWay(float dt);

  /* Indicates whether or not there is a teammate between the tank and the shooting target.
   * @param dt The time since the last frame.
   * @return True if there is a teammate between the tank and the shooting target, false otherwise.
   */
  bool teammateInTheWay(float dt);

  /* Postpone shooting for now.
   * @param dt The time since the last frame.
   */
  void postponeShot(float dt);

  /* Shoot, and reset the timer for the next shot to a random amount.
   * @param dt The time since the last frame.
   */
  void shoot(float dt);


// ---------- doUpdate, drop flag ----------
  /* Indicates whether or not the tank is holding a flag.
   * @param dt The time since the last frame.
   * @return True if the tank is holding a flag, false otherwise.
   */
  bool isHoldingFlag(float dt);

  /* Indicates whether or not the flag can be dropped.
   * @param dt The time since the last frame.
   * @return True if the flag can be dropped, false otherwise.
   */
  bool flagIsSticky(float dt);

  /* Indicates whether or not the flag belongs to some team
   * @param dt The time since the last frame.
   * @return True if the flag belongs to some team, false otherwise.
   */
  bool flagTeamExists(float dt);

  /* Indicates whether or not the flag is the tank's team flag.
   * @param dt The time since the last frame.
   * @return True if the flag is the tank's team flag, false otherwise.
   */
  bool isMyTeamFlag(float dt);

  /* Drops the flag at the tank's present location.
   * @param dt The time since the last frame.
   */
  void dropFlag(float dt);


private:
  // the angle that the incoming shot is traveling in
  float incomingShotAngle;

  // the distance from the tank to the shooting target
  float distanceToTarget;


// ---------- path stuff ----------
  // stores the goal that each team wants to reach
  static MyNode teamGoal[CtfTeams];

  // stores each team's A* path to the team goal
  static std::vector<MyNode> teamPaths[CtfTeams];

  // the tank's goal, to be compared with the team goal
  MyNode myGoal;

  void setTeamGoalNode();
  MyNode getTeamStartNode();
  void setPath(std::vector<MyNode> &myPath, MyNode start, MyNode goal);
  std::vector<MyNode> getSmoothPath(std::vector<MyNode> inputPath);

  void checkLineOfSight();
  bool obstructedLineOfSight(const float *fromPos, const float *toPos);

  void getCenterOfMass(float centerOfMass[2]);

  bool teamHasEnemyFlag();
  std::vector<Flag> getAllEnemyFlags();

  void getSeparation(float v[3]);

  // used to do nothing for the first few run loops of the game
  static int loops;

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

