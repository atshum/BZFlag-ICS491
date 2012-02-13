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

/**
 * @file
 *
 * Holds classes for making decisions based on a decision
 * tree. Decision trees consist of a series of decisions, arranged so
 * that the results of one decision lead to another, until finally a
 * decision is reached at the end of the tree.
 */
#ifndef AICORE_DECTREE_H
#define AICORE_DECTREE_H

#include "RobotPlayer.h"

#define NULL 0

namespace aicore
{

    /**
     * A decision tree node is a base class for anything that makes a
     * decision.
     */
    class DecisionTreeNode
    {
    public:
        /**
         * The make decision method carries out a decision making
         * process and returns the new decision tree node that we've
         * reached in the tree.
         */
        virtual DecisionTreeNode* makeDecision(RobotPlayer* bot, float dt) = 0;
    };

    /**
     * An action is a decision tree node at the end of the tree. It
     * simply returns itself as the result of the next decision.
     */
    class DecisionTreeAction : public DecisionTreeNode
    {
    public:
        /**
         * Makes the decision - in  this case there is no decision, so
         * this method returns the action back again..
         */
        virtual DecisionTreeNode* makeDecision(RobotPlayer* bot, float dt)
        {
            return this;
        }
    };

    /**
     * Other than actions, the tree is made up of decisions, which
     * come up with some boolean result and choose a branch based on
     * that.
     */
    class Decision : public DecisionTreeNode
    {
    public:
        DecisionTreeNode* trueBranch;
        DecisionTreeNode* falseBranch;

        /**
         * This method actually does the checking for the decision.
         */
        virtual bool getBranch(RobotPlayer* bot, float dt) = 0;

        /**
         * This is where the decision tree algorithm is located: it
         * recursively walks down the tree until it reaches the final
         * item to return (which is an action).
         */
        virtual DecisionTreeNode* makeDecision(RobotPlayer* bot, float dt);
    };

    /**
     * This class represents a decision given by the function pointer,
	 * ptr
     */
	class DecisionPtr : public aicore::Decision
	{
	public:
		/**
		* This will hold the function pointer
		*/
		bool (RobotPlayer::*decFuncPtr)(float dt);

        virtual DecisionTreeNode* makeDecision(RobotPlayer* bot, float dt);
		virtual bool getBranch(RobotPlayer* bot, float dt);
	};

	/**
	* This is a very simple action that just holds a function pointer
	* to the actual action to run.
	*/
	class ActionPtr : public aicore::DecisionTreeAction
	{
	public:
		/**
		* This will hold the function pointer
		*/
		void (RobotPlayer::*actFuncPtr)(float dt);
	};

	class DecisionTrees
	{
	public:
		static void init();
		// Holds our list of decisions
		static DecisionPtr doUpdateMotionDecisions[7];
		// Holds our list of actions
		static ActionPtr doUpdateMotionActions[5];
	};

}; // end of namespace

#endif // AICORE_DECTREE_H
