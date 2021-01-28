/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2020 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   expr_log.c
 * @brief  logarithm expression handler
 * @author Stefan Vigerske
 * @author Benjamin Mueller
 * @author Ksenia Bestuzheva
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <string.h>

#include "scip/expr_value.h"
#include "scip/expr_log.h"

#define EXPRHDLR_NAME         "log"
#define EXPRHDLR_DESC         "logarithmic expression"
#define EXPRHDLR_PRECEDENCE  80000
#define EXPRHDLR_HASHKEY     SCIPcalcFibHash(16273.0)

/*
 * Data structures
 */

/** expression handler data */
struct SCIP_ExprHdlrData
{
   SCIP_Real             minzerodistance;    /**< minimal distance from zero to enforce for child in bound tightening */
   SCIP_Bool             warnedonpole;       /**< whether we warned on enforcing a minimal non-zero bound for child */
};

/*
 * Local methods
 */


/*
 * Callback methods of expression handler
 */

/** simplifies a log expression.
 * Evaluates the logarithm function when its child is a value expression
 * TODO: split products ?
 * TODO: log(exp(*)) = *
 */
static
SCIP_DECL_EXPRSIMPLIFY(simplifyLog)
{  /*lint --e{715}*/
   SCIP_EXPR* child;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(simplifiedexpr != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);

   child = SCIPexprGetChildren(expr)[0];
   assert(child != NULL);

   /* check for value expression */
   if( SCIPisExprValue(scip, child) )
   {
      /* TODO how to handle a non-positive value? */
      assert(SCIPgetValueExprValue(child) > 0.0);

      SCIP_CALL( SCIPcreateExprValue(scip, simplifiedexpr, log(SCIPgetValueExprValue(child)), ownercreate,
            ownercreatedata) );
   }
   else
   {
      *simplifiedexpr = expr;

      /* we have to capture it, since it must simulate a "normal" simplified call in which a new expression is created */
      SCIPcaptureExpr(*simplifiedexpr);
   }

   return SCIP_OKAY;
}

static
SCIP_DECL_EXPRCOPYHDLR(copyhdlrLog)
{  /*lint --e{715}*/
   SCIP_CALL( SCIPincludeExprHdlrLog(scip) );

   return SCIP_OKAY;
}

static
SCIP_DECL_EXPRFREEHDLR(freehdlrLog)
{  /*lint --e{715}*/
   assert(exprhdlrdata != NULL);
   assert(*exprhdlrdata != NULL);

   SCIPfreeBlockMemory(scip, exprhdlrdata);

   return SCIP_OKAY;
}

static
SCIP_DECL_EXPRCOPYDATA(copydataLog)
{  /*lint --e{715}*/
   assert(targetexprdata != NULL);
   assert(sourceexpr != NULL);
   assert(SCIPexprGetData(sourceexpr) == NULL);

   *targetexprdata = NULL;

   return SCIP_OKAY;
}

static
SCIP_DECL_EXPRFREEDATA(freedataLog)
{  /*lint --e{715}*/
   assert(expr != NULL);

   SCIPexprSetData(expr, NULL);

   return SCIP_OKAY;
}

static
SCIP_DECL_EXPRPARSE(parseLog)
{  /*lint --e{715}*/
   SCIP_EXPR* childexpr;

   assert(expr != NULL);

   /* parse child expression from remaining string */
   SCIP_CALL( SCIPparseExpr(scip, &childexpr, string, endstring, ownercreate, ownercreatedata) );
   assert(childexpr != NULL);

   /* create logarithmic expression */
   SCIP_CALL( SCIPcreateExprLog(scip, expr, childexpr, ownercreate, ownercreatedata) );
   assert(*expr != NULL);

   /* release child expression since it has been captured by the logarithmic expression */
   SCIP_CALL( SCIPreleaseExpr(scip, &childexpr) );

   *success = TRUE;

   return SCIP_OKAY;
}

/** expression point evaluation callback */
static
SCIP_DECL_EXPREVAL(evalLog)
{  /*lint --e{715}*/
   assert(expr != NULL);
   assert(SCIPexprGetData(expr) == NULL);
   assert(SCIPexprGetNChildren(expr) == 1);
   assert(SCIPexprGetEvalValue(SCIPexprGetChildren(expr)[0]) != SCIP_INVALID); /*lint !e777*/

   if( SCIPexprGetEvalValue(SCIPexprGetChildren(expr)[0]) <= 0.0 )
   {
      SCIPdebugMsg(scip, "invalid evaluation of logarithmic expression\n");
      *val = SCIP_INVALID;
   }
   else
   {
      *val = log(SCIPexprGetEvalValue(SCIPexprGetChildren(expr)[0]));
   }

   return SCIP_OKAY;
}

/** expression derivative evaluation callback */
static
SCIP_DECL_EXPRBWDIFF(bwdiffLog)
{  /*lint --e{715}*/
   SCIP_EXPR* child;

   assert(expr != NULL);
   assert(childidx == 0);
   assert(SCIPexprGetEvalValue(expr) != SCIP_INVALID); /*lint !e777*/

   child = SCIPexprGetChildren(expr)[0];
   assert(child != NULL);
   assert(strcmp(SCIPexprhdlrGetName(SCIPexprGetHdlr(child)), "val") != 0);
   assert(SCIPexprGetEvalValue(child) > 0.0);

   *val = 1.0 / SCIPexprGetEvalValue(child);

   return SCIP_OKAY;
}

/** expression interval evaluation callback */
static
SCIP_DECL_EXPRINTEVAL(intevalLog)
{  /*lint --e{715}*/
   SCIP_EXPRHDLRDATA* exprhdlrdata;
   SCIP_INTERVAL childinterval;

   assert(expr != NULL);
   assert(SCIPexprGetData(expr) == NULL);
   assert(SCIPexprGetNChildren(expr) == 1);

   exprhdlrdata = SCIPexprhdlrGetData(SCIPexprGetHdlr(expr));
   assert(exprhdlrdata != NULL);

   childinterval = SCIPexprGetActivity(SCIPexprGetChildren(expr)[0]);

   /* pretend childinterval to be >= epsilon, see also reversepropLog */
   if( childinterval.inf < exprhdlrdata->minzerodistance && exprhdlrdata->minzerodistance > 0.0 )
   {
      if( !exprhdlrdata->warnedonpole && SCIPgetVerbLevel(scip) > SCIP_VERBLEVEL_NONE )
      {
         SCIPinfoMessage(scip, NULL, "Changing lower bound for child of log() from %g to %g.\n"
            "Check your model formulation or use option expr/" EXPRHDLR_NAME "/minzerodistance to avoid this warning.\n",
            childinterval.inf, exprhdlrdata->minzerodistance);
         SCIPinfoMessage(scip, NULL, "Expression: ");
         SCIP_CALL( SCIPprintExpr(scip, expr, NULL) );
         SCIPinfoMessage(scip, NULL, "\n");
         exprhdlrdata->warnedonpole = TRUE;
      }
      childinterval.inf = exprhdlrdata->minzerodistance;
   }

   if( SCIPintervalIsEmpty(SCIP_INTERVAL_INFINITY, childinterval) )
   {
      SCIPintervalSetEmpty(interval);
      return SCIP_OKAY;
   }

   SCIPintervalLog(SCIP_INTERVAL_INFINITY, interval, childinterval);

   return SCIP_OKAY;
}

/** expression estimation callback */
static
SCIP_DECL_EXPRESTIMATE(estimateLog)
{  /*lint --e{715}*/
   SCIP_Real lb;
   SCIP_Real ub;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);
   assert(strcmp(SCIPexprhdlrGetName(SCIPexprGetHdlr(expr)), EXPRHDLR_NAME) == 0);
   assert(coefs != NULL);
   assert(constant != NULL);
   assert(islocal != NULL);
   assert(branchcand != NULL);
   assert(*branchcand == TRUE);
   assert(success != NULL);
   assert(refpoint != NULL);

   lb = localbounds[0].inf;
   ub = localbounds[0].sup;

   *coefs = 0.0;
   *constant = 0.0;
   *success = TRUE;

   if( overestimate )
   {
      if( !SCIPisPositive(scip, refpoint[0]) )
      {
         /* if refpoint is 0 (then lb=0 probably) or below, then slope is infinite, then try to move away from 0 */
         if( SCIPisZero(scip, ub) )
         {
            *success = FALSE;
            return SCIP_OKAY;
         }

         if( localbounds[0].sup < 0.2 )
            refpoint[0] = 0.5 * lb + 0.5 * ub;
         else
            refpoint[0] = 0.1;
      }

      SCIPaddLogLinearization(scip, refpoint[0], SCIPexprIsIntegral(SCIPexprGetChildren(expr)[0]), coefs, constant, success);
      *islocal = FALSE; /* linearization is globally valid */
      *branchcand = FALSE;
   }
   else
   {
      SCIPaddLogSecant(scip, lb, ub, coefs, constant, success);
      *islocal = TRUE; /* secants are only valid locally */
   }

   return SCIP_OKAY;
}

/** init sepa callback that initializes LP for a logarithm expression */
static
SCIP_DECL_EXPRINITESTIMATES(initestimatesLog)
{
   SCIP_Real refpointsover[3] = {SCIP_INVALID, SCIP_INVALID, SCIP_INVALID};
   SCIP_Bool overest[4] = {TRUE, TRUE, TRUE, FALSE};
   SCIP_EXPR* child;
   SCIP_Real lb;
   SCIP_Real ub;
   SCIP_Bool success;
   int i;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);
   assert(strcmp(SCIPexprhdlrGetName(SCIPexprGetHdlr(expr)), EXPRHDLR_NAME) == 0);

   /* get expression data */
   child = SCIPexprGetChildren(expr)[0];
   assert(child != NULL);

   lb = SCIPintervalGetInf(bounds[0]);
   ub = SCIPintervalGetSup(bounds[0]);

   if( SCIPisEQ(scip, lb, ub) )
      return SCIP_OKAY;

   if( overestimate )
   {
      /* adjust lb */
      lb = MAX(lb, MIN(0.5 * lb + 0.5 * ub, 0.1));

      refpointsover[0] = lb;
      refpointsover[1] = SCIPisInfinity(scip, ub) ? lb + 2.0 : (lb + ub) / 2;
      refpointsover[2] = SCIPisInfinity(scip, ub) ? lb + 20.0 : ub;
   }

   *nreturned = 0;

   for( i = 0; i < 4; ++i )
   {
      if( (overest[i] && !overestimate) || (!overest[i] && (overestimate || SCIPisInfinity(scip, ub))) )
         continue;

      assert(!overest[i] || (SCIPisLE(scip, refpointsover[i], ub) && SCIPisGE(scip, refpointsover[i], lb))); /*lint !e661*/

      success = TRUE;
      coefs[*nreturned][0] = 0.0;
      constant[*nreturned] = 0.0;

      if( overest[i] )
      {
         assert(i < 3);
         SCIPaddLogLinearization(scip, refpointsover[i], SCIPexprIsIntegral(child), coefs[*nreturned], &constant[*nreturned], &success); /*lint !e661*/
      }
      else
         SCIPaddLogSecant(scip, lb, ub, coefs[*nreturned], &constant[*nreturned], &success);

      if( success )
      {
         ++(*nreturned);
      }
   }

   return SCIP_OKAY;
}

/** expression reverse propagation callback */
static
SCIP_DECL_EXPRREVERSEPROP(reversepropLog)
{  /*lint --e{715}*/
   SCIP_EXPRHDLRDATA* exprhdlrdata;

   assert(scip != NULL);
   assert(expr != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);

   exprhdlrdata = SCIPexprhdlrGetData(SCIPexprGetHdlr(expr));
   assert(exprhdlrdata != NULL);

   /* f = log(c0) -> c0 = exp(f) */
   SCIPintervalExp(SCIP_INTERVAL_INFINITY, &childrenbounds[0], bounds);

   /* force child lower bound to be at least epsilon away from 0
    * this can help a lot in enforcement (try ex8_5_3)
    * child being equal 0 is already forbidden, so making it strictly greater-equal epsilon enforces
    * and hopefully doesn't introduce much problems
    * if childrenbounds[0].sup < epsilon, too, then this will result in a cutoff
    */
   if( childrenbounds[0].inf < exprhdlrdata->minzerodistance )
   {
      SCIPdebugMsg(scip, "Pushing child lower bound from %g to %g; upper bound remains at %g\n", childrenbounds[0].inf, SCIPepsilon(scip), childrenbounds[0].sup);

      if( !exprhdlrdata->warnedonpole && SCIPgetVerbLevel(scip) > SCIP_VERBLEVEL_NONE )
      {
         SCIPinfoMessage(scip, NULL, "Changing lower bound for child of log() from %g to %g.\n"
            "Check your model formulation or use option expr/" EXPRHDLR_NAME "/minzerodistance to avoid this warning.\n",
            childrenbounds[0].inf, exprhdlrdata->minzerodistance);
         SCIPinfoMessage(scip, NULL, "Expression: ");
         SCIP_CALL( SCIPprintExpr(scip, expr, NULL) );
         SCIPinfoMessage(scip, NULL, "\n");
         exprhdlrdata->warnedonpole = TRUE;
      }

      childrenbounds[0].inf = exprhdlrdata->minzerodistance;
   }

   return SCIP_OKAY;
}

/** expression hash callback */
static
SCIP_DECL_EXPRHASH(hashLog)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(expr != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);
   assert(hashkey != NULL);
   assert(childrenhashes != NULL);

   *hashkey = EXPRHDLR_HASHKEY;
   *hashkey ^= childrenhashes[0];

   return SCIP_OKAY;
}

/** expression curvature detection callback */
static
SCIP_DECL_EXPRCURVATURE(curvatureLog)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(expr != NULL);
   assert(childcurv != NULL);
   assert(SCIPexprGetNChildren(expr) == 1);

   /* expression is concave if child is concave, expression cannot be linear or convex */
   if( exprcurvature == SCIP_EXPRCURV_CONCAVE )
   {
      *childcurv = SCIP_EXPRCURV_CONCAVE;
      *success = TRUE;
   }
   else
      *success = FALSE;

   return SCIP_OKAY;
}

/** expression monotonicity detection callback */
static
SCIP_DECL_EXPRMONOTONICITY(monotonicityLog)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(expr != NULL);
   assert(result != NULL);
   assert(childidx == 0);

   *result = SCIP_MONOTONE_INC;

   return SCIP_OKAY;
}

/** creates the handler for logarithmic expression and includes it into SCIP */
SCIP_RETCODE SCIPincludeExprHdlrLog(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_EXPRHDLR* exprhdlr;
   SCIP_EXPRHDLRDATA* exprhdlrdata;

   SCIP_CALL( SCIPallocClearBlockMemory(scip, &exprhdlrdata) );

   SCIP_CALL( SCIPincludeExprHdlr(scip, &exprhdlr, EXPRHDLR_NAME, EXPRHDLR_DESC, EXPRHDLR_PRECEDENCE, evalLog,
         exprhdlrdata) );
   assert(exprhdlr != NULL);

   SCIPexprhdlrSetCopyFreeHdlr(exprhdlr, copyhdlrLog, freehdlrLog);
   SCIPexprhdlrSetCopyFreeData(exprhdlr, copydataLog, freedataLog);
   SCIPexprhdlrSetSimplify(exprhdlr, simplifyLog);
   SCIPexprhdlrSetParse(exprhdlr, parseLog);
   SCIPexprhdlrSetIntEval(exprhdlr, intevalLog);
   SCIPexprhdlrSetEstimate(exprhdlr, initestimatesLog, estimateLog);
   SCIPexprhdlrSetReverseProp(exprhdlr, reversepropLog);
   SCIPexprhdlrSetHash(exprhdlr, hashLog);
   SCIPexprhdlrSetDiff(exprhdlr, bwdiffLog, NULL, NULL);
   SCIPexprhdlrSetCurvature(exprhdlr, curvatureLog);
   SCIPexprhdlrSetMonotonicity(exprhdlr, monotonicityLog);

   SCIP_CALL( SCIPaddRealParam(scip, "expr/" EXPRHDLR_NAME "/minzerodistance",
      "minimal distance from zero to enforce for child in bound tightening",
      &exprhdlrdata->minzerodistance, FALSE, SCIPepsilon(scip), 0.0, 1.0, NULL, NULL) );

   return SCIP_OKAY;
}

/** creates a logarithmic expression */
SCIP_RETCODE SCIPcreateExprLog(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR**           expr,               /**< pointer where to store expression */
   SCIP_EXPR*            child,              /**< single child */
   SCIP_DECL_EXPR_OWNERCREATE((*ownercreate)), /**< function to call to create ownerdata */
   void*                 ownercreatedata     /**< data to pass to ownercreate */
   )
{
   assert(expr != NULL);
   assert(child != NULL);

   SCIP_CALL( SCIPcreateExpr(scip, expr, SCIPfindExprHdlr(scip, EXPRHDLR_NAME), NULL, 1, &child, ownercreate,
         ownercreatedata) );

   return SCIP_OKAY;
}

/** indicates whether expression is of log-type */
SCIP_Bool SCIPisExprLog(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_EXPR*            expr                /**< expression */
   )
{
   assert(expr != NULL);

   return strcmp(SCIPexprhdlrGetName(SCIPexprGetHdlr(expr)), EXPRHDLR_NAME) == 0;
}
