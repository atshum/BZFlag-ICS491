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
	  // doUpdateMotion
    doUpdateMotionDecisions[0].decFuncPtr  = &RobotPlayer::tankIsAlive;
    doUpdateMotionDecisions[0].trueBranch  = &doUpdateMotionDecisions[1];
    doUpdateMotionDecisions[0].falseBranch = &doUpdateMotionActions[0];  // do nothing

    doUpdateMotionDecisions[1].decFuncPtr  = &RobotPlayer::shotComing;
    doUpdateMotionDecisions[1].trueBranch  = &doUpdateMotionActions[1];  // evade
    doUpdateMotionDecisions[1].falseBranch = &doUpdateMotionActions[2];  // follow path

    doUpdateMotionActions[0].actFuncPtr = &RobotPlayer::doNothing;
    doUpdateMotionActions[1].actFuncPtr = &RobotPlayer::evade;
    doUpdateMotionActions[2].actFuncPtr = &RobotPlayer::followPath;

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

    dropFlagDecisions[2].decFuncPtr  = &RobotPlayer::flagIsSticky;
    dropFlagDecisions[2].trueBranch  = &dropFlagActions[0];    // do nothing
    dropFlagDecisions[2].falseBranch = &dropFlagDecisions[3];

    dropFlagDecisions[3].decFuncPtr  = &RobotPlayer::flagTeamExists;
    dropFlagDecisions[3].trueBranch  = &dropFlagDecisions[4];
    dropFlagDecisions[3].falseBranch = &dropFlagActions[0];    // do nothing

    dropFlagDecisions[4].decFuncPtr  = &RobotPlayer::isMyTeamFlag;
    dropFlagDecisions[4].trueBranch  = &dropFlagActions[1];    // drop flag
    dropFlagDecisions[4].falseBranch = &dropFlagActions[0];    // do nothing

    dropFlagActions[0].actFuncPtr = &RobotPlayer::doNothing;
    dropFlagActions[1].actFuncPtr = &RobotPlayer::dropFlag;

	}

	DecisionPtr DecisionTrees::doUpdateMotionDecisions[2];
	ActionPtr   DecisionTrees::doUpdateMotionActions[3];

	DecisionPtr DecisionTrees::shootDecisions[6];
  ActionPtr   DecisionTrees::shootActions[3];

  DecisionPtr DecisionTrees::dropFlagDecisions[5];
  ActionPtr   DecisionTrees::dropFlagActions[2];

}; // end of namespace
