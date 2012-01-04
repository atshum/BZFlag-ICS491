/******************************************************************************************
*                                                                                        *
*    Yet Another Graph-Search Based Planning Library (YAGSBPL)                           *
*    A template-based C++ library for graph search and planning                          *
*    Version 2.0                                                                         *
*    ----------------------------------------------------------                          *
*    Copyright (C) 2011  Subhrajit Bhattacharya                                          *
*                                                                                        *
*    This program is free software: you can redistribute it and/or modify                *
*    it under the terms of the GNU General Public License as published by                *
*    the Free Software Foundation, either version 3 of the License, or                   *
*    (at your option) any later version.                                                 *
*                                                                                        *
*    This program is distributed in the hope that it will be useful,                     *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of                      *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                       *
*    GNU General Public License for more details <http://www.gnu.org/licenses/>.         *
*                                                                                        *
*                                                                                        *
*    Contact: subhrajit@gmail.com, http://subhrajit.net/                                 *
*                                                                                        *
*                                                                                        *
******************************************************************************************/
//    For a detailed tutorial and download, visit 
//    http://subhrajit.net/index.php?WPage=yagsbpl


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HeapContainer<NodeType,CostType,PlannerSpecificVariables>::push 
									( SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* np, 
										SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* searchStart )
{
	if (!start && !end) // empty list
	{
		key_init(np);
		start = np;
		end = np;
		heap_size = 1;
	}
	else
	{
		heapItem_p pointer;
		if (searchStart)
			pointer = searchStart;
		else
			pointer = key_find(np->f);
		
		while (true)
		{
			// found position at the beginning
			if ( pointer->f <= np->f && !pointer->prev )
			{
				start->prev = np;
				np->nxt = start;
				start = np;
				break;
			}
			// found position at the middle
			if ( pointer->f <= np->f && pointer->prev->f >= np->f )
			{
				np->prev = pointer->prev;
				np->nxt = pointer;
				pointer->prev->nxt = np;
				pointer->prev = np;
				break;
			}
			// found position at the end
			if ( pointer->f >= np->f && !pointer->nxt )
			{
				end->nxt = np;
				np->prev = end;
				end = np;
				break;
			}
			
			if ( pointer->f <= np->f )
				pointer = pointer->prev;
			else if ( pointer->f > np->f )
				pointer = pointer->nxt;
		}
		
		np->inHeap = true;
		heap_size++;
		key_update(np);
	}
}


template <class NodeType, class CostType, class PlannerSpecificVariables>
SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* HeapContainer<NodeType,CostType,PlannerSpecificVariables>::pop(void)
{
	heapItem_p ret;
	ret = end;
	key_remove(ret);
	
	if (heap_size > 1)
	{
		end = end->prev;
		end->nxt = NULL;
		ret->prev = NULL;
		ret->inHeap = false;
		heap_size--;
	}
	else if (heap_size==1)
	{
		start = NULL;
		end = NULL;
		ret->inHeap = false;
		heap_size--;
	}
	
	return ret;
}


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HeapContainer<NodeType,CostType,PlannerSpecificVariables>::remove (SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* np)
{
	key_remove(np);
	
	if (np->prev) // Not at the beginning
		np->prev->nxt = np->nxt;
	else
		start = np->nxt;
		
	if (np->nxt)
		np->nxt->prev = np->prev;
	else
		end = np->prev;
		
	np->nxt = NULL;
	np->prev = NULL;
	np->inHeap = false;
	heap_size--;
}

// -------------


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HeapContainer<NodeType,CostType,PlannerSpecificVariables>::key_init(heapItem_p item)
{
	for (int a=0; a<keyCount; a++)
	{
		keyPoints[a] = item;
		keyVals[a] = item->f;
	}
}


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HeapContainer<NodeType,CostType,PlannerSpecificVariables>::key_update(heapItem_p item)
		// To be called after instering item into heap
{
	if ( heap_size<=1 || start->f==end->f )
	{
		key_init(item);
		return;
	}
	CostType range = start->f - end->f;
	int itemPos = (int)( (( item->f - end->f ) * keyCount) / range ); // Can assume values from 0 to keyCount
	int itemPosP1 = itemPos + 1;
	CostType PosIdealVal = end->f + (itemPosP1*range)/keyCountP1;
	
	
	if ( itemPos<keyCount   &&  _yagsbpl_abs( item->f - PosIdealVal ) < _yagsbpl_abs( keyVals[itemPos] - PosIdealVal ) )
	{
		keyPoints[itemPos] = item;
		keyVals[itemPos] = PosIdealVal;
	}
	else if ( itemPos>0 )
	{
		itemPos--;
		itemPosP1--;
		PosIdealVal = end->f + (itemPosP1*range)/keyCountP1;
		if ( _yagsbpl_abs( item->f - PosIdealVal ) < _yagsbpl_abs( keyVals[itemPos] - PosIdealVal ) )
		{
			keyPoints[itemPos] = item;
			keyVals[itemPos] = PosIdealVal;
		}
	}
}


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HeapContainer<NodeType,CostType,PlannerSpecificVariables>::key_remove(heapItem_p item)
			// should be called before the nxt and prev are set to NULL.
{
	int itemTentativePos;
	if (start->f == end->f)
		itemTentativePos = 0;
	else
		itemTentativePos = (int)( (( item->f - end->f ) * keyCount) / (start->f - end->f) ); // value: 0 to keyCount
	int l=itemTentativePos-1, r=itemTentativePos;
	
	while (r < keyCount)
	{
		if (keyPoints[r] == item)
		{
			if (item->nxt)
			{
				keyPoints[r] = item->nxt;
				keyVals[r] = item->nxt->f;
				return;
			}
			if (item->prev)
			{
				keyPoints[r] = item->prev;
				keyVals[r] = item->prev->f;
				return;
			}
		}
		r++;
	}
	while (l >= 0)
	{
		if (keyPoints[l] == item)
		{
			if (item->prev)
			{
				keyPoints[l] = item->prev;
				keyVals[l] = item->prev->f;
				return;
			}
			if (item->nxt)
			{
				keyPoints[l] = item->nxt;
				keyVals[l] = item->nxt->f;
				return;
			}
		}
		l--;
	}
	
}


template <class NodeType, class CostType, class PlannerSpecificVariables>
SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* 
						HeapContainer<NodeType,CostType,PlannerSpecificVariables>::key_find (CostType f)
{
	CostType fRes = (start->f - end->f) / keyCountP1;
	int pos = keyCount / 2; // level=2 to start with.
	int level = 4;
	while ( level <= keyCount  &&  pos >= 0  &&  pos < keyCount)
	{
		if ( keyVals[pos] < f )
		{
			if ( f - keyVals[pos] <= fRes )
				break;
			pos += keyCount / level;
		}
		else
		{
			if ( keyVals[pos] - f <= fRes )
				break;
			pos -= keyCount / level;
		}
		level *= 2;
	}
	
	if (pos < 0)
		return (end);
	if (pos >= keyCount)
		return (start);
	if ( !keyPoints[pos]->inHeap )
	{
		if ( f - end->f  <=  start->f - f )
			return(end);
		else
			return(start);
	}
	return (keyPoints[pos]);
}

// =================================================================================


template <class NodeType, class CostType>
GenericSearchGraphDescriptor<NodeType,CostType>::GenericSearchGraphDescriptor()
{
	hashBinSizeIncreaseStep = 128;
	// Initiating pointers to all functions as NULL - makes easy to check if a function was defined
	getHashBin_fp = NULL;
	isAccessible_fp = NULL;
	getSuccessors_fp = NULL;
	getPredecessors_fp = NULL;
	getHeuristics_fp = NULL;
	storePath_fp = NULL;
	stopSearch_fp = NULL;
	func_container = NULL;
}

template <class NodeType, class CostType>
void GenericSearchGraphDescriptor<NodeType,CostType>::init(void)
{
	if (SeedNodes.size() == 0)
		SeedNodes.push_back(SeedNode);
	// Other initializations - to be included in future versions
}

// ---------------------------------

template <class NodeType, class CostType>
int GenericSearchGraphDescriptor<NodeType,CostType>::_getHashBin(NodeType& n)
{
	if (getHashBin_fp)
		return ( getHashBin_fp(n) );
	
	if (func_container)
	{
		func_container->func_redefined = true;
		int ret = func_container->getHashBin(n);
		if (func_container->func_redefined)
			return ret;
	}
	
	return 0;
}

template <class NodeType, class CostType>
bool GenericSearchGraphDescriptor<NodeType,CostType>::_isAccessible(NodeType& n)
{
	if (isAccessible_fp)
		return ( isAccessible_fp(n) );
	
	if (func_container)
	{
		func_container->func_redefined = true;
		bool ret = func_container->isAccessible(n);
		if (func_container->func_redefined)
			return ret;
	}
	
	return true;
}

template <class NodeType, class CostType>
void GenericSearchGraphDescriptor<NodeType,CostType>::_getSuccessors(NodeType& n, std::vector<NodeType>* s, std::vector<CostType>* c)
{
	if (getSuccessors_fp)
		getSuccessors_fp(n, s, c);
	else if (func_container)
		func_container->getSuccessors(n, s, c);
}

template <class NodeType, class CostType>
void GenericSearchGraphDescriptor<NodeType,CostType>::_getPredecessors(NodeType& n, std::vector<NodeType>* s, std::vector<CostType>* c)
{
	if (getPredecessors_fp)
		getPredecessors_fp(n, s, c);
	else if (func_container)
		func_container->getPredecessors(n, s, c);
}

template <class NodeType, class CostType>
CostType GenericSearchGraphDescriptor<NodeType,CostType>::_getHeuristics(NodeType& n1, NodeType& n2)
{
	if (getHeuristics_fp)
		return ( getHeuristics_fp(n1, n2) );
	
	if (func_container)
	{
		func_container->func_redefined = true;
		CostType ret = func_container->getHeuristics(n1, n2);
		if (func_container->func_redefined)
			return ret;
	}
	
	return ((CostType)0);
}

template <class NodeType, class CostType>
CostType GenericSearchGraphDescriptor<NodeType,CostType>::_getHeuristicsToTarget(NodeType& n)
{
	return ( _getHeuristics(n, TargetNode) );
}

template <class NodeType, class CostType>
bool GenericSearchGraphDescriptor<NodeType,CostType>::_storePath(NodeType& n)
{
	if (storePath_fp)
		return ( storePath_fp(n) );
	
	if (func_container)
	{
		func_container->func_redefined = true;
		bool ret = func_container->storePath(n);
		if (func_container->func_redefined)
			return ret;
	}
	
	return false;
}

template <class NodeType, class CostType>
bool GenericSearchGraphDescriptor<NodeType,CostType>::_stopSearch(NodeType& n)
{
	if (stopSearch_fp)
		return ( stopSearch_fp(n) );
	
	if (func_container)
	{
		func_container->func_redefined = true;
		bool ret = func_container->stopSearch(n);
		if (func_container->func_redefined)
			return ret;
	}
	
	if ( n == TargetNode )
		return true;
	
	return false;
}

// =================================================================================


template <class NodeType, class CostType, class PlannerSpecificVariables>
void HashTableContainer<NodeType,CostType,PlannerSpecificVariables>::init_HastTable(int hash_table_size)
{
	hashTableSize = hash_table_size;
	HashTable = new std::vector< SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* > [ hashTableSize ];
	for (int a=0; a<hashTableSize; a++)
		if ( HashTable[a].capacity() >= HashTable[a].size()-1 )
			HashTable[a].reserve( HashTable[a].capacity() + friendGraphDescriptor_p->hashBinSizeIncreaseStep );
}

template <class NodeType, class CostType, class PlannerSpecificVariables>
SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* 
	HashTableContainer<NodeType,CostType,PlannerSpecificVariables>::getNodeInHash(NodeType n)
{
	// Search in bin
	int hashBin = friendGraphDescriptor_p->_getHashBin(n);
	// A SIGSEGV signal generated from here most likely 'getHashBin' returned a bin index larger than (hashTableSize-1).
	for (int a=0; a<HashTable[hashBin].size(); a++)
		if ( HashTable[hashBin][a]->n == n )
			return (HashTable[hashBin][a]);
	
	// If new node, create it!
	SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>* newSearchGraphNode = 
															new SearchGraphNode<NodeType,CostType,PlannerSpecificVariables>;
	newSearchGraphNode->n = n; // WARNING: Nothing else is set yet!
	if ( HashTable[hashBin].capacity() <= HashTable[hashBin].size()+1 )
		HashTable[hashBin].reserve( HashTable[hashBin].capacity() + friendGraphDescriptor_p->hashBinSizeIncreaseStep );
	HashTable[hashBin].push_back(newSearchGraphNode);
	return ( newSearchGraphNode );
}

// =================================================================================


template <class NodeType, class CostType, class PlannerSpecificVariables>
void GenericPlanner<NodeType,CostType,PlannerSpecificVariables>::init
						( GenericSearchGraphDescriptor<NodeType,CostType> theEnv , int heapKeyNum )
{
	GraphDescriptor = new GenericSearchGraphDescriptor<NodeType,CostType>;
	*GraphDescriptor = theEnv;
	
	hash = new HashTableContainer<NodeType,CostType,PlannerSpecificVariables>;
	hash->friendGraphDescriptor_p = GraphDescriptor;
	hash->init_HastTable( GraphDescriptor->hashTableSize );
	
	heap = new HeapContainer<NodeType,CostType,PlannerSpecificVariables>(heapKeyNum);
}


