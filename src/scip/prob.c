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
#pragma ident "@(#) $Id: prob.c,v 1.77 2005/08/17 14:25:30 bzfpfend Exp $"

/**@file   prob.c
 * @brief  Methods and datastructures for storing and manipulating the main problem
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "scip/def.h"
#include "scip/message.h"
#include "scip/set.h"
#include "scip/stat.h"
#include "scip/misc.h"
#include "scip/event.h"
#include "scip/lp.h"
#include "scip/var.h"
#include "scip/prob.h"
#include "scip/tree.h"
#include "scip/branch.h"
#include "scip/cons.h"




/*
 * dymanic memory arrays
 */

/** resizes vars array to be able to store at least num entries */
static
RETCODE probEnsureVarsMem(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   int              num                 /**< minimal number of slots in array */
   )
{
   assert(prob != NULL);
   assert(set != NULL);

   if( num > prob->varssize )
   {
      int newsize;

      newsize = SCIPsetCalcMemGrowSize(set, num);
      ALLOC_OKAY( reallocMemoryArray(&prob->vars, newsize) );
      prob->varssize = newsize;
   }
   assert(num <= prob->varssize);

   return SCIP_OKAY;
}

/** resizes fixedvars array to be able to store at least num entries */
static
RETCODE probEnsureFixedvarsMem(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   int              num                 /**< minimal number of slots in array */
   )
{
   assert(prob != NULL);
   assert(set != NULL);

   if( num > prob->fixedvarssize )
   {
      int newsize;

      newsize = SCIPsetCalcMemGrowSize(set, num);
      ALLOC_OKAY( reallocMemoryArray(&prob->fixedvars, newsize) );
      prob->fixedvarssize = newsize;
   }
   assert(num <= prob->fixedvarssize);

   return SCIP_OKAY;
}

/** resizes deletedvars array to be able to store at least num entries */
static
RETCODE probEnsureDeletedvarsMem(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   int              num                 /**< minimal number of slots in array */
   )
{
   assert(prob != NULL);
   assert(set != NULL);

   if( num > prob->deletedvarssize )
   {
      int newsize;

      newsize = SCIPsetCalcMemGrowSize(set, num);
      ALLOC_OKAY( reallocMemoryArray(&prob->deletedvars, newsize) );
      prob->deletedvarssize = newsize;
   }
   assert(num <= prob->deletedvarssize);

   return SCIP_OKAY;
}

/** resizes conss array to be able to store at least num entries */
static
RETCODE probEnsureConssMem(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   int              num                 /**< minimal number of slots in array */
   )
{
   assert(prob != NULL);
   assert(set != NULL);

   if( num > prob->consssize )
   {
      int newsize;

      newsize = SCIPsetCalcMemGrowSize(set, num);
      ALLOC_OKAY( reallocMemoryArray(&prob->conss, newsize) );
      prob->consssize = newsize;
   }
   assert(num <= prob->consssize);

   return SCIP_OKAY;
}



/*
 * problem creation
 */

/** creates problem data structure
 *  If the problem type requires the use of variable pricers, these pricers should be activated with calls
 *  to SCIPactivatePricer(). These pricers are automatically deactivated, when the problem is freed.
 */
RETCODE SCIPprobCreate(
   PROB**           prob,               /**< pointer to problem data structure */
   BLKMEM*          blkmem,             /**< block memory */
   const char*      name,               /**< problem name */
   DECL_PROBDELORIG ((*probdelorig)),   /**< frees user data of original problem */
   DECL_PROBTRANS   ((*probtrans)),     /**< creates user data of transformed problem by transforming original user data */
   DECL_PROBDELTRANS((*probdeltrans)),  /**< frees user data of transformed problem */
   DECL_PROBINITSOL ((*probinitsol)),   /**< solving process initialization method of transformed data */
   DECL_PROBEXITSOL ((*probexitsol)),   /**< solving process deinitialization method of transformed data */
   PROBDATA*        probdata,           /**< user problem data set by the reader */
   Bool             transformed         /**< is this the transformed problem? */
   )
{
   assert(prob != NULL);

   ALLOC_OKAY( allocMemory(prob) );
   ALLOC_OKAY( duplicateMemoryArray(&(*prob)->name, name, strlen(name)+1) );

   (*prob)->probdata = probdata;
   (*prob)->probdelorig = probdelorig;
   (*prob)->probtrans = probtrans;
   (*prob)->probdeltrans = probdeltrans;
   (*prob)->probinitsol = probinitsol;
   (*prob)->probexitsol = probexitsol;
   CHECK_OKAY( SCIPhashtableCreate(&(*prob)->varnames, blkmem, SCIP_HASHSIZE_NAMES,
                  SCIPhashGetKeyVar, SCIPhashKeyEqString, SCIPhashKeyValString) );
   (*prob)->vars = NULL;
   (*prob)->varssize = 0;
   (*prob)->nvars = 0;
   (*prob)->nbinvars = 0;
   (*prob)->nintvars = 0;
   (*prob)->nimplvars = 0;
   (*prob)->ncontvars = 0;
   (*prob)->ncolvars = 0;
   (*prob)->fixedvars = NULL;
   (*prob)->fixedvarssize = 0;
   (*prob)->nfixedvars = 0;
   (*prob)->deletedvars = NULL;
   (*prob)->deletedvarssize = 0;
   (*prob)->ndeletedvars = 0;
   CHECK_OKAY( SCIPhashtableCreate(&(*prob)->consnames, blkmem, SCIP_HASHSIZE_NAMES,
                  SCIPhashGetKeyCons, SCIPhashKeyEqString, SCIPhashKeyValString) );
   (*prob)->conss = NULL;
   (*prob)->consssize = 0;
   (*prob)->nconss = 0;
   (*prob)->maxnconss = 0;
   (*prob)->startnvars = 0;
   (*prob)->startnconss = 0;
   (*prob)->objsense = SCIP_OBJSENSE_MINIMIZE;
   (*prob)->objoffset = 0.0;
   (*prob)->objlim = SCIP_INVALID;
   (*prob)->objisintegral = FALSE;
   (*prob)->transformed = transformed;

   return SCIP_OKAY;
}

/** frees problem data structure */
RETCODE SCIPprobFree(
   PROB**           prob,               /**< pointer to problem data structure */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< dynamic problem statistics */
   LP*              lp                  /**< current LP data (or NULL, if it's the original problem) */
   )
{
   int v;

   assert(prob != NULL);
   assert(*prob != NULL);
   assert(set != NULL);
   
   /* remove all constraints from the problem */
   while( (*prob)->nconss > 0 )
   {
      assert((*prob)->conss != NULL);
      CHECK_OKAY( SCIPprobDelCons(*prob, blkmem, set, stat, (*prob)->conss[0]) );
   }

   if( (*prob)->transformed )
   {
      int h;

      /* unlock variables for all constraint handlers that don't need constraints */
      for( h = 0; h < set->nconshdlrs; ++h )
      {
         if( !SCIPconshdlrNeedsCons(set->conshdlrs[h]) )
         {
            CHECK_OKAY( SCIPconshdlrUnlockVars(set->conshdlrs[h], set) );
         }
      }
   }

   /* free constraint array */
   freeMemoryArrayNull(&(*prob)->conss);
   
   /* release problem variables */
   for( v = 0; v < (*prob)->nvars; ++v )
   {
      assert(SCIPvarGetProbindex((*prob)->vars[v]) >= 0);
      SCIPvarSetProbindex((*prob)->vars[v], -1);
      CHECK_OKAY( SCIPvarRelease(&(*prob)->vars[v], blkmem, set, lp) );
   }
   freeMemoryArrayNull(&(*prob)->vars);

   /* release fixed problem variables */
   for( v = 0; v < (*prob)->nfixedvars; ++v )
   {
      assert(SCIPvarGetProbindex((*prob)->fixedvars[v]) == -1);
      CHECK_OKAY( SCIPvarRelease(&(*prob)->fixedvars[v], blkmem, set, lp) );
   }
   freeMemoryArrayNull(&(*prob)->fixedvars);

   /* free deleted problem variables array */
   freeMemoryArrayNull(&(*prob)->deletedvars);

   /* free user problem data */
   if( (*prob)->transformed )
   {
      if( (*prob)->probdeltrans != NULL )
      {
         CHECK_OKAY( (*prob)->probdeltrans(set->scip, &(*prob)->probdata) );
      }
   }
   else
   {
      if( (*prob)->probdelorig != NULL )
      {
         CHECK_OKAY( (*prob)->probdelorig(set->scip, &(*prob)->probdata) );
      }
   }

   /* free hash tables for names */
   SCIPhashtableFree(&(*prob)->varnames);
   SCIPhashtableFree(&(*prob)->consnames);

   freeMemoryArray(&(*prob)->name);
   freeMemory(prob);
   
   return SCIP_OKAY;
}

/** transform problem data into normalized form */
RETCODE SCIPprobTransform(
   PROB*            source,             /**< problem to transform */
   BLKMEM*          blkmem,             /**< block memory buffer */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< problem statistics */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTFILTER*     eventfilter,        /**< event filter for global (not variable dependent) events */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   PROB**           target              /**< pointer to target problem data structure */
   )
{
   VAR* targetvar;
   CONS* targetcons;
   char transname[MAXSTRLEN];
   int v;
   int c;
   int h;

   assert(set != NULL);
   assert(source != NULL);
   assert(blkmem != NULL);
   assert(target != NULL);

   debugMessage("transform problem: original has %d variables\n", source->nvars);

   /* create target problem data (probdelorig and probtrans are not needed, probdata is set later) */
   sprintf(transname, "t_%s", source->name);
   CHECK_OKAY( SCIPprobCreate(target, blkmem, transname, NULL, NULL, source->probdeltrans, 
                  source->probinitsol, source->probexitsol, NULL, TRUE) );
   SCIPprobSetObjsense(*target, source->objsense);

   /* transform objective limit */
   if( source->objlim < SCIP_INVALID )
      SCIPprobSetObjlim(*target, source->objlim);

   /* transform and copy all variables to target problem */
   CHECK_OKAY( probEnsureVarsMem(*target, set, source->nvars) );
   for( v = 0; v < source->nvars; ++v )
   {
      CHECK_OKAY( SCIPvarTransform(source->vars[v], blkmem, set, stat, source->objsense, &targetvar) );
      CHECK_OKAY( SCIPprobAddVar(*target, blkmem, set, lp, branchcand, eventfilter, eventqueue, targetvar) );
      CHECK_OKAY( SCIPvarRelease(&targetvar, blkmem, set, NULL) );
   }
   assert((*target)->nvars == source->nvars);

   /* call user data transformation */
   if( source->probtrans != NULL )
   {
      CHECK_OKAY( source->probtrans(set->scip, source->probdata, &(*target)->probdata) );
   }
   else
      (*target)->probdata = source->probdata;

   /* transform and copy all constraints to target problem */
   for( c = 0; c < source->nconss; ++c )
   {
      CHECK_OKAY( SCIPconsTransform(source->conss[c], blkmem, set, &targetcons) );
      CHECK_OKAY( SCIPprobAddCons(*target, set, stat, targetcons) );
      CHECK_OKAY( SCIPconsRelease(&targetcons, blkmem, set) );
   }

   /* lock variables for all constraint handlers that don't need constraints */
   for( h = 0; h < set->nconshdlrs; ++h )
   {
      if( !SCIPconshdlrNeedsCons(set->conshdlrs[h]) )
      {
         CHECK_OKAY( SCIPconshdlrLockVars(set->conshdlrs[h], set) );
      }
   }

   /* objective value is always integral, iff original objective value is always integral and shift is integral */
   (*target)->objisintegral = source->objisintegral && SCIPsetIsIntegral(set, (*target)->objoffset);

   return SCIP_OKAY;
}

/** resets the global and local bounds of original variables in original problem to their original values */
RETCODE SCIPprobResetBounds(
   PROB*            prob,               /**< original problem data */
   BLKMEM*          blkmem,             /**< block memory */
   SET*             set                 /**< global SCIP settings */
   )
{
   int v;

   assert(prob != NULL);
   assert(!prob->transformed);
   assert(prob->nfixedvars == 0);

   for( v = 0; v < prob->nvars; ++v )
   {
      CHECK_OKAY( SCIPvarResetBounds(prob->vars[v], blkmem, set) );
   }

   return SCIP_OKAY;
}




/*
 * problem modification
 */

/** sets user problem data */
void SCIPprobSetData(
   PROB*            prob,               /**< problem */
   PROBDATA*        probdata            /**< user problem data to use */
   )
{
   assert(prob != NULL);

   prob->probdata = probdata;
}

/** inserts variable at the correct position in vars array, depending on its type */
static
void probInsertVar(
   PROB*            prob,               /**< problem data */
   VAR*             var                 /**< variable to insert */
   )
{
   int insertpos;
   int intstart;
   int implstart;
   int contstart;

   assert(prob != NULL);
   assert(prob->vars != NULL);
   assert(prob->nvars < prob->varssize);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) == -1);
   assert(SCIPvarGetStatus(var) == SCIP_VARSTATUS_ORIGINAL
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_LOOSE
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN);

   /* insert variable in array */
   insertpos = prob->nvars;
   intstart = prob->nbinvars;
   implstart = intstart + prob->nintvars;
   contstart = implstart + prob->nimplvars;

   if( SCIPvarGetType(var) == SCIP_VARTYPE_CONTINUOUS )
      prob->ncontvars++;
   else
   {
      if( insertpos > contstart )
      {
         prob->vars[insertpos] = prob->vars[contstart];
         SCIPvarSetProbindex(prob->vars[insertpos], insertpos);
         insertpos = contstart;
      }
      assert(insertpos == contstart);

      if( SCIPvarGetType(var) == SCIP_VARTYPE_IMPLINT )
         prob->nimplvars++;
      else
      {
         if( insertpos > implstart )
         {
            prob->vars[insertpos] = prob->vars[implstart];
            SCIPvarSetProbindex(prob->vars[insertpos], insertpos);
            insertpos = implstart;
         }
         assert(insertpos == implstart);

         if( SCIPvarGetType(var) == SCIP_VARTYPE_INTEGER )
            prob->nintvars++;
         else
         {
            assert(SCIPvarGetType(var) == SCIP_VARTYPE_BINARY);
            if( insertpos > intstart )
            {
               prob->vars[insertpos] = prob->vars[intstart];
               SCIPvarSetProbindex(prob->vars[insertpos], insertpos);
               insertpos = intstart;
            }
            assert(insertpos == intstart);

            prob->nbinvars++;
         }
      }
   }
   prob->nvars++;

   assert(prob->nvars == prob->nbinvars + prob->nintvars + prob->nimplvars + prob->ncontvars);
   assert((SCIPvarGetType(var) == SCIP_VARTYPE_BINARY && insertpos == prob->nbinvars - 1)
      || (SCIPvarGetType(var) == SCIP_VARTYPE_INTEGER && insertpos == prob->nbinvars + prob->nintvars - 1)
      || (SCIPvarGetType(var) == SCIP_VARTYPE_IMPLINT && insertpos == prob->nbinvars + prob->nintvars + prob->nimplvars - 1)
      || (SCIPvarGetType(var) == SCIP_VARTYPE_CONTINUOUS
         && insertpos == prob->nbinvars + prob->nintvars + prob->nimplvars + prob->ncontvars - 1));

   prob->vars[insertpos] = var;
   SCIPvarSetProbindex(var, insertpos);

   /* update number of column variables in problem */
   if( SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN )
      prob->ncolvars++;
   assert(0 <= prob->ncolvars && prob->ncolvars <= prob->nvars);
}

/** removes variable from vars array */
static
void probRemoveVar(
   PROB*            prob,               /**< problem data */
   VAR*             var                 /**< variable to remove */
   )
{
   int freepos;
   int intstart;
   int implstart;
   int contstart;

   assert(prob != NULL);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) >= 0);
   assert(prob->vars != NULL);
   assert(prob->vars[SCIPvarGetProbindex(var)] == var);

   intstart = prob->nbinvars;
   implstart = intstart + prob->nintvars;
   contstart = implstart + prob->nimplvars;

   switch( SCIPvarGetType(var) )
   {
   case SCIP_VARTYPE_BINARY:
      assert(0 <= SCIPvarGetProbindex(var) && SCIPvarGetProbindex(var) < intstart);
      prob->nbinvars--;
      break;
   case SCIP_VARTYPE_INTEGER:
      assert(intstart <= SCIPvarGetProbindex(var) && SCIPvarGetProbindex(var) < implstart);
      prob->nintvars--;
      break;
   case SCIP_VARTYPE_IMPLINT:
      assert(implstart <= SCIPvarGetProbindex(var) && SCIPvarGetProbindex(var) < contstart);
      prob->nimplvars--;
      break;
   case SCIP_VARTYPE_CONTINUOUS:
      assert(contstart <= SCIPvarGetProbindex(var) && SCIPvarGetProbindex(var) < prob->nvars);
      prob->ncontvars--;
      break;
   default:
      errorMessage("unknown variable type\n");
      SCIPABORT();
   }

   /* move last binary, last integer, last implicit, and last continuous variable forward to fill the free slot */
   freepos = SCIPvarGetProbindex(var);
   if( freepos < intstart-1 )
   {
      /* move last binary variable to free slot */
      prob->vars[freepos] = prob->vars[intstart-1];
      SCIPvarSetProbindex(prob->vars[freepos], freepos);
      freepos = intstart-1;
   }
   if( freepos < implstart-1 )
   {
      /* move last integer variable to free slot */
      prob->vars[freepos] = prob->vars[implstart-1];
      SCIPvarSetProbindex(prob->vars[freepos], freepos);
      freepos = implstart-1;
   }
   if( freepos < contstart-1 )
   {
      /* move last implicit integer variable to free slot */
      prob->vars[freepos] = prob->vars[contstart-1];
      SCIPvarSetProbindex(prob->vars[freepos], freepos);
      freepos = contstart-1;
   }
   if( freepos < prob->nvars-1 )
   {
      /* move last implicit integer variable to free slot */
      prob->vars[freepos] = prob->vars[prob->nvars-1];
      SCIPvarSetProbindex(prob->vars[freepos], freepos);
      freepos = prob->nvars-1;
   }
   assert(freepos == prob->nvars-1);

   prob->nvars--;
   SCIPvarSetProbindex(var, -1);

   assert(prob->nvars == prob->nbinvars + prob->nintvars + prob->nimplvars + prob->ncontvars);

   /* update number of column variables in problem */
   if( SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN )
      prob->ncolvars--;
   assert(0 <= prob->ncolvars && prob->ncolvars <= prob->nvars);
}

/** adds variable to the problem and captures it */
RETCODE SCIPprobAddVar(
   PROB*            prob,               /**< problem data */
   BLKMEM*          blkmem,             /**< block memory buffers */
   SET*             set,                /**< global SCIP settings */
   LP*              lp,                 /**< current LP data */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   EVENTFILTER*     eventfilter,        /**< event filter for global (not variable dependent) events */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   VAR*             var                 /**< variable to add */
   )
{
   assert(prob != NULL);
   assert(set != NULL);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) == -1);
   assert(SCIPvarGetStatus(var) == SCIP_VARSTATUS_ORIGINAL
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_LOOSE
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN);

   /* capture variable */
   SCIPvarCapture(var);

   /* allocate additional memory */
   CHECK_OKAY( probEnsureVarsMem(prob, set, prob->nvars+1) );
   
   /* insert variable in vars array and mark it to be in problem */
   probInsertVar(prob, var);

   /* add variable's name to the namespace */
   CHECK_OKAY( SCIPhashtableInsert(prob->varnames, (void*)var) );

   /* update branching candidates and pseudo and loose objective value in the LP */
   if( SCIPvarGetStatus(var) != SCIP_VARSTATUS_ORIGINAL )
   {
      CHECK_OKAY( SCIPbranchcandUpdateVar(branchcand, set, var) );
      CHECK_OKAY( SCIPlpUpdateAddVar(lp, set, var) );
   }

   debugMessage("added variable <%s> to problem (%d variables: %d binary, %d integer, %d implicit, %d continuous)\n",
      SCIPvarGetName(var), prob->nvars, prob->nbinvars, prob->nintvars, prob->nimplvars, prob->ncontvars);

   if( prob->transformed )
   {
      EVENT* event;

      /* issue VARADDED event */
      CHECK_OKAY( SCIPeventCreateVarAdded(&event, blkmem, var) );
      CHECK_OKAY( SCIPeventqueueAdd(eventqueue, blkmem, set, NULL, NULL, NULL, eventfilter, &event) );
   }

   return SCIP_OKAY;
}

/** marks variable to be removed from the problem; however, the variable is NOT removed from the constraints */
RETCODE SCIPprobDelVar(
   PROB*            prob,               /**< problem data */
   BLKMEM*          blkmem,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   EVENTFILTER*     eventfilter,        /**< event filter for global (not variable dependent) events */
   EVENTQUEUE*      eventqueue,         /**< event queue */
   VAR*             var                 /**< problem variable */
   )
{
   assert(prob != NULL);
   assert(set != NULL);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) != -1);
   assert(SCIPvarGetStatus(var) == SCIP_VARSTATUS_ORIGINAL
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_LOOSE
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN);

   debugMessage("deleting variable <%s> from problem (%d variables: %d binary, %d integer, %d implicit, %d continuous)\n",
      SCIPvarGetName(var), prob->nvars, prob->nbinvars, prob->nintvars, prob->nimplvars, prob->ncontvars);

   /* mark variable to be deleted from the problem */
   SCIPvarMarkDeleted(var);

   if( prob->transformed )
   {
      EVENT* event;

      /* issue VARDELETED event */
      CHECK_OKAY( SCIPeventCreateVarDeleted(&event, blkmem, var) );
      CHECK_OKAY( SCIPeventqueueAdd(eventqueue, blkmem, set, NULL, NULL, NULL, eventfilter, &event) );
   }

   /* remember that the variable should be deleted from the problem in SCIPprobPerformVarDeletions() */
   CHECK_OKAY( probEnsureDeletedvarsMem(prob, set, prob->ndeletedvars+1) );
   prob->deletedvars[prob->ndeletedvars] = var;
   prob->ndeletedvars++;

   return SCIP_OKAY;
}

/** actually removes the deleted variables from the problem and releases them */
RETCODE SCIPprobPerformVarDeletions(
   PROB*            prob,               /**< problem data */
   BLKMEM*          blkmem,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   LP*              lp,                 /**< current LP data (may be NULL) */
   BRANCHCAND*      branchcand          /**< branching candidate storage */
   )
{
   int i;

   assert(prob != NULL);

   for( i = 0; i < prob->ndeletedvars; ++i )
   {
      VAR* var;

      var = prob->deletedvars[i];

      /* don't delete the variable, if it was fixed or aggregated in the meantime */
      if( SCIPvarGetProbindex(var) == -1 )
      {
         debugMessage("perform deletion of <%s> [%p]\n", SCIPvarGetName(var), var);
         
         /* convert column variable back into loose variable, free LP column */
         if( SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN )
         {
            CHECK_OKAY( SCIPvarLoose(var, blkmem, set, prob, lp) );
         }
         
         /* update branching candidates and pseudo and loose objective value in the LP */
         if( SCIPvarGetStatus(var) != SCIP_VARSTATUS_ORIGINAL )
         {
            CHECK_OKAY( SCIPlpUpdateDelVar(lp, set, var) );
            CHECK_OKAY( SCIPbranchcandRemoveVar(branchcand, var) );
         }
         
         /* remove variable's name from the namespace */
         assert(SCIPhashtableExists(prob->varnames, (void*)var));
         CHECK_OKAY( SCIPhashtableRemove(prob->varnames, (void*)var) );

         /* remove variable from vars array and mark it to be not in problem */
         probRemoveVar(prob, var);

         /* release variable */
         CHECK_OKAY( SCIPvarRelease(&prob->deletedvars[i], blkmem, set, lp) );
      }
   }
   prob->ndeletedvars = 0;

   return SCIP_OKAY;
}

/** changes the type of a variable in the problem */
RETCODE SCIPprobChgVarType(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   VAR*             var,                /**< variable to add */
   VARTYPE          vartype             /**< new type of variable */
   )
{
   assert(prob != NULL);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) >= 0);
   assert(SCIPvarGetStatus(var) == SCIP_VARSTATUS_ORIGINAL
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_LOOSE
      || SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN);

   if( SCIPvarGetType(var) == vartype )
      return SCIP_OKAY;

   /* temporarily remove variable from problem */
   probRemoveVar(prob, var);

   /* change the type of the variable */
   CHECK_OKAY( SCIPvarChgType(var, vartype) );

   /* reinsert variable into problem */
   probInsertVar(prob, var);

   /* update branching candidates */
   assert(branchcand != NULL || SCIPvarGetStatus(var) == SCIP_VARSTATUS_ORIGINAL);
   if( branchcand != NULL )
   {
      CHECK_OKAY( SCIPbranchcandUpdateVar(branchcand, set, var) );
   }

   return SCIP_OKAY;
}

/** informs problem, that the given loose problem variable changed its status */
RETCODE SCIPprobVarChangedStatus(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   BRANCHCAND*      branchcand,         /**< branching candidate storage */
   VAR*             var                 /**< problem variable */
   )
{
   assert(prob != NULL);
   assert(var != NULL);
   assert(SCIPvarGetProbindex(var) != -1);

   /* get current status of variable */
   switch( SCIPvarGetStatus(var) )
   {
   case SCIP_VARSTATUS_ORIGINAL:
      errorMessage("variables cannot switch to ORIGINAL status\n");
      return SCIP_INVALIDDATA;

   case SCIP_VARSTATUS_LOOSE:
      /* variable switched from column to loose */
      prob->ncolvars--;
      break;

   case SCIP_VARSTATUS_COLUMN:
      /* variable switched from non-column to column */
      prob->ncolvars++;
      break;

   case SCIP_VARSTATUS_FIXED:
   case SCIP_VARSTATUS_AGGREGATED:
   case SCIP_VARSTATUS_MULTAGGR:
   case SCIP_VARSTATUS_NEGATED:
      /* variable switched from unfixed to fixed (if it was fixed before, probindex would have been -1) */

      /* remove variable from problem */
      probRemoveVar(prob, var);
      
      /* insert variable in fixedvars array */
      CHECK_OKAY( probEnsureFixedvarsMem(prob, set, prob->nfixedvars+1) );
      prob->fixedvars[prob->nfixedvars] = var;
      prob->nfixedvars++;

      /* update branching candidates */
      CHECK_OKAY( SCIPbranchcandUpdateVar(branchcand, set, var) );
      break;

   default:
      errorMessage("invalid variable status <%d>\n", SCIPvarGetStatus(var));
      return SCIP_INVALIDDATA;
   }
   assert(0 <= prob->ncolvars && prob->ncolvars <= prob->nvars);

   return SCIP_OKAY;
}

/** adds constraint to the problem and captures it;
 *  a local constraint is automatically upgraded into a global constraint
 */
RETCODE SCIPprobAddCons(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< dynamic problem statistics */
   CONS*            cons                /**< constraint to add */
   )
{
   assert(prob != NULL);
   assert(cons != NULL);
   assert(cons->addconssetchg == NULL);
   assert(cons->addarraypos == -1);

   /* mark the constraint as problem constraint, and remember the constraint's position */
   cons->addconssetchg = NULL;
   cons->addarraypos = prob->nconss;

   /* add the constraint to the problem's constraint array */
   CHECK_OKAY( probEnsureConssMem(prob, set, prob->nconss+1) );
   prob->conss[prob->nconss] = cons;
   prob->nconss++;
   prob->maxnconss = MAX(prob->maxnconss, prob->nconss);

   /* undelete constraint, if it was globally deleted in the past */
   cons->deleted = FALSE;

   /* mark constraint to be globally valid */
   cons->local = FALSE;

   /* capture constraint */
   SCIPconsCapture(cons);

   /* add constraint's name to the namespace */
   CHECK_OKAY( SCIPhashtableInsert(prob->consnames, (void*)cons) );

   /* if the problem is the transformed problem, activate and lock constraint */
   if( prob->transformed )
   {
      /* activate constraint */
      CHECK_OKAY( SCIPconsActivate(cons, set, stat, -1) );

      /* if constraint is a check-constraint, lock roundings of constraint's variables */
      if( SCIPconsIsChecked(cons) )
      {
         CHECK_OKAY( SCIPconsAddLocks(cons, set, +1, 0) );
      }
   }

   return SCIP_OKAY;
}

/** releases and removes constraint from the problem; if the user has not captured the constraint for his own use, the
 *  constraint may be invalid after the call
 */
RETCODE SCIPprobDelCons(
   PROB*            prob,               /**< problem data */
   BLKMEM*          blkmem,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< dynamic problem statistics */
   CONS*            cons                /**< constraint to remove */
   )
{
   int arraypos;

   assert(prob != NULL);
   assert(blkmem != NULL);
   assert(cons != NULL);
   assert(cons->addconssetchg == NULL);
   assert(0 <= cons->addarraypos && cons->addarraypos < prob->nconss);
   assert(prob->conss != NULL);
   assert(prob->conss[cons->addarraypos] == cons);

   /* if the problem is the transformed problem, deactivate and unlock constraint */
   if( prob->transformed )
   {
      /* if constraint is a check-constraint, unlock roundings of constraint's variables */
      if( SCIPconsIsChecked(cons) )
      {
         CHECK_OKAY( SCIPconsAddLocks(cons, set, -1, 0) );
      }

      /* deactivate constraint, if it is currently active */
      if( cons->active && !cons->updatedeactivate )
      {
         CHECK_OKAY( SCIPconsDeactivate(cons, set, stat) );
      }
   }
   assert(!cons->active || cons->updatedeactivate);
   assert(!cons->enabled || cons->updatedeactivate);

   /* remove constraint's name from the namespace */
   assert(SCIPhashtableExists(prob->consnames, (void*)cons));
   CHECK_OKAY( SCIPhashtableRemove(prob->consnames, (void*)cons) );

   /* remove the constraint from the problem's constraint array */
   arraypos = cons->addarraypos;
   prob->conss[arraypos] = prob->conss[prob->nconss-1];
   assert(prob->conss[arraypos] != NULL);
   assert(prob->conss[arraypos]->addconssetchg == NULL);
   prob->conss[arraypos]->addarraypos = arraypos;
   prob->nconss--;

   /* mark the constraint to be no longer in the problem */
   cons->addarraypos = -1;

   /* release constraint */
   CHECK_OKAY( SCIPconsRelease(&cons, blkmem, set) );

   return SCIP_OKAY;
}

/** remembers the current number of constraints in the problem's internal data structure
 *  - resets maximum number of constraints to current number of constraints
 *  - remembers current number of constraints as starting number of constraints
 */
void SCIPprobMarkNConss(
   PROB*            prob                /**< problem data */
   )
{
   assert(prob != NULL);

   /* remember number of constraints for statistic */
   prob->maxnconss = prob->nconss;
   prob->startnvars = prob->nvars;
   prob->startnconss = prob->nconss;
}

/** sets objective sense: minimization or maximization */
void SCIPprobSetObjsense(
   PROB*            prob,               /**< problem data */
   OBJSENSE         objsense            /**< new objective sense */
   )
{
   assert(prob != NULL);
   assert(prob->objsense == SCIP_OBJSENSE_MAXIMIZE || prob->objsense == SCIP_OBJSENSE_MINIMIZE);
   assert(objsense == SCIP_OBJSENSE_MAXIMIZE || objsense == SCIP_OBJSENSE_MINIMIZE);

   prob->objsense = objsense;
}

/** adds value to objective offset */
void SCIPprobAddObjoffset(
   PROB*            prob,               /**< problem data */
   Real             addval              /**< value to add to objective offset */
   )
{
   assert(prob != NULL);
   assert(prob->transformed);

   debugMessage("adding %g to objective offset %g: new offset = %g\n", addval, prob->objoffset, prob->objoffset + addval);
   prob->objoffset += addval;
}

/** sets limit on objective function, such that only solutions better than this limit are accepted */
void SCIPprobSetObjlim(
   PROB*            prob,               /**< problem data */
   Real             objlim              /**< external objective limit */
   )
{
   assert(prob != NULL);

   prob->objlim = objlim;
}

/** informs the problem, that its objective value is always integral in every feasible solution */
void SCIPprobSetObjIntegral(
   PROB*            prob                /**< problem data */
   )
{
   assert(prob != NULL);
   
   prob->objisintegral = TRUE;
}

/** sets integral objective value flag, if all variables with non-zero objective values are integral and have 
 *  integral objective value
 */
void SCIPprobCheckObjIntegral(
   PROB*            prob,               /**< problem data */
   SET*             set                 /**< global SCIP settings */
   )
{
   Real obj;
   int v;

   assert(prob != NULL);
   
   /* if we know already, that the objective value is integral, nothing has to be done */
   if( prob->objisintegral )
      return;

   /* if there exist unknown variables, we cannot conclude that the objective value is always integral */
   if( set->nactivepricers != 0 )
      return;

   /* if the objective value offset is fractional, the value itself is possibly fractional */
   if( !SCIPsetIsIntegral(set, prob->objoffset) )
      return;

   /* scan through the variables */
   for( v = 0; v < prob->nvars; ++v )
   {
      /* get objective value of variable */
      obj = SCIPvarGetObj(prob->vars[v]);

      /* check, if objective value is non-zero */
      if( !SCIPsetIsZero(set, obj) )
      {
         /* if variable's objective value is fractional, the problem's objective value may also be fractional */
         if( !SCIPsetIsIntegral(set, obj) )
            break;
         
         /* if variable with non-zero objective value is continuous, the problem's objective value may be fractional */
         if( SCIPvarGetType(prob->vars[v]) == SCIP_VARTYPE_CONTINUOUS )
            break;
      }
   }

   /* objective value is integral, if the variable loop scanned all variables */
   prob->objisintegral = (v == prob->nvars);
}

/** remembers the current solution as root solution in the problem variables */
void SCIPprobStoreRootSol(
   PROB*            prob,               /**< problem data */
   Bool             roothaslp           /**< is the root solution from LP? */
   )
{
   int v;

   assert(prob != NULL);
   assert(prob->transformed);

   for( v = 0; v < prob->nvars; ++v )
      SCIPvarStoreRootSol(prob->vars[v], roothaslp);
}

/** informs problem, that the presolving process was finished, and updates all internal data structures */
RETCODE SCIPprobExitPresolve(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat                /**< problem statistics */
   )
{
   /* check, wheter objective value is always integral */
   SCIPprobCheckObjIntegral(prob, set);

   /* reset implication counter */
   SCIPstatResetImplications(stat);

   return SCIP_OKAY;
}

/** initializes problem for branch and bound process and resets all constraint's ages and histories of current run */
RETCODE SCIPprobInitSolve(
   PROB*            prob,               /**< problem data */
   SET*             set                 /**< global SCIP settings */
   )
{
   int c;
   int v;

   assert(prob != NULL);
   assert(prob->transformed);
   assert(set != NULL);

   /* reset constraint's ages */
   for( c = 0; c < prob->nconss; ++c )
   {
      CHECK_OKAY( SCIPconsResetAge(prob->conss[c], set) );
   }

   /* initialize variables for solving */
   for( v = 0; v < prob->nvars; ++v )
      SCIPvarInitSolve(prob->vars[v]);

   /* call user data function */
   if( prob->probinitsol != NULL )
   {
      CHECK_OKAY( prob->probinitsol(set->scip, prob->probdata) );
   }

   return SCIP_OKAY;
}

/** deinitializes problem after branch and bound process, and converts all COLUMN variables back into LOOSE variables */
RETCODE SCIPprobExitSolve(
   PROB*            prob,               /**< problem data */
   BLKMEM*          blkmem,             /**< block memory */
   SET*             set,                /**< global SCIP settings */
   LP*              lp                  /**< current LP data */
   )
{
   VAR* var;
   int v;

   assert(prob != NULL);
   assert(prob->transformed);
   assert(set != NULL);

   /* call user data function */
   if( prob->probexitsol != NULL )
   {
      CHECK_OKAY( prob->probexitsol(set->scip, prob->probdata) );
   }

   /* convert all COLUMN variables back into LOOSE variables */
   if( prob->ncolvars > 0 )
   {
      for( v = 0; v < prob->nvars; ++v )
      {
         var = prob->vars[v];
         if( SCIPvarGetStatus(var) == SCIP_VARSTATUS_COLUMN )
         {
            CHECK_OKAY( SCIPvarLoose(var, blkmem, set, prob, lp) );
         }
      }
   }
   assert(prob->ncolvars == 0);

   return SCIP_OKAY;
}




/*
 * problem information
 */

/** gets problem name */
const char* SCIPprobGetName(
   PROB*            prob                /**< problem data */
   )
{
   assert(prob != NULL);
   return prob->name;
}

/** gets user problem data */
PROBDATA* SCIPprobGetData(
   PROB*            prob                /**< problem */
   )
{
   assert(prob != NULL);

   return prob->probdata;
}

/** returns the external value of the given internal objective value */
Real SCIPprobExternObjval(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   Real             objval              /**< internal objective value */
   )
{
   assert(prob != NULL);
   assert(prob->transformed);

   if( SCIPsetIsInfinity(set, objval) )
      return (Real)prob->objsense * SCIPsetInfinity(set);
   else if( SCIPsetIsInfinity(set, -objval) )
      return -(Real)prob->objsense * SCIPsetInfinity(set);
   else
      return (Real)prob->objsense * (objval + prob->objoffset);
}

/** returns the internal value of the given external objective value */
Real SCIPprobInternObjval(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   Real             objval              /**< external objective value */
   )
{
   assert(prob != NULL);
   assert(prob->transformed);

   if( SCIPsetIsInfinity(set, objval) )
      return (Real)prob->objsense * SCIPsetInfinity(set);
   else if( SCIPsetIsInfinity(set, -objval) )
      return -(Real)prob->objsense * SCIPsetInfinity(set);
   else
      return (Real)prob->objsense * objval - prob->objoffset;
}

/** gets limit on objective function in external space */
Real SCIPprobGetObjlim(
   PROB*            prob,               /**< problem data */
   SET*             set                 /**< global SCIP settings */
   )
{
   assert(prob != NULL);
   assert(set != NULL);

   return prob->objlim >= SCIP_INVALID ? (Real)(prob->objsense) * SCIPsetInfinity(set) : prob->objlim;
}

/** returns whether the objective value is known to be integral in every feasible solution */
Bool SCIPprobIsObjIntegral(
   PROB*            prob                /**< problem data */
   )
{
   assert(prob != NULL);

   return prob->objisintegral;
}

/** returns variable of the problem with given name */
VAR* SCIPprobFindVar(
   PROB*            prob,               /**< problem data */
   const char*      name                /**< name of variable to find */
   )
{
   assert(prob != NULL);
   assert(name != NULL);

   return (VAR*)(SCIPhashtableRetrieve(prob->varnames, (void*)name));
}

/** returns constraint of the problem with given name */
CONS* SCIPprobFindCons(
   PROB*            prob,               /**< problem data */
   const char*      name                /**< name of variable to find */
   )
{
   assert(prob != NULL);
   assert(name != NULL);

   return (CONS*)(SCIPhashtableRetrieve(prob->consnames, (void*)name));
}

/** returns TRUE iff all columns, i.e. every variable with non-empty column w.r.t. all ever created rows, are present
 *  in the LP, and FALSE, if there are additional already existing columns, that may be added to the LP in pricing
 */
Bool SCIPprobAllColsInLP(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   LP*              lp                  /**< current LP data */
   )
{
   assert(SCIPlpGetNCols(lp) <= prob->ncolvars && prob->ncolvars <= prob->nvars);

   return (SCIPlpGetNCols(lp) == prob->ncolvars && set->nactivepricers == 0);
}

/** displays current pseudo solution */
void SCIPprobPrintPseudoSol(
   PROB*            prob,               /**< problem data */
   SET*             set                 /**< global SCIP settings */
   )
{
   VAR* var;
   Real solval;
   int v;
   
   for( v = 0; v < prob->nvars; ++v )
   {
      var = prob->vars[v];
      assert(var != NULL);
      solval = SCIPvarGetPseudoSol(var);
      if( !SCIPsetIsZero(set, solval) )
         SCIPmessagePrintInfo(" <%s>=%g", SCIPvarGetName(var), solval);
   }
   SCIPmessagePrintInfo("\n");
}

/** outputs problem statistics */
void SCIPprobPrintStatistics(
   PROB*            prob,               /**< problem data */
   FILE*            file                /**< output file (or NULL for standard output) */
   )
{
   assert(prob != NULL);

   SCIPmessageFPrintInfo(file, "  Problem name     : %s\n", prob->name);
   SCIPmessageFPrintInfo(file, "  Variables        : %d (%d binary, %d integer, %d implicit integer, %d continuous)\n",
      prob->nvars, prob->nbinvars, prob->nintvars, prob->nimplvars, prob->ncontvars);
   SCIPmessageFPrintInfo(file, "  Constraints      : %d initial, %d maximal\n", prob->startnconss, prob->maxnconss);
}

/** outputs problem to file stream */
RETCODE SCIPprobPrint(
   PROB*            prob,               /**< problem data */
   SET*             set,                /**< global SCIP settings */
   FILE*            file                /**< output file (or NULL for standard output) */
   )
{
   int i;

   assert(prob != NULL);

   SCIPmessageFPrintInfo(file, "STATISTICS\n");
   SCIPprobPrintStatistics(prob, file);

   if( prob->nvars > 0 )
   {
      SCIPmessageFPrintInfo(file, "VARIABLES\n");
      for( i = 0; i < prob->nvars; ++i )
         SCIPvarPrint(prob->vars[i], set, file);
   }

   if( prob->nfixedvars > 0 )
   {
      SCIPmessageFPrintInfo(file, "FIXED\n");
      for( i = 0; i < prob->nfixedvars; ++i )
         SCIPvarPrint(prob->fixedvars[i], set, file);
   }

   if( prob->nconss > 0 )
   {
      SCIPmessageFPrintInfo(file, "CONSTRAINTS\n");
      for( i = 0; i < prob->nconss; ++i )
      {
         CHECK_OKAY( SCIPconsPrint(prob->conss[i], set, file) );
      }
   }

   SCIPmessageFPrintInfo(file, "END\n");

   return SCIP_OKAY;
}
