/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2012 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   cons_integral.c
 * @brief  constraint handler for the integrality constraint
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>
#include <limits.h>
#ifdef WITH_EXACTSOLVE
#include "gmp.h"
#endif

#include "scip/cons_integral.h"
#include "scip/cons_exactlp.h"


#define CONSHDLR_NAME          "integral"
#define CONSHDLR_DESC          "integrality constraint"
#define CONSHDLR_SEPAPRIORITY         0 /**< priority of the constraint handler for separation */
#define CONSHDLR_ENFOPRIORITY         0 /**< priority of the constraint handler for constraint enforcing */
#define CONSHDLR_CHECKPRIORITY        0 /**< priority of the constraint handler for checking feasibility */
#define CONSHDLR_SEPAFREQ            -1 /**< frequency for separating cuts; zero means to separate only in the root node */
#define CONSHDLR_PROPFREQ            -1 /**< frequency for propagating domains; zero means only preprocessing propagation */
#define CONSHDLR_EAGERFREQ           -1 /**< frequency for using all instead of only the useful constraints in separation,
                                         *   propagation and enforcement, -1 for no eager evaluations, 0 for first only */
#define CONSHDLR_MAXPREROUNDS        -1 /**< maximal number of presolving rounds the constraint handler participates in (-1: no limit) */
#define CONSHDLR_DELAYSEPA        FALSE /**< should separation method be delayed, if other separators found cuts? */
#define CONSHDLR_DELAYPROP        FALSE /**< should propagation method be delayed, if other propagators found reductions? */
#define CONSHDLR_DELAYPRESOL      FALSE /**< should presolving method be delayed, if other presolvers found reductions? */
#define CONSHDLR_NEEDSCONS        FALSE /**< should the constraint handler be skipped, if no constraints are available? */

#define CONSHDLR_PROP_TIMING             SCIP_PROPTIMING_BEFORELP


/*
 * Callback methods
 */

/** copy method for constraint handler plugins (called when SCIP copies plugins) */
static
SCIP_DECL_CONSHDLRCOPY(conshdlrCopyIntegral)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);

   /* call inclusion method of constraint handler */
   SCIP_CALL( SCIPincludeConshdlrIntegral(scip) );
 
   *valid = TRUE;

   return SCIP_OKAY;
}


/** destructor of constraint handler to free constraint handler data (called when SCIP is exiting) */
#define consFreeIntegral NULL


/** initialization method of constraint handler (called after problem was transformed) */
#define consInitIntegral NULL


/** deinitialization method of constraint handler (called before transformed problem is freed) */
#define consExitIntegral NULL


/** presolving initialization method of constraint handler (called when presolving is about to begin) */
#define consInitpreIntegral NULL


/** presolving deinitialization method of constraint handler (called after presolving has been finished) */
#define consExitpreIntegral NULL


/** solving process initialization method of constraint handler (called when branch and bound process is about to begin) */
#define consInitsolIntegral NULL


/** solving process deinitialization method of constraint handler (called before branch and bound process data is freed) */
#define consExitsolIntegral NULL


/** frees specific constraint data */
#define consDeleteIntegral NULL


/** transforms constraint data into data belonging to the transformed problem */ 
#define consTransIntegral NULL


/** LP initialization method of constraint handler (called before the initial LP relaxation at a node is solved) */
#define consInitlpIntegral NULL


/** separation method of constraint handler for LP solutions */
#define consSepalpIntegral NULL


/** separation method of constraint handler for arbitrary primal solutions */
#define consSepasolIntegral NULL


/** constraint enforcing method of constraint handler for LP solutions */
static
SCIP_DECL_CONSENFOLP(consEnfolpIntegral)
{  /*lint --e{715}*/
   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(scip != NULL);
   assert(conss == NULL);
   assert(nconss == 0);
   assert(result != NULL);

   SCIPdebugMessage("Enfolp method of integrality constraint: %d fractional variables\n", SCIPgetNLPBranchCands(scip));

#ifdef WITH_EXACTSOLVE
   /* in exactip mode with pure rational approach, the branching is based on the exact LP solution 
    * (computed in enfolp method of exactlp constraint handler) and unbounded root LPs are also handled
    * in exactlp constraint handler  
    */
   assert(SCIPisExactSolve(scip));
   if( SCIPselectDualBoundMethod(scip, FALSE) == 'e' )
   {
      *result = SCIP_FEASIBLE;
      return SCIP_OKAY;
   }
#endif

   /* if the root LP is unbounded, we want to terminate with UNBOUNDED or INFORUNBOUNDED,
    * depending on whether we are able to construct an integral solution; in any case we do not want to branch
    */
   if( SCIPgetLPSolstat(scip) == SCIP_LPSOLSTAT_UNBOUNDEDRAY )
   {
      if( SCIPgetNLPBranchCands(scip) == 0 )
         *result = SCIP_FEASIBLE;
      else
         *result = SCIP_INFEASIBLE;
      return SCIP_OKAY;
   }

   /* call branching methods */
   SCIP_CALL( SCIPbranchLP(scip, result) );

   /* if no branching was done, the LP solution was not fractional */
   if( *result == SCIP_DIDNOTRUN )
      *result = SCIP_FEASIBLE;

   return SCIP_OKAY;
}


/** constraint enforcing method of constraint handler for pseudo solutions */
#define consEnfopsIntegral NULL


/** feasibility check method of constraint handler for integral solutions */
static
SCIP_DECL_CONSCHECK(consCheckIntegral)
{  /*lint --e{715}*/
   SCIP_VAR** vars;
   SCIP_Real solval;
   int nbin;
   int nint;
   int v;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(scip != NULL);

   SCIPdebugMessage("Check method of integrality constraint (checkintegrality=%u)\n", checkintegrality);

   SCIP_CALL( SCIPgetSolVarsData(scip, sol, &vars, NULL, &nbin, &nint, NULL, NULL) );

   *result = SCIP_FEASIBLE;

   if( checkintegrality )
   {
      int ninteger;

      ninteger = nbin + nint;

      for( v = 0; v < ninteger; ++v )
      {
         solval = SCIPgetSolVal(scip, sol, vars[v]);

#ifdef WITH_EXACTSOLVE
         {
            mpq_t solvalexact;
         
            assert(SCIPisExactSolve(scip));

            mpq_init(solvalexact);

            /**@todo exiptodo: presolving extension 
             * - this only works if presolving is disabled (solval may already be an approximation since 
             *   solution values of aggregated variables are calculated in floating point arithmetic in SCIPgetSolVal()) 
             */ 
            mpq_set_d(solvalexact, solval);
            if( mpz_get_si(mpq_denref(solvalexact)) != 1 ) 
            {
               *result = SCIP_INFEASIBLE;

               if( printreason )
               {
                  char s[SCIP_MAXSTRLEN];

                  gmp_snprintf(s, SCIP_MAXSTRLEN, "violation: integrality condition of variable <%s> = %Qd\n", 
                     SCIPvarGetName(vars[v]), solvalexact);
                  SCIPinfoMessage(scip, NULL, s);
               }
            }

            mpq_clear(solvalexact);
         }
#else
         assert(!SCIPisExactSolve(scip));

         if( !SCIPisFeasIntegral(scip, solval) )
         {
            *result = SCIP_INFEASIBLE;

            if( printreason )
            {
               SCIPinfoMessage(scip, NULL, "violation: integrality condition of variable <%s> = %.15g\n", 
                  SCIPvarGetName(vars[v]), solval);
            }
            break;
         }
#endif
      }
   }
#ifndef NDEBUG
   else
   {
      for( v = 0; v < nbin + nint; ++v )
      {
         solval = SCIPgetSolVal(scip, sol, vars[v]);
         if( !SCIPisExactSolve(scip) )
            assert(SCIPisFeasIntegral(scip, solval));
         else
         {
#ifdef WITH_EXACTSOLVE
            mpq_t solvalexact;
            
            mpq_init(solvalexact);
            mpq_set_d(solvalexact, solval);

            assert(mpz_get_si(mpq_denref(solvalexact)) == 1); 

            mpq_clear(solvalexact);
#endif
         }
      }
   }
#endif

   return SCIP_OKAY;
}


/** domain propagation method of constraint handler */
#define consPropIntegral NULL


/** presolving method of constraint handler */
#define consPresolIntegral NULL


/** propagation conflict resolving method of constraint handler */
#define consRespropIntegral NULL


/** variable rounding lock method of constraint handler */
static
SCIP_DECL_CONSLOCK(consLockIntegral)
{  /*lint --e{715}*/
   return SCIP_OKAY;
}


/** constraint activation notification method of constraint handler */
#define consActiveIntegral NULL


/** constraint deactivation notification method of constraint handler */
#define consDeactiveIntegral NULL


/** constraint enabling notification method of constraint handler */
#define consEnableIntegral NULL


/** constraint disabling notification method of constraint handler */
#define consDisableIntegral NULL


/** variable deletion method of constraint handler */
#define consDelvarsIntegral NULL


/** constraint display method of constraint handler */
#define consPrintIntegral NULL

/** constraint copying method of constraint handler */
#define consCopyIntegral NULL

/** constraint parsing method of constraint handler */
#define consParseIntegral NULL

/** constraint method of constraint handler which returns the variables (if possible) */
#define consGetVarsIntegral NULL

/** constraint method of constraint handler which returns the number of variables (if possible) */
#define consGetNVarsIntegral NULL


/*
 * constraint specific interface methods
 */

/** creates the handler for integrality constraint and includes it in SCIP */
SCIP_RETCODE SCIPincludeConshdlrIntegral(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_CONSHDLRDATA* conshdlrdata;
   SCIP_CONSHDLR* conshdlr;

   /* create integral constraint handler data */
   conshdlrdata = NULL;

   /* include constraint handler */
   SCIP_CALL( SCIPincludeConshdlrBasic(scip, &conshdlr, CONSHDLR_NAME, CONSHDLR_DESC,
         CONSHDLR_SEPAPRIORITY, CONSHDLR_ENFOPRIORITY, CONSHDLR_CHECKPRIORITY,
         CONSHDLR_EAGERFREQ, CONSHDLR_MAXPREROUNDS,
         CONSHDLR_DELAYSEPA, CONSHDLR_DELAYPROP, CONSHDLR_DELAYPRESOL, CONSHDLR_NEEDSCONS,
         CONSHDLR_PROP_TIMING,
         consEnfolpIntegral, consEnfopsIntegral, consCheckIntegral, consLockIntegral,
         conshdlrdata) );

   assert(conshdlr != NULL);

   /* set non-fundamental callbacks via specific setter functions */
   SCIP_CALL( SCIPsetConshdlrCopy(scip, conshdlr, conshdlrCopyIntegral, consCopyIntegral) );

   return SCIP_OKAY;
}
