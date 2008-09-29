/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2008 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: pricer_healthcare.h,v 1.3 2008/09/29 19:49:58 bzfheinz Exp $"

/**@file   pricer_healthcare.h
 * @brief  healthcare variable pricer
 * @author Arne Nielsen
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __HCP_PRICER_HEALTHCARE__
#define __HCP_PRICER_HEALTHCARE__

#include "scip/scip.h"


/** creates the healthcare variable pricer and includes it in SCIP */
extern
SCIP_RETCODE HCPincludePricerHealthcare(
   SCIP*                 scip                /**< SCIP data structure */
   );


#endif
