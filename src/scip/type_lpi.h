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
#pragma ident "@(#) $Id: type_lpi.h,v 1.8 2005/01/11 14:33:24 bzfpfend Exp $"

/**@file   type_lpi.h
 * @brief  type definitions for specific LP solvers interface
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __TYPE_LPI_H__
#define __TYPE_LPI_H__


/** objective sense */
enum ObjSen
{
   SCIP_OBJSEN_MAXIMIZE = -1,           /**< maximize objective function */
   SCIP_OBJSEN_MINIMIZE = +1            /**< minimize objective function */
};
typedef enum ObjSen OBJSEN;

/** LP solver parameters */
enum LPParam
{
   SCIP_LPPAR_FROMSCRATCH =  0,         /**< solver should start from scratch at next call? */
   SCIP_LPPAR_FASTMIP     =  1,         /**< fast mip setting of LP solver */
   SCIP_LPPAR_SCALING     =  2,         /**< should LP solver use scaling? */
   SCIP_LPPAR_PRESOLVING  =  3,         /**< should LP solver use presolving? */
   SCIP_LPPAR_PRICING     =  4,         /**< pricing strategy */
   SCIP_LPPAR_LPINFO      =  5,         /**< should LP solver output information to the screen? */
   SCIP_LPPAR_FEASTOL     =  6,         /**< feasibility tolerance for primal variables and slacks */
   SCIP_LPPAR_DUALFEASTOL =  7,         /**< feasibility tolerance for dual variables and reduced costs */
   SCIP_LPPAR_LOBJLIM     =  8,         /**< lower objective limit */
   SCIP_LPPAR_UOBJLIM     =  9,         /**< upper objective limit */
   SCIP_LPPAR_LPITLIM     = 10,         /**< LP iteration limit */
   SCIP_LPPAR_LPTILIM     = 11          /**< LP time limit */
};
typedef enum LPParam LPPARAM;

/** LP pricing strategy */
enum Pricing
{
   SCIP_PRICING_AUTO        = 0,        /**< the LP solver should use its prefered strategy */
   SCIP_PRICING_FULL        = 1,        /**< full pricing */
   SCIP_PRICING_STEEP       = 2,        /**< steepest edge pricing */
   SCIP_PRICING_STEEPQSTART = 3         /**< steepest edge pricing without initial dual norms */
};
typedef enum Pricing PRICING;

/** basis status for columns and rows */
enum BaseStat
{
   SCIP_BASESTAT_LOWER = 0,             /**< (slack) variable is at its lower bound */
   SCIP_BASESTAT_BASIC = 1,             /**< (slack) variable is basic */
   SCIP_BASESTAT_UPPER = 2,             /**< (slack) variable is at its upper bound */
   SCIP_BASESTAT_ZERO  = 3              /**< free variable is non-basic and set to zero */
};
typedef enum BaseStat BASESTAT;

typedef struct LPi LPI;                 /**< solver dependent LP interface */
typedef struct LPiState LPISTATE;       /**< complete LP state (i.e. basis information, dual norms) */


#endif
