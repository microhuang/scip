/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2016 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   expr_sum.h
 * @ingroup EXPRHDLRS
 * @brief  sum expression handler
 * @author Stefan Vigerske
 * @author Benjamin Mueller
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_EXPR_SUM_H__
#define __SCIP_EXPR_SUM_H__


#include "scip/scip.h"
#include "scip/type_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the handler for sum expressions and includes it into SCIP
 *
 * @ingroup ExprhdlrIncludes
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeExprhdlrSum(
   SCIP*                 scip                /**< SCIP data structure */
   );

/**@addtogroup EXPRHDLRS
 *
 * @{
 *
 * @name Sum expression
 *
 * This expression handler provides the sum function, that is,
 * \f[
 *   x \mapsto c + \sum_{i=1}^n a_i x_i
 * \f]
 * for some constant c and constant coefficients \f$a_i\f$.
 *
 * @{
 */

/** creates a sum expression */
SCIP_EXPORT
SCIP_RETCODE SCIPcreateExprSum(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR**           expr,               /**< pointer where to store expression */
   int                   nchildren,          /**< number of children */
   SCIP_EXPR**           children,           /**< children */
   SCIP_Real*            coefficients,       /**< array with coefficients for all children (or NULL if all 1.0) */
   SCIP_Real             constant,           /**< constant term of sum */
   SCIP_DECL_EXPR_OWNERCREATE((*ownercreate)), /**< function to call to create ownerdata */
   void*                 ownercreatedata     /**< data to pass to ownercreate */
   );

/** sets the constant of a summation expression */
SCIP_EXPORT
void SCIPsetConstantExprSum(
   SCIP_EXPR*            expr,               /**< sum expression */
   SCIP_Real             constant            /**< constant */
   );

/** appends an expression to a sum expression */
SCIP_EXPORT
SCIP_RETCODE SCIPappendExprSumExpr(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR*            expr,               /**< sum expression */
   SCIP_EXPR*            child,              /**< expression to be appended */
   SCIP_Real             childcoef           /**< child's coefficient */
   );

/** multiplies given sum expression by a constant */
SCIP_EXPORT
void SCIPmultiplyByConstantExprSum(
   SCIP_EXPR*            expr,               /**< sum expression */
   SCIP_Real             constant            /**< constant that multiplies sum expression */
   );

/** @}
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __SCIP_EXPR_SUM_H__ */
