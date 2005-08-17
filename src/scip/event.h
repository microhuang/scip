/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2005 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2005 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: event.h,v 1.38 2005/08/17 14:25:29 bzfpfend Exp $"

/**@file   event.h
 * @brief  internal methods for managing events
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_EVENT_H__
#define __SCIP_EVENT_H__


#include "scip/def.h"
#include "scip/memory.h"
#include "scip/type_retcode.h"
#include "scip/type_set.h"
#include "scip/type_event.h"
#include "scip/type_lp.h"
#include "scip/type_var.h"
#include "scip/type_sol.h"
#include "scip/type_primal.h"
#include "scip/type_branch.h"
#include "scip/pub_event.h"

#include "scip/struct_event.h"



/*
 * Event handler methods
 */

/** creates an event handler */
extern
RETCODE SCIPeventhdlrCreate(
   EVENTHDLR**      eventhdlr,          /**< pointer to event handler data structure */
   const char*      name,               /**< name of event handler */
   const char*      desc,               /**< description of event handler */
   DECL_EVENTFREE   ((*eventfree)),     /**< destructor of event handler */
   DECL_EVENTINIT   ((*eventinit)),     /**< initialize event handler */
   DECL_EVENTEXIT   ((*eventexit)),     /**< deinitialize event handler */
   DECL_EVENTINITSOL((*eventinitsol)),  /**< solving process initialization method of event handler */
   DECL_EVENTEXITSOL((*eventexitsol)),  /**< solving process deinitialization method of event handler */
   DECL_EVENTDELETE ((*eventdelete)),   /**< free specific event data */
   DECL_EVENTEXEC   ((*eventexec)),     /**< execute event handler */
   EVENTHDLRDATA*   eventhdlrdata       /**< event handler data */
   );

/** calls destructor and frees memory of event handler */
extern
RETCODE SCIPeventhdlrFree(
   EVENTHDLR**      eventhdlr,          /**< pointer to event handler data structure */
   SET*             set                 /**< global SCIP settings */
   );

/** initializes event handler */
extern
RETCODE SCIPeventhdlrInit(
   EVENTHDLR*       eventhdlr,          /**< event handler for this event */
   SET*             set                 /**< global SCIP settings */
   );

/** calls exit method of event handler */
extern
RETCODE SCIPeventhdlrExit(
   EVENTHDLR*       eventhdlr,          /**< event handler for this event */
   SET*             set                 /**< global SCIP settings */
   );

/** informs event handler that the branch and bound process is being started */
extern
RETCODE SCIPeventhdlrInitsol(
   EVENTHDLR*       eventhdlr,          /**< event handler */
   SET*             set                 /**< global SCIP settings */
   );

/** informs event handler that the branch and bound process data is being freed */
extern
RETCODE SCIPeventhdlrExitsol(
   EVENTHDLR*       eventhdlr,          /**< event handler */
   SET*             set                 /**< global SCIP settings */
   );

/** calls execution method of event handler */
extern
RETCODE SCIPeventhdlrExec(
   EVENTHDLR*       eventhdlr,          /**< event handler */
   SET*             set,                /**< global SCIP settings */
   EVENT*           event,              /**< event to call event handler with */
   EVENTDATA*       eventdata           /**< user data for the issued event */
   );




/*
 * Event methods
 */

/** creates an event for an addition of a variable to the problem */
extern
RETCODE SCIPeventCreateVarAdded(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var                 /**< variable that was added to the problem */
   );

/** creates an event for a deletion of a variable from the problem */
extern
RETCODE SCIPeventCreateVarDeleted(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var                 /**< variable that is to be deleted from the problem */
   );

/** creates an event for a fixing of a variable */
extern
RETCODE SCIPeventCreateVarFixed(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var                 /**< variable that was fixed */
   );

/** creates an event for a change in the number of locks of a variable */
extern
RETCODE SCIPeventCreateLocksChanged(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var                 /**< variable that was fixed */
   );

/** creates an event for a change in the objective value of a variable */
extern
RETCODE SCIPeventCreateObjChanged(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var,                /**< variable whose objective value changed */
   Real             oldobj,             /**< old objective value before value changed */
   Real             newobj              /**< new objective value after value changed */
   );

/** creates an event for a change in the lower bound of a variable */
extern
RETCODE SCIPeventCreateLbChanged(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var,                /**< variable whose bound changed */
   Real             oldbound,           /**< old bound before bound changed */
   Real             newbound            /**< new bound after bound changed */
   );

/** creates an event for a change in the upper bound of a variable */
extern
RETCODE SCIPeventCreateUbChanged(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var,                /**< variable whose bound changed */
   Real             oldbound,           /**< old bound before bound changed */
   Real             newbound            /**< new bound after bound changed */
   );

/** creates an event for an addition to the variable's implications list, clique or variable bounds information */
extern
RETCODE SCIPeventCreateImplAdded(
   EVENT**          event,              /**< pointer to store the event */
   BLKMEM*          blkmem,             /**< block memory */
   VAR*             var                 /**< variable that was fixed */
   );

/** frees an event */
extern
RETCODE SCIPeventFree(
   EVENT**          event,              /**< event to free */
   BLKMEM*          blkmem              /**< block memory buffer */
   );

/** sets type of event */
extern
RETCODE SCIPeventChgType(
   EVENT*           event,              /**< event */
   EVENTTYPE        eventtype           /**< new event type */
   );

/** sets node for a node or LP event */
extern
RETCODE SCIPeventChgNode(
   EVENT*           event,              /**< event */
   NODE*            node                /**< new node */
   );

/** sets solution for a primal solution event */
extern
RETCODE SCIPeventChgSol(
   EVENT*           event,              /**< event */
   SOL*             sol                 /**< new primal solution */
   );

/** processes event by calling the appropriate event handlers */
extern
RETCODE SCIPeventProcess(
   EVENT*           event,              /**< event */
   SET*             set,                /**< global SCIP settings */
   PRIMAL*          primal,             /**< primal data; only needed for objchanged events */
   LP*              lp,                 /**< current LP data; only needed for obj/boundchanged events */
   BRANCHCAND*      branchcand,         /**< branching candidate storage; only needed for boundchange events */
   EVENTFILTER*     eventfilter         /**< event filter for global events; not needed for variable specific events */
   );



/*
 * Event filter methods
 */

/** creates an event filter */
extern
RETCODE SCIPeventfilterCreate(
   EVENTFILTER**    eventfilter,        /**< pointer to store the event filter */
   BLKMEM*          blkmem              /**< block memory buffer */
   );

/** frees an event filter and the associated event data entries */
extern
RETCODE SCIPeventfilterFree(
   EVENTFILTER**    eventfilter,        /**< pointer to store the event filter */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set                 /**< global SCIP settings */
   );

/** adds element to event filter */
extern
RETCODE SCIPeventfilterAdd(
   EVENTFILTER*     eventfilter,        /**< event filter */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   EVENTTYPE        eventtype,          /**< event type to catch */
   EVENTHDLR*       eventhdlr,          /**< event handler to call for the event processing */
   EVENTDATA*       eventdata,          /**< event data to pass to the event handler for the event processing */
   int*             filterpos           /**< pointer to store position of event filter entry, or NULL */
   );

/** deletes element from event filter */
extern
RETCODE SCIPeventfilterDel(
   EVENTFILTER*     eventfilter,        /**< event filter */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   EVENTTYPE        eventtype,          /**< event type */
   EVENTHDLR*       eventhdlr,          /**< event handler to call for the event processing */
   EVENTDATA*       eventdata,          /**< event data to pass to the event handler for the event processing */
   int              filterpos           /**< position of event filter entry, or -1 if unknown */
   );

/** processes the event with all event handlers with matching filter setting */
extern
RETCODE SCIPeventfilterProcess(
   EVENTFILTER*     eventfilter,        /**< event filter */
   SET*             set,                /**< global SCIP settings */
   EVENT*           event               /**< event to process */
   );



/*
 * Event queue methods
 */

/** creates an event queue */
extern
RETCODE SCIPeventqueueCreate(
   EVENTQUEUE**     eventqueue          /**< pointer to store the event queue */
   );

/** frees event queue; there must not be any unprocessed eventy in the queue! */
extern
RETCODE SCIPeventqueueFree(
   EVENTQUEUE**     eventqueue          /**< pointer to the event queue */
   );

/** processes event or adds event to the event queue */
extern
RETCODE SCIPeventqueueAdd(
   EVENTQUEUE*      eventqueue,         /**< event queue */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   PRIMAL*          primal,             /**< primal data; only needed for objchanged events */
   LP*              lp,                 /**< current LP data; only needed for obj/boundchanged events */
   BRANCHCAND*      branchcand,         /**< branching candidate storage; only needed for boundchange events */
   EVENTFILTER*     eventfilter,        /**< event filter for global events; not needed for variable specific events */
   EVENT**          event               /**< pointer to event to add to the queue; will be NULL after queue addition */
   );

/** marks queue to delay incoming events until a call to SCIPeventqueueProcess() */
extern
RETCODE SCIPeventqueueDelay(
   EVENTQUEUE*      eventqueue          /**< event queue */
   );

/** processes all events in the queue */
extern
RETCODE SCIPeventqueueProcess(
   EVENTQUEUE*      eventqueue,         /**< event queue */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   PRIMAL*          primal,             /**< primal data */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTFILTER*     eventfilter         /**< event filter for global (not variable dependent) events */
   );

/** returns TRUE iff events of the queue are delayed until the next SCIPeventqueueProcess() call */
extern
Bool SCIPeventqueueIsDelayed(
   EVENTQUEUE*      eventqueue          /**< event queue */
   );

#endif
