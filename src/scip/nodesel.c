/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2002 Tobias Achterberg                              */
/*                            Thorsten Koch                                  */
/*                            Alexander Martin                               */
/*                  2002-2002 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   nodesel.c
 * @brief  datastructures and methods for node selectors
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "nodesel.h"



/** node priority queue data structure */
struct NodePQ
{
   int              len;                /**< number of used element slots */
   int              size;               /**< total number of available element slots */
   NODE**           slots;              /**< array of element slots */
   Real             lowerboundsum;      /**< sum of lower bounds of all nodes in the queue */
   Real             lowerbound;         /**< minimal lower bound value of all nodes in the queue */
   int              nlowerbounds;       /**< number of nodes in the queue with minimal lower bound (0 if invalid) */
};

/** node selector */
struct Nodesel
{
   char*            name;               /**< name of node selector */
   char*            desc;               /**< description of node selector */
   DECL_NODESELINIT((*nodeselinit));    /**< initialise node selector */
   DECL_NODESELEXIT((*nodeselexit));    /**< deinitialise node selector */
   DECL_NODESELSLCT((*nodeselslct));    /**< node selection method */
   DECL_NODESELCOMP((*nodeselcomp));    /**< node comparison method */
   NODESELDATA*     nodeseldata;        /**< node selector data */
   unsigned int     lowestboundfirst:1; /**< does node comparison sorts w.r.t. lower bound as primal criterion? */
   unsigned int     initialized:1;      /**< is node selector initialized? */
};



/* node priority queue methods */

#define PQ_PARENT(q) (((q)-1)/2)
#define PQ_LEFTCHILD(p) (2*(p)+1)
#define PQ_RIGHTCHILD(p) (2*(p)+2)

static
RETCODE nodepqResize(                   /**< resizes node memory to hold at least the given number of nodes */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set,                /**< global SCIP settings */
   int              minsize             /**< minimal number of storeable nodes */
   )
{
   assert(nodepq != NULL);
   
   if( minsize <= nodepq->size )
      return SCIP_OKAY;

   nodepq->size = SCIPsetCalcTreeGrowSize(set, minsize);
   ALLOC_OKAY( reallocMemoryArray(nodepq->slots, nodepq->size) );

   return SCIP_OKAY;
}

static
void nodepqUpdateLowerbound(            /**< updates the cached minimal lower bound of all nodes in the queue */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to be inserted */
   )
{
   assert(nodepq != NULL);
   assert(node != NULL);

   if( SCIPsetIsLE(set, node->lowerbound, nodepq->lowerbound) )
   {
      if( SCIPsetIsEQ(set, node->lowerbound, nodepq->lowerbound) )
         nodepq->nlowerbounds++;
      else
      {
         nodepq->lowerbound = node->lowerbound;
         nodepq->nlowerbounds = 1;
      }
   }
}

RETCODE SCIPnodepqCreate(               /**< creates node priority queue */
   NODEPQ**         nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);

   ALLOC_OKAY( allocMemory(*nodepq) );
   (*nodepq)->len = 0;
   (*nodepq)->size = 0;
   (*nodepq)->slots = NULL;
   (*nodepq)->lowerboundsum = 0.0;
   (*nodepq)->lowerbound = SCIP_INVALID;
   (*nodepq)->nlowerbounds = 0;

   return SCIP_OKAY;
}

void SCIPnodepqDestroy(                 /**< frees node priority queue, but not the data nodes themselves */
   NODEPQ**         nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(*nodepq != NULL);

   freeMemoryArrayNull((*nodepq)->slots);
   freeMemory(*nodepq);
}

RETCODE SCIPnodepqFree(                 /**< frees node priority queue and all nodes in the queue */
   NODEPQ**         nodepq,             /**< pointer to a node priority queue */
   MEMHDR*          memhdr,             /**< block memory buffers */
   const SET*       set,                /**< global SCIP settings */
   TREE*            tree,               /**< branch-and-bound tree */
   LP*              lp                  /**< actual LP data */
   )
{
   int i;

   assert(nodepq != NULL);
   assert(*nodepq != NULL);

   /* free the nodes of the queue */
   for( i = 0; i < (*nodepq)->len; ++i )
   {
      CHECK_OKAY( SCIPnodeFree(&(*nodepq)->slots[i], memhdr, set, tree, lp) );
   }
   
   /* free the queue data structure */
   SCIPnodepqDestroy(nodepq);

   return SCIP_OKAY;
}

RETCODE SCIPnodepqInsert(               /**< inserts node into node priority queue */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to be inserted */
   )
{
   SCIP* scip;
   NODESEL* nodesel;
   int pos;

   assert(nodepq != NULL);
   assert(nodepq->len >= 0);
   assert(set != NULL);
   assert(node != NULL);

   scip = set->scip;
   nodesel = set->nodesel;
   assert(nodesel->nodeselcomp != NULL);

   CHECK_OKAY( nodepqResize(nodepq, set, nodepq->len+1) );

   /* insert node as leaf in the tree, move it towards the root as long it is better than its parent */
   pos = nodepq->len;
   nodepq->len++;
   nodepq->lowerboundsum += node->lowerbound;
   while( pos > 0 && nodesel->nodeselcomp(nodesel, scip, node, nodepq->slots[PQ_PARENT(pos)]) < 0 )
   {
      nodepq->slots[pos] = nodepq->slots[PQ_PARENT(pos)];
      pos = PQ_PARENT(pos);
   }
   nodepq->slots[pos] = node;

   /* update the minimal lower bound */
   nodepqUpdateLowerbound(nodepq, set, node);

   return SCIP_OKAY;
}

static
Bool nodepqDelPos(                      /**< deletes node at given position from the node priority queue;
                                           returns TRUE, if the parent fell down to the free position */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set,                /**< global SCIP settings */
   int              rempos              /**< queue position of node to remove */
   )
{
   SCIP* scip;
   NODESEL* nodesel;
   NODE* lastnode;
   int freepos;
   int childpos;
   int parentpos;
   int brotherpos;
   Bool parentfelldown;

   assert(nodepq != NULL);
   assert(nodepq->len >= 0);
   assert(set != NULL);
   assert(0 <= rempos && rempos < nodepq->len);

   scip = set->scip;
   nodesel = set->nodesel;
   assert(nodesel->nodeselcomp != NULL);

   /* update the minimal lower bound */
   if( nodepq->nlowerbounds > 0 )
   {
      NODE* node;

      node = nodepq->slots[rempos];
      assert(SCIPsetIsGE(set, node->lowerbound, nodepq->lowerbound));

      if( SCIPsetIsEQ(set, node->lowerbound, nodepq->lowerbound) )
      {
         nodepq->nlowerbounds--;
         if( nodepq->nlowerbounds == 0 )
            nodepq->lowerbound = SCIP_INVALID;
      }
   }

   /* remove node of the tree and get a free slot,
    * if the last node of the queue is better than the parent of the removed node:
    *  - move the parent to the free slot, until the last node can be placed in the free slot
    * if the last node of the queue is not better than the parent of the free slot:
    *  - move the better child to the free slot until the last node can be placed in the free slot
    */
   lastnode = nodepq->slots[nodepq->len-1];
   nodepq->len--;
   nodepq->lowerboundsum -= nodepq->slots[rempos]->lowerbound;
   freepos = rempos;

   /* try to move parents downwards to insert last node */
   parentfelldown = FALSE;
   parentpos = PQ_PARENT(freepos);
   while( freepos > 0 && nodesel->nodeselcomp(nodesel, scip, lastnode, nodepq->slots[parentpos]) < 0 )
   {
      nodepq->slots[freepos] = nodepq->slots[parentpos];
      freepos = parentpos;
      parentpos = PQ_PARENT(freepos);
      parentfelldown = TRUE;
   }
   if( !parentfelldown )
   {
      /* downward moving of parents was not successful -> move children upwards */
      while( freepos < PQ_PARENT(nodepq->len-1) ) /* as long as free slot has children... */
      {
         /* select the better child of free slot */
         childpos = PQ_LEFTCHILD(freepos);
         assert(childpos < nodepq->len);
         brotherpos = PQ_RIGHTCHILD(freepos);
         if( brotherpos < nodepq->len
            && nodesel->nodeselcomp(nodesel, scip, nodepq->slots[brotherpos], nodepq->slots[childpos]) < 0 )
            childpos = brotherpos;
         /* exit search loop if better child is not better than last node */
         if( nodesel->nodeselcomp(nodesel, scip, lastnode, nodepq->slots[childpos]) <= 0 )
            break;
         /* move better child upwards, free slot is now the better child's slot */
         nodepq->slots[freepos] = nodepq->slots[childpos];
         freepos = childpos;
      }
   }
   assert(0 <= freepos && freepos < nodepq->len);
   assert(!parentfelldown || PQ_LEFTCHILD(freepos) < nodepq->len);
   nodepq->slots[freepos] = lastnode;

   return parentfelldown;
}

NODE* SCIPnodepqRemove(                 /**< removes and returns best node from the node priority queue */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   NODE* root;

   assert(nodepq != NULL);

   if( nodepq->len == 0 )
      return NULL;

   root = nodepq->slots[0];
   nodepqDelPos(nodepq, set, 0);

   return root;
}

NODE* SCIPnodepqFirst(                  /**< returns the best node of the queue without removing it */
   const NODEPQ*    nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(nodepq->len >= 0);

   if( nodepq->len == 0 )
      return NULL;

   return nodepq->slots[0];
}

int SCIPnodepqLen(                      /**< returns the number of nodes stored in the node priority queue */
   const NODEPQ*    nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(nodepq->len >= 0);

   return nodepq->len;
}

Real SCIPnodepqGetLowerbound(           /**< gets the minimal lower bound of all nodes in the queue */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   NODE* node;
   int i;

   assert(nodepq != NULL);
   assert(set != NULL);
   assert(set->nodesel != NULL);

   if( set->nodesel->lowestboundfirst )
   {
      /* the node selector's compare method sorts the minimal lower bound to the front */
      if( nodepq->len > 0 )
      {
         assert(nodepq->slots[0] != NULL);
         nodepq->lowerbound = nodepq->slots[0]->lowerbound;
      }
      else
         nodepq->lowerbound = set->infinity;
   }
   else
   {
      /* if we don't know the minimal lower bound, compare all nodes */
      if( nodepq->nlowerbounds == 0 )
      {
         nodepq->lowerbound = set->infinity;
         nodepq->nlowerbounds = 0;
         for( i = 0; i < nodepq->len; ++i )
            nodepqUpdateLowerbound(nodepq, set, nodepq->slots[i]);
      }
   }
   assert(nodepq->lowerbound < SCIP_INVALID);

   return nodepq->lowerbound;
}

Real SCIPnodepqGetLowerboundSum(        /**< gets the sum of lower bounds of all nodes in the queue */
   NODEPQ*          nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);

   return nodepq->lowerboundsum;
}

RETCODE SCIPnodepqBound(                /**< free all nodes from the queue that are cut off by the given upper bound */
   NODEPQ*          nodepq,             /**< pointer to a node priority queue */
   MEMHDR*          memhdr,             /**< block memory buffer */
   const SET*       set,                /**< global SCIP settings */
   TREE*            tree,               /**< branch-and-bound tree */
   LP*              lp,                 /**< actual LP data */
   Real             upperbound          /**< upper bound: all nodes with lowerbound >= upperbound are cut off */
   )
{
   NODE* node;
   int pos;
   Bool parentfelldown;

   assert(nodepq != NULL);

   debugMessage("bounding node queue of length %d with upperbound=%g\n", nodepq->len, upperbound);
   pos = nodepq->len-1;
   while( pos >= 0 )
   {
      assert(pos < nodepq->len);
      node = nodepq->slots[pos];
      assert(node != NULL);
      assert(node->nodetype == SCIP_NODETYPE_LEAF);
      if( SCIPsetIsGE(set, node->lowerbound, upperbound) )
      {
         debugMessage("free node in slot %d at depth %d with lowerbound=%g\n", pos, node->depth, node->lowerbound);
         /* cut off node; because we looped from back to front, the node must be a leaf of the PQ tree */
         assert(PQ_LEFTCHILD(pos) >= nodepq->len);

         /* free the slot in the node PQ */
         parentfelldown = nodepqDelPos(nodepq, set, pos);

         /* - if the slot was occupied by the parent, we have to check this slot (the parent) again; unfortunately,
          *   we will check the node which occupied the parent's slot again, even though it cannot be cut off;
          * - otherwise, the slot was the last slot or it was occupied by a node with a position greater than
          *   the actual position; this node was already checked and we can decrease the position
          */
         if( !parentfelldown )
            pos--;

         /* free memory of the node */
         CHECK_OKAY( SCIPnodeFree(&node, memhdr, set, tree, lp) );
      }
      else
         pos--;
   }
   debugMessage(" -> bounded node queue has length %d\n", nodepq->len);

   return SCIP_OKAY;
}



/* node selector methods */

RETCODE SCIPnodeselCreate(              /**< creates a node selector */
   NODESEL**        nodesel,            /**< pointer to store node selector */
   const char*      name,               /**< name of node selector */
   const char*      desc,               /**< description of node selector */
   DECL_NODESELINIT((*nodeselinit)),    /**< initialise node selector */
   DECL_NODESELEXIT((*nodeselexit)),    /**< deinitialise node selector */
   DECL_NODESELSLCT((*nodeselslct)),    /**< node selection method */
   DECL_NODESELCOMP((*nodeselcomp)),    /**< node comparison method */
   NODESELDATA*     nodeseldata,        /**< node selector data */
   Bool             lowestboundfirst    /**< does node comparison sorts w.r.t. lower bound as primal criterion? */
   )
{
   assert(nodesel != NULL);
   assert(name != NULL);
   assert(desc != NULL);
   assert(nodeselslct != NULL);
   assert(nodeselcomp != NULL);

   ALLOC_OKAY( allocMemory(*nodesel) );
   ALLOC_OKAY( duplicateMemoryArray((*nodesel)->name, name, strlen(name)+1) );
   ALLOC_OKAY( duplicateMemoryArray((*nodesel)->desc, desc, strlen(desc)+1) );
   (*nodesel)->nodeselinit = nodeselinit;
   (*nodesel)->nodeselexit = nodeselexit;
   (*nodesel)->nodeselslct = nodeselslct;
   (*nodesel)->nodeselcomp = nodeselcomp;
   (*nodesel)->nodeseldata = nodeseldata;
   (*nodesel)->lowestboundfirst = lowestboundfirst;
   (*nodesel)->initialized = FALSE;

   return SCIP_OKAY;
}
   
RETCODE SCIPnodeselFree(                /**< frees memory of node selector */
   NODESEL**        nodesel             /**< pointer to node selector data structure */
   )
{
   assert(nodesel != NULL);
   assert(*nodesel != NULL);
   assert(!(*nodesel)->initialized);

   freeMemoryArray((*nodesel)->name);
   freeMemoryArray((*nodesel)->desc);
   freeMemory(*nodesel);

   return SCIP_OKAY;
}

RETCODE SCIPnodeselInit(                /**< initializes node selector */
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip                /**< SCIP data structure */   
   )
{
   assert(nodesel != NULL);
   assert(scip != NULL);

   if( nodesel->initialized )
   {
      char s[255];
      sprintf(s, "Node selector <%s> already initialized", nodesel->name);
      errorMessage(s);
      return SCIP_INVALIDCALL;
   }

   if( nodesel->nodeselinit != NULL )
   {
      CHECK_OKAY( nodesel->nodeselinit(nodesel, scip) );
   }
   nodesel->initialized = TRUE;

   return SCIP_OKAY;
}

RETCODE SCIPnodeselExit(                /**< deinitializes node selector */
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip                /**< SCIP data structure */   
   )
{
   assert(nodesel != NULL);
   assert(scip != NULL);

   if( !nodesel->initialized )
   {
      char s[255];
      sprintf(s, "Node selector <%s> not initialized", nodesel->name);
      errorMessage(s);
      return SCIP_INVALIDCALL;
   }

   if( nodesel->nodeselexit != NULL )
   {
      CHECK_OKAY( nodesel->nodeselexit(nodesel, scip) );
   }
   nodesel->initialized = FALSE;

   return SCIP_OKAY;
}

RETCODE SCIPnodeselSelect(              /**< select next node to be processed */
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip,               /**< SCIP data structure */   
   NODE**           selnode             /**< pointer to store node to be processed next */
   )
{
   assert(nodesel != NULL);
   assert(nodesel->nodeselslct != NULL);
   assert(scip != NULL);
   assert(selnode != NULL);

   CHECK_OKAY( nodesel->nodeselslct(nodesel, scip, selnode) );

   return SCIP_OKAY;
}

int SCIPnodeselCompare(                 /**< compares two nodes; returns -1/0/+1 if node1 better/equal/worse than node2 */
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip,               /**< SCIP data structure */   
   NODE*            node1,              /**< first node to compare */
   NODE*            node2               /**< second node to compare */
   )
{
   assert(nodesel != NULL);
   assert(nodesel->nodeselcomp != NULL);
   assert(scip != NULL);
   assert(node1 != NULL);
   assert(node2 != NULL);

   return nodesel->nodeselcomp(nodesel, scip, node1, node2);
}

const char* SCIPnodeselGetName(         /**< gets name of node selector */
   NODESEL*         nodesel             /**< node selector */
   )
{
   assert(nodesel != NULL);

   return nodesel->name;
}

Bool SCIPnodeselIsInitialized(          /**< is node selector initialized? */
   NODESEL*         nodesel             /**< node selector */
   )
{
   assert(nodesel != NULL);

   return nodesel->initialized;
}

