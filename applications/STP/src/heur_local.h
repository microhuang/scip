/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2021 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scipopt.org.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   heur_local.h
 * @brief  Improvement heuristic for Steiner problems
 * @author Daniel Rehfeldt
 *
 * This file implements three local search heuristics, namely vertex insertion, key-path exchange and key-vertex elimination,
 * see "Fast Local Search for Steiner Trees in Graphs" by Uchoa and Werneck. Furthermore, it includes several non-published local
 * search heuristics for prize-collecting Steiner problem tree variants.
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_HEUR_LOCAL_H__
#define __SCIP_HEUR_LOCAL_H__


#include "scip/scip.h"
#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the local primal heuristic and includes it in SCIP */
SCIP_EXPORT
SCIP_RETCODE SCIPStpIncludeHeurLocal(
   SCIP*                 scip                /**< SCIP data structure */
   );

/** perform local heuristics on a given Steiner tree */
SCIP_EXPORT
SCIP_RETCODE SCIPStpHeurLocalRun(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                graph,              /**< graph data structure */
   int*                  best_result         /**< array indicating whether an arc is part of the solution (CONNECTED/UNKNOWN) */
   );


/** perform fast local heuristics on a given Steiner tree */
SCIP_EXPORT
SCIP_RETCODE SCIPStpHeurLocalRunFast(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                graph,              /**< graph data structure */
   int*                  best_result         /**< array indicating whether an arc is part of the solution (CONNECTED/UNKNOWN) */
   );

/** Implication based local heuristic for (R)PC and MW */
SCIP_EXPORT
SCIP_RETCODE SCIPStpHeurLocalExtendPcMwImp(
   SCIP*                 scip,               /**< SCIP data structure */
   const GRAPH*          graph,              /**< graph data structure */
   int*                  result              /**< array indicating whether an arc is part of the solution (CONNECTED/UNKNOWN) */
   );

/** greedy extension local heuristic for (R)PC and MW */
SCIP_EXPORT
SCIP_RETCODE SCIPStpHeurLocalExtendPcMw(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                graph,              /**< graph data structure */
   const SCIP_Real*      cost,               /**< edge cost array*/
   int*                  stedge,             /**< initialized array to indicate whether an edge is part of the Steiner tree */
   STP_Bool*             stvertex            /**< uninitialized array to indicate whether a vertex is part of the Steiner tree */
);

/** greedy extension local heuristic for (R)PC and MW */
SCIP_EXPORT
SCIP_RETCODE SCIPStpHeurLocalExtendPcMwOut(
   SCIP*                 scip,               /**< SCIP data structure */
   GRAPH*                graph,              /**< graph data structure */
   int*                  stedge,             /**< initialized array to indicate whether an edge is part of the Steiner tree */
   STP_Bool*             stvertex            /**< uninitialized array to indicate whether a vertex is part of the Steiner tree */
);

#ifdef __cplusplus
}
#endif

#endif
