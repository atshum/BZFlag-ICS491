/*
 * Defines the classes used for decision trees.
 *
 * Part of the Artificial Intelligence for Games system.
 *
 * Copyright (c) Ian Millington 2003-2006. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */
#include "dectree.h"

namespace aicore {

  DecisionTreeNode* Decision::makeDecision(RobotPlayer* bot, float dt) {
    // Choose a branch based on the getBranch method
    if (getBranch(bot, dt)) {
      // Make sure its not null before recursing.
      if (trueBranch == NULL) return NULL;
      else return trueBranch->makeDecision(bot, dt);
    }
    else {
      // Make sure its not null before recursing.
      if (falseBranch == NULL) return NULL;
      else return falseBranch->makeDecision(bot, dt);
    }
  }
    
	DecisionTreeNode* DecisionPtr::makeDecision(RobotPlayer* bot, float dt) {
    // Choose a branch based on the getBranch method
    if ( getBranch(bot, dt) ) {
      // Make sure its not null before recursing.
      if (trueBranch == NULL) return NULL;
      else return trueBranch->makeDecision(bot, dt);
    } else {
      // Make sure its not null before recursing.
      if (falseBranch == NULL) return NULL;
      else return falseBranch->makeDecision(bot, dt);
    }
  }

	bool DecisionPtr::getBranch(RobotPlayer* bot, float dt)
	{
		return (bot->*decFuncPtr)(dt);
	}

	// Set up the trees
	void DecisionTrees::init() {

	  // setTarget, assign roles to tanks
	  assignRoleDecisions[0].decFuncPtr = &RobotPlayer::hasLowestId;
	  assignRoleDecisions[0].trueBranch  = &assignRoleActions[0];
	  assignRoleDecisions[0].falseBranch = &assignRoleDecisions[1];

    assignRoleDecisions[1].decFuncPtr = &RobotPlayer::hasHighestId;
    assignRoleDecisions[1].trueBranch  = &assignRoleActions[1];  // capture flags
    assignRoleDecisions[1].falseBranch = &assignRoleActions[2];  // kill enemies

	  assignRoleActions[0].actFuncPtr = &RobotPlayer::setRoleGuardFlag;
	  assignRoleActions[1].actFuncPtr = &RobotPlayer::setRoleCaptureFlags;
	  assignRoleActions[2].actFuncPtr = &RobotPlayer::setRoleKillEnemies;


	  // doUpdateMotion
    doUpdateMotionDecisions[0].decFuncPtr  = &RobotPlayer::tankIsAlive;
    doUpdateMotionDecisions[0].trueBranch  = &doUpdateMotionDecisions[1];
    doUpdateMotionDecisions[0].falseBranch = &doUpdateMotionActions[0];    // do nothing

    doUpdateMotionDecisions[1].decFuncPtr  = &RobotPlayer::shotComing;
    doUpdateMotionDecisions[1].trueBranch  = &doUpdateMotionActions[1];    // evade
    doUpdateMotionDecisions[1].falseBranch = &doUpdateMotionDecisions[2];

    doUpdateMotionDecisions[2].decFuncPtr  = &RobotPlayer::isGuardingFlag;
    doUpdateMotionDecisions[2].trueBranch  = &doUpdateMotionDecisions[3];
    doUpdateMotionDecisions[2].falseBranch = &doUpdateMotionActions[2];  // not guard, follow path

    doUpdateMotionDecisions[3].decFuncPtr  = &RobotPlayer::hasMyTeamFlag;
    doUpdateMotionDecisions[3].trueBranch  = &doUpdateMotionDecisions[4];
    doUpdateMotionDecisions[3].falseBranch = &doUpdateMotionActions[2];  // follow path to flag

    doUpdateMotionDecisions[4].decFuncPtr  = &RobotPlayer::isAtTeamBase;
    doUpdateMotionDecisions[4].trueBranch  = &doUpdateMotionActions[3];  // aim at closest enemy
    doUpdateMotionDecisions[4].falseBranch = &doUpdateMotionActions[2];  // follow path to base

    doUpdateMotionActions[0].actFuncPtr = &RobotPlayer::doNothing;
    doUpdateMotionActions[1].actFuncPtr = &RobotPlayer::evade;
    doUpdateMotionActions[2].actFuncPtr = &RobotPlayer::followPath;
    doUpdateMotionActions[3].actFuncPtr = &RobotPlayer::aimAtClosestEnemy;


    // doUpdate, shoot
    shootDecisions[0].decFuncPtr  = &RobotPlayer::tankIsAlive;
    shootDecisions[0].trueBranch  = &shootDecisions[1];
    shootDecisions[0].falseBranch = &shootActions[0];    // do nothing

    shootDecisions[1].decFuncPtr  = &RobotPlayer::readyToFire;
    shootDecisions[1].trueBranch  = &shootDecisions[2];
    shootDecisions[1].falseBranch = &shootActions[0];    // do nothing

    shootDecisions[2].decFuncPtr  = &RobotPlayer::shotTimerElapsed;
    shootDecisions[2].trueBranch  = &shootDecisions[3];
    shootDecisions[2].falseBranch = &shootActions[0];    // do nothing

    shootDecisions[3].decFuncPtr  = &RobotPlayer::willBarelyMiss;
    shootDecisions[3].trueBranch  = &shootDecisions[4];
    shootDecisions[3].falseBranch = &shootActions[0];    // do nothing

    shootDecisions[4].decFuncPtr  = &RobotPlayer::buildingInTheWay;
    shootDecisions[4].trueBranch  = &shootActions[0];    // do nothing
    shootDecisions[4].falseBranch = &shootDecisions[5];

    shootDecisions[5].decFuncPtr  = &RobotPlayer::teammateInTheWay;
    shootDecisions[5].trueBranch  = &shootActions[1];    // delay shot
    shootDecisions[5].falseBranch = &shootActions[2];    // shoot

    shootActions[0].actFuncPtr = &RobotPlayer::doNothing;
    shootActions[1].actFuncPtr = &RobotPlayer::postponeShot;
    shootActions[2].actFuncPtr = &RobotPlayer::shoot;


    // doUpdate, drop flag
    dropFlagDecisions[0].decFuncPtr  = &RobotPlayer::tankIsAlive;
    dropFlagDecisions[0].trueBranch  = &dropFlagDecisions[1];
    dropFlagDecisions[0].falseBranch = &dropFlagActions[0];    // do nothing

    dropFlagDecisions[1].decFuncPtr  = &RobotPlayer::isHoldingFlag;
    dropFlagDecisions[1].trueBranch  = &dropFlagDecisions[2];
    dropFlagDecisions[1].falseBranch = &dropFlagActions[0];    // do nothing

    dropFlagDecisions[2].decFuncPtr  = &RobotPlayer::hasMyTeamFlag;
    dropFlagDecisions[2].trueBranch  = &dropFlagDecisions[3];
    dropFlagDecisions[2].falseBranch = &dropFlagDecisions[4];

    dropFlagDecisions[3].decFuncPtr  = &RobotPlayer::isGuardingFlag;
    dropFlagDecisions[3].trueBranch  = &dropFlagActions[0];    // guarding, don't drop, do nothing
    dropFlagDecisions[3].falseBranch = &dropFlagActions[1];    // drop flag, let guard get it

    dropFlagDecisions[4].decFuncPtr  = &RobotPlayer::isSomeTeamFlag;
    dropFlagDecisions[4].trueBranch  = &dropFlagActions[0];    // enemy flag, do nothing
    dropFlagDecisions[4].falseBranch = &dropFlagActions[0];    // is a good/bad flag, do nothing

/*
    dropFlagDecisions[4].decFuncPtr  = &RobotPlayer::flagIsSticky;
    dropFlagDecisions[4].trueBranch  = &dropFlagActions[0];    // do nothing
    dropFlagDecisions[4].falseBranch = &dropFlagDecisions[3];

*/

    dropFlagActions[0].actFuncPtr = &RobotPlayer::doNothing;
    dropFlagActions[1].actFuncPtr = &RobotPlayer::dropFlag;

	}

  DecisionPtr DecisionTrees::assignRoleDecisions[2];
  ActionPtr   DecisionTrees::assignRoleActions[3];

	DecisionPtr DecisionTrees::doUpdateMotionDecisions[5];
	ActionPtr   DecisionTrees::doUpdateMotionActions[4];

	DecisionPtr DecisionTrees::shootDecisions[6];
  ActionPtr   DecisionTrees::shootActions[3];

  DecisionPtr DecisionTrees::dropFlagDecisions[5];
  ActionPtr   DecisionTrees::dropFlagActions[2];

}; // end of namespace
