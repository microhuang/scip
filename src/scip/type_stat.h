/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2004 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2004 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: type_stat.h,v 1.3 2004/10/29 10:39:00 bzfpfend Exp $"

/**@file   type_stat.h
 * @brief  type definitions for problem statistics
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __TYPE_STAT_H__
#define __TYPE_STAT_H__


/** SCIP solving status */
enum Status
{
   SCIP_STATUS_UNKNOWN       =  0,      /**< the solving status is not yet known */
   SCIP_STATUS_USERINTERRUPT =  1,      /**< the user interrupted the solving process (by pressing Ctrl-C) */
   SCIP_STATUS_NODELIMIT     =  2,      /**< the solving process was interrupted because the node limit was reached */
   SCIP_STATUS_TIMELIMIT     =  3,      /**< the solving process was interrupted because the time limit was reached */
   SCIP_STATUS_MEMLIMIT      =  4,      /**< the solving process was interrupted because the memory limit was reached */
   SCIP_STATUS_GAPLIMIT      =  5,      /**< the solving process was interrupted because the gap limit was reached */
   SCIP_STATUS_SOLLIMIT      =  6,      /**< the solving process was interrupted because the solution limit was reached */
   SCIP_STATUS_BESTSOLLIMIT  =  7,      /**< the solving process was interrupted because the solution improvement limit
                                         *   was reached */
   SCIP_STATUS_OPTIMAL       =  8,      /**< the problem was solved to optimality, an optimal solution is available */
   SCIP_STATUS_INFEASIBLE    =  9,      /**< the problem was proven to be infeasible */
   SCIP_STATUS_UNBOUNDED     = 10,      /**< the problem was proven to be unbounded */
   SCIP_STATUS_INFORUNBD     = 11       /**< the problem was proven to be either infeasible or unbounded */
};
typedef enum Status STATUS;

typedef struct Stat STAT;               /**< problem and runtime specific statistics */


#endif
