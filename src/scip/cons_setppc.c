/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2003 Tobias Achterberg                              */
/*                            Thorsten Koch                                  */
/*                  2002-2003 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   cons_setppc.c
 * @brief  constraint handler for the set partitioning / packing / covering constraints
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>
#include <limits.h>

#include "cons_setppc.h"
#include "cons_linear.h"


#define CONSHDLR_NAME          "setppc"
#define CONSHDLR_DESC          "set partitioning / packing / covering constraints"
#define CONSHDLR_SEPAPRIORITY   +700000
#define CONSHDLR_ENFOPRIORITY   +700000
#define CONSHDLR_CHECKPRIORITY  -700000
#define CONSHDLR_SEPAFREQ             4
#define CONSHDLR_PROPFREQ            -1
#define CONSHDLR_NEEDSCONS         TRUE

#define EVENTHDLR_NAME         "setppc"
#define EVENTHDLR_DESC         "bound change event handler for set partitioning / packing / covering constraints"

#define LINCONSUPGD_PRIORITY    +700000

#define DEFAULT_NPSEUDOBRANCHES       2  /**< number of children created in pseudo branching */
#define MINBRANCHWEIGHT             0.3  /**< minimum weight of both sets in binary set branching */
#define MAXBRANCHWEIGHT             0.7  /**< maximum weight of both sets in binary set branching */

enum SetppcType
{
   SCIP_SETPPCTYPE_PARTITIONING = 0,     /**< constraint is a set partitioning constraint: a*x == 1 */
   SCIP_SETPPCTYPE_PACKING      = 1,     /**< constraint is a set packing constraint:      a*x <= 1 */
   SCIP_SETPPCTYPE_COVERING     = 2      /**< constraint is a set covering constraint:     a*x >= 1 */
};
typedef enum SetppcType SETPPCTYPE;

/** constraint handler data */
struct ConsHdlrData
{
   INTARRAY*        varuses;            /**< number of times a var is used in the active set ppc constraints */
   int              npseudobranches;    /**< number of children created in pseudo branching */
};

/** set partitioning / packing / covering constraint data */
struct SetppcCons
{
   VAR**            vars;               /**< variables of the constraint */
   int              varssize;           /**< size of vars array */
   int              nvars;              /**< number of variables in the constraint */
   int              nfixedzeros;        /**< actual number of variables fixed to zero in the constraint */
   int              nfixedones;         /**< actual number of variables fixed to one in the constraint */
   unsigned int     setppctype:2;       /**< type of constraint: set partitioning, packing or covering */
   unsigned int     local:1;            /**< is constraint only valid locally? */
   unsigned int     modifiable:1;       /**< is data modifiable during node processing (subject to column generation)? */
   unsigned int     removeable:1;       /**< should the row be removed from the LP due to aging or cleanup? */
   unsigned int     transformed:1;      /**< does the constraint data belongs to the transformed problem? */
   unsigned int     changed:1;          /**< was constraint changed since last preprocess/propagate call? */
};
typedef struct SetppcCons SETPPCCONS;   /**< set partitioning / packing / covering constraint data */

/** constraint data for set partitioning constraints */
struct ConsData
{
   SETPPCCONS*      setppccons;         /**< set partitioning / packing / covering constraint data */
   ROW*             row;                /**< LP row, if constraint is already stored in LP row format */
};




/*
 * Local methods
 */

/** creates constaint handler data for set partitioning / packing / covering constraint handler */
static
RETCODE conshdlrdataCreate(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLRDATA**   conshdlrdata        /**< pointer to store the constraint handler data */
   )
{
   assert(conshdlrdata != NULL);

   CHECK_OKAY( SCIPallocMemory(scip, conshdlrdata) );
   CHECK_OKAY( SCIPcreateIntarray(scip, &(*conshdlrdata)->varuses) );
   (*conshdlrdata)->npseudobranches = DEFAULT_NPSEUDOBRANCHES;

   return SCIP_OKAY;
}

/** frees constraint handler data for set partitioning / packing / covering constraint handler */
static
RETCODE conshdlrdataFree(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLRDATA**   conshdlrdata        /**< pointer to the constraint handler data */
   )
{
   assert(conshdlrdata != NULL);
   assert(*conshdlrdata != NULL);

   CHECK_OKAY( SCIPfreeIntarray(scip, &(*conshdlrdata)->varuses) );
   SCIPfreeMemory(scip, conshdlrdata);

   return SCIP_OKAY;
}

/** increases the usage counter of the given variable */
static
RETCODE conshdlrdataIncVaruses(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLRDATA*    conshdlrdata,       /**< constraint handler data */
   VAR*             var                 /**< variable to increase usage counter for */
   )
{
   INTARRAY* varuses;

   assert(conshdlrdata != NULL);
   assert(var != NULL);

   varuses = conshdlrdata->varuses;
   assert(varuses != NULL);

   /* if the variable is the negation of a problem variable, count the varuses in the problem variable */
   if( SCIPvarIsNegated(var) )
   {
      VAR* negvar;

      CHECK_OKAY( SCIPgetNegatedVar(scip, var, &negvar) );
      var = negvar;
   }

   /* increase varuses counter */
   CHECK_OKAY( SCIPincIntarrayVal(scip, varuses, SCIPvarGetIndex(var), +1) );

   /*debugMessage("varuses of <%s>: %d\n", SCIPvarGetName(var), SCIPgetIntarrayVal(scip, varuses, SCIPvarGetIndex(var)));*/

   return SCIP_OKAY;
}

/** decreases the usage counter of the given variable */
static
RETCODE conshdlrdataDecVaruses(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLRDATA*    conshdlrdata,       /**< constraint handler data */
   VAR*             var                 /**< variable to increase usage counter for */
   )
{
   INTARRAY* varuses;

   assert(conshdlrdata != NULL);
   assert(var != NULL);

   varuses = conshdlrdata->varuses;
   assert(varuses != NULL);

   /* if the variable is the negation of a problem variable, count the varuses in the problem variable */
   if( SCIPvarIsNegated(var) )
   {
      VAR* negvar;

      CHECK_OKAY( SCIPgetNegatedVar(scip, var, &negvar) );
      var = negvar;
   }

   /* decrease varuses counter */
   CHECK_OKAY( SCIPincIntarrayVal(scip, varuses, SCIPvarGetIndex(var), -1) );
   assert(SCIPgetIntarrayVal(scip, varuses, SCIPvarGetIndex(var)) >= 0);

   /*debugMessage("varuses of <%s>: %d\n", SCIPvarGetName(var), SCIPgetIntarrayVal(scip, varuses, SCIPvarGetIndex(var)));*/

   return SCIP_OKAY;
}

/** creates event data for variable at given position, and catches events */
static
RETCODE setppcconsCatchEvent(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   EVENTHDLR*       eventhdlr,          /**< event handler to call for the event processing */
   int              pos                 /**< array position of variable to catch bound change events for */
   )
{
   VAR* var;

   assert(setppccons != NULL);
   assert(eventhdlr != NULL);
   assert(0 <= pos && pos < setppccons->nvars);
   assert(setppccons->vars != NULL);

   var = setppccons->vars[pos];
   assert(var != NULL);

   /* catch bound change events on variables */
   CHECK_OKAY( SCIPcatchVarEvent(scip, var, SCIP_EVENTTYPE_BOUNDCHANGED, eventhdlr, (EVENTDATA*)setppccons) );
   
   /* update the fixed variables counters for this variable */
   if( SCIPisEQ(scip, SCIPvarGetUbLocal(var), 0.0) )
      setppccons->nfixedzeros++;
   else if( SCIPisEQ(scip, SCIPvarGetLbLocal(var), 1.0) )
      setppccons->nfixedones++;

   return SCIP_OKAY;
}

/** deletes event data for variable at given position, and drops events */
static
RETCODE setppcconsDropEvent(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   EVENTHDLR*       eventhdlr,          /**< event handler to call for the event processing */
   int              pos                 /**< array position of variable to catch bound change events for */
   )
{
   VAR* var;

   assert(setppccons != NULL);
   assert(eventhdlr != NULL);
   assert(0 <= pos && pos < setppccons->nvars);
   assert(setppccons->vars != NULL);

   var = setppccons->vars[pos];
   assert(var != NULL);
   
   CHECK_OKAY( SCIPdropVarEvent(scip, var, eventhdlr, (EVENTDATA*)setppccons) );

   /* update the fixed variables counters for this variable */
   if( SCIPisEQ(scip, SCIPvarGetUbLocal(var), 0.0) )
      setppccons->nfixedzeros--;
   else if( SCIPisEQ(scip, SCIPvarGetLbLocal(var), 1.0) )
      setppccons->nfixedones--;

   return SCIP_OKAY;
}

/** catches bound change events and locks rounding for variable at given position in transformed set ppc constraint */
static
RETCODE setppcconsLockCoef(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   EVENTHDLR*       eventhdlr,          /**< event handler for bound change events, or NULL */
   int              pos                 /**< position of variable in set partitioning constraint */
   )
{
   VAR* var;
      
   assert(scip != NULL);
   assert(setppccons != NULL);
   assert(setppccons->transformed);
   assert(0 <= pos && pos < setppccons->nvars);

   var = setppccons->vars[pos];
   
   /*debugMessage("locking coefficient <%s> in set ppc constraint\n", SCIPvarGetName(var));*/

   if( eventhdlr == NULL )
   {
      /* get event handler for updating set partitioning / packing / covering constraint activity bounds */
      eventhdlr = SCIPfindEventHdlr(scip, EVENTHDLR_NAME);
      if( eventhdlr == NULL )
      {
         errorMessage("event handler for set partitioning / packing / covering constraints not found");
         return SCIP_PLUGINNOTFOUND;
      }
   }

   /* catch bound change events on variable */
   assert(SCIPvarIsTransformed(var));
   CHECK_OKAY( setppcconsCatchEvent(scip, setppccons, eventhdlr, pos) );

   /* forbid rounding of variable */
   if( !setppccons->local )
   {
      switch( setppccons->setppctype )
      {
      case SCIP_SETPPCTYPE_PARTITIONING:
         SCIPvarForbidRound(var);
         break;
      case SCIP_SETPPCTYPE_PACKING:
         SCIPvarForbidRoundUp(var);
         break;
      case SCIP_SETPPCTYPE_COVERING:
         SCIPvarForbidRoundDown(var);
         break;
      }
   }

   return SCIP_OKAY;
}

/** drops bound change events and unlocks rounding for variable at given position in transformed set ppc constraint */
static
RETCODE setppcconsUnlockCoef(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   EVENTHDLR*       eventhdlr,          /**< event handler for bound change events, or NULL */
   int              pos                 /**< position of variable in set partitioning constraint */
   )
{
   VAR* var;

   assert(scip != NULL);
   assert(setppccons != NULL);
   assert(setppccons->transformed);
   assert(0 <= pos && pos < setppccons->nvars);

   var = setppccons->vars[pos];

   /*debugMessage("unlocking coefficient <%s> in set ppc constraint\n", SCIPvarGetName(var));*/

   if( eventhdlr == NULL )
   {
      /* get event handler for updating set partitioning / packing / covering constraint activity bounds */
      eventhdlr = SCIPfindEventHdlr(scip, EVENTHDLR_NAME);
      if( eventhdlr == NULL )
      {
         errorMessage("event handler for set partitioning / packing / covering constraints not found");
         return SCIP_PLUGINNOTFOUND;
      }
   }
   
   /* drop bound change events on variable */
   assert(SCIPvarIsTransformed(var));
   CHECK_OKAY( setppcconsDropEvent(scip, setppccons, eventhdlr, pos) );

   /* allow rounding of variable */
   if( !setppccons->local )
   {
      switch( setppccons->setppctype )
      {
      case SCIP_SETPPCTYPE_PARTITIONING:
         SCIPvarAllowRound(var);
         break;
      case SCIP_SETPPCTYPE_PACKING:
         SCIPvarAllowRoundUp(var);
         break;
      case SCIP_SETPPCTYPE_COVERING:
         SCIPvarAllowRoundDown(var);
         break;
      default:
         errorMessage("unknown setppc type");
         return SCIP_INVALIDDATA;
      }
   }

   return SCIP_OKAY;
}

/** catches bound change events and locks rounding for all variables in transformed set ppc constraint */
static
RETCODE setppcconsLockAllCoefs(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons         /**< set partitioning / packing / covering constraint object */
   )
{
   EVENTHDLR* eventhdlr;
   int i;

   assert(scip != NULL);
   assert(setppccons != NULL);
   assert(setppccons->transformed);

   /* get event handler for updating set partitioning / packing / covering constraint activity bounds */
   eventhdlr = SCIPfindEventHdlr(scip, EVENTHDLR_NAME);
   if( eventhdlr == NULL )
   {
      errorMessage("event handler for set partitioning / packing / covering constraints not found");
      return SCIP_PLUGINNOTFOUND;
   }

   /* lock every single coefficient */
   for( i = 0; i < setppccons->nvars; ++i )
   {
      CHECK_OKAY( setppcconsLockCoef(scip, setppccons, eventhdlr, i) );
   }

   return SCIP_OKAY;
}

/** drops bound change events and unlocks rounding for all variables in transformed set ppc constraint */
static
RETCODE setppcconsUnlockAllCoefs(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons          /**< set partitioning / packing / covering constraint object */
   )
{
   EVENTHDLR* eventhdlr;
   int i;

   assert(scip != NULL);
   assert(setppccons != NULL);
   assert(setppccons->transformed);

   /* get event handler for updating set partitioning / packing / covering constraint activity bounds */
   eventhdlr = SCIPfindEventHdlr(scip, EVENTHDLR_NAME);
   if( eventhdlr == NULL )
   {
      errorMessage("event handler for set partitioning / packing / covering constraints not found");
      return SCIP_PLUGINNOTFOUND;
   }

   /* unlock every single coefficient */
   for( i = 0; i < setppccons->nvars; ++i )
   {
      CHECK_OKAY( setppcconsUnlockCoef(scip, setppccons, eventhdlr, i) );
   }

   return SCIP_OKAY;
}

/** deletes coefficient at given position from set ppc constraint object */
static
RETCODE setppcconsDelCoefPos(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   int              pos                 /**< position of coefficient to delete */
   )
{
   VAR* var;

   assert(setppccons != NULL);
   assert(0 <= pos && pos < setppccons->nvars);

   var = setppccons->vars[pos];
   assert(var != NULL);
   assert(setppccons->transformed ^ (!SCIPvarIsTransformed(var)));

   if( setppccons->transformed )
   {
      /* drop bound change events and unlock the rounding of variable */
      CHECK_OKAY( setppcconsUnlockCoef(scip, setppccons, NULL, pos) );
   }

   /* move the last variable to the free slot */
   setppccons->vars[pos] = setppccons->vars[setppccons->nvars-1];
   setppccons->nvars--;

   setppccons->changed = TRUE;

   return SCIP_OKAY;
}

/** creates a set partitioning / packing / covering constraint data object */
static
RETCODE setppcconsCreate(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS**     setppccons,         /**< pointer to store the set partitioning / packing / covering constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< variables of the constraint */
   SETPPCTYPE       setppctype,         /**< type of constraint: set partitioning, packing, or covering constraint */
   Bool             modifiable,         /**< is constraint modifiable (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   assert(setppccons != NULL);
   assert(nvars == 0 || vars != NULL);

   CHECK_OKAY( SCIPallocBlockMemory(scip, setppccons) );
   if( nvars > 0 )
   {
      VAR* var;
      int v;

      CHECK_OKAY( SCIPduplicateBlockMemoryArray(scip, &(*setppccons)->vars, vars, nvars) );
      (*setppccons)->varssize = nvars;
      (*setppccons)->nvars = nvars;
   }
   else
   {
      (*setppccons)->vars = NULL;
      (*setppccons)->varssize = 0;
      (*setppccons)->nvars = 0;
   }
   (*setppccons)->nfixedzeros = 0;
   (*setppccons)->nfixedones = 0;
   (*setppccons)->setppctype = setppctype;
   (*setppccons)->local = FALSE;
   (*setppccons)->modifiable = modifiable;
   (*setppccons)->removeable = removeable;
   (*setppccons)->transformed = FALSE;
   (*setppccons)->changed = TRUE;

   return SCIP_OKAY;
}   

/** creates a transformed set partitioning / packing / covering constraint data object */
static
RETCODE setppcconsCreateTransformed(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS**     setppccons,         /**< pointer to store the set partitioning / packing / covering constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< variables of the constraint */
   SETPPCTYPE       setppctype,         /**< type of constraint: set partitioning, packing, or covering constraint */
   Bool             local,              /**< is constraint only locally valid? */
   Bool             modifiable,         /**< is constraint modifiable (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   EVENTHDLR* eventhdlr;
   VAR* var;
   int i;

   assert(setppccons != NULL);
   assert(nvars == 0 || vars != NULL);

   CHECK_OKAY( setppcconsCreate(scip, setppccons, nvars, vars, setppctype, modifiable, removeable) );
   (*setppccons)->local = local;
   (*setppccons)->transformed = TRUE;

   /* transform the variables */
   for( i = 0; i < (*setppccons)->nvars; ++i )
   {
      var = (*setppccons)->vars[i];
      assert(var != NULL);
      assert(SCIPisLE(scip, 0.0, SCIPvarGetLbLocal(var)));
      assert(SCIPisLE(scip, SCIPvarGetLbLocal(var), SCIPvarGetUbLocal(var)));
      assert(SCIPisLE(scip, SCIPvarGetUbLocal(var), 1.0));
      assert(SCIPisIntegral(scip, SCIPvarGetLbLocal(var)));
      assert(SCIPisIntegral(scip, SCIPvarGetUbLocal(var)));

      /* use transformed variables in constraint instead original ones */
      if( !SCIPvarIsTransformed(var) )
      {
         CHECK_OKAY( SCIPgetTransformedVar(scip, var, &var) );
         assert(var != NULL);
         (*setppccons)->vars[i] = var;
      }
      assert(SCIPvarIsTransformed(var));
      assert(SCIPvarGetType(var) == SCIP_VARTYPE_BINARY);
   }

   /* catch bound change events and lock the rounding of variables */
   CHECK_OKAY( setppcconsLockAllCoefs(scip, *setppccons) );

   return SCIP_OKAY;
}

/** frees a set partitioning / packing / covering constraint data */
static
RETCODE setppcconsFree(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS**     setppccons         /**< pointer to store the set partitioning / packing / covering constraint */
   )
{
   assert(setppccons != NULL);
   assert(*setppccons != NULL);

   if( (*setppccons)->transformed )
   {
      /* drop bound change events and unlock the rounding of variables */
      CHECK_OKAY( setppcconsUnlockAllCoefs(scip, *setppccons) );
   }

   SCIPfreeBlockMemoryArrayNull(scip, &(*setppccons)->vars, (*setppccons)->varssize);
   SCIPfreeBlockMemory(scip, setppccons);

   return SCIP_OKAY;
}

/** creates an LP row from a set partitioning / packing / covering constraint data object */
static
RETCODE setppcconsToRow(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint data */
   const char*      name,               /**< name of the constraint */
   ROW**            row                 /**< pointer to an LP row data object */
   )
{
   Real lhs;
   Real rhs;
   int v;

   assert(setppccons != NULL);
   assert(row != NULL);

   switch( setppccons->setppctype )
   {
   case SCIP_SETPPCTYPE_PARTITIONING:
      lhs = 1.0;
      rhs = 1.0;
      break;
   case SCIP_SETPPCTYPE_PACKING:
      lhs = -SCIPinfinity(scip);
      rhs = 1.0;
      break;
   case SCIP_SETPPCTYPE_COVERING:
      lhs = 1.0;
      rhs = SCIPinfinity(scip);
      break;
   default:
      errorMessage("unknown setppc type");
      return SCIP_INVALIDDATA;
   }

   CHECK_OKAY( SCIPcreateRow(scip, row, name, 0, NULL, NULL, lhs, rhs,
                  setppccons->local, setppccons->modifiable, setppccons->removeable) );
   
   for( v = 0; v < setppccons->nvars; ++v )
   {
      CHECK_OKAY( SCIPaddVarToRow(scip, *row, setppccons->vars[v], 1.0) );
   }

   return SCIP_OKAY;
}

/** prints set partitioning / packing / covering constraint to file stream */
static
void setppcconsPrint(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint object */
   FILE*            file                /**< output file (or NULL for standard output) */
   )
{
   int v;

   assert(setppccons != NULL);

   if( file == NULL )
      file = stdout;

   /* print coefficients */
   if( setppccons->nvars == 0 )
      fprintf(file, "0 ");
   for( v = 0; v < setppccons->nvars; ++v )
   {
      assert(setppccons->vars[v] != NULL);
      fprintf(file, "+%s ", SCIPvarGetName(setppccons->vars[v]));
   }

   /* print right hand side */
   switch( setppccons->setppctype )
   {
   case SCIP_SETPPCTYPE_PARTITIONING:
      fprintf(file, "= 1\n");
      break;
   case SCIP_SETPPCTYPE_PACKING:
      fprintf(file, ">= 1\n");
      break;
   case SCIP_SETPPCTYPE_COVERING:
      fprintf(file, "<= 1\n");
      break;
   default:
      errorMessage("unknown setppc type");
      abort();
   }
}

/** checks constraint for violation only looking at the fixed variables, applies further fixings if possible */
static
RETCODE processFixings(
   SCIP*            scip,               /**< SCIP data structure */
   CONS*            cons,               /**< set partitioning / packing / covering constraint to be processed */
   Bool*            cutoff,             /**< pointer to store TRUE, if the node can be cut off */
   Bool*            reduceddom,         /**< pointer to store TRUE, if a domain reduction was found */
   Bool*            addcut,             /**< pointer to store whether this constraint must be added as a cut */
   Bool*            mustcheck           /**< pointer to store whether this constraint must be checked for feasibility */
   )
{
   CONSDATA* consdata;
   SETPPCCONS* setppccons;

   assert(cons != NULL);
   assert(SCIPconsGetHdlr(cons) != NULL);
   assert(strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), CONSHDLR_NAME) == 0);
   assert(cutoff != NULL);
   assert(reduceddom != NULL);
   assert(addcut != NULL);
   assert(mustcheck != NULL);

   consdata = SCIPconsGetData(cons);
   assert(consdata != NULL);
   setppccons = consdata->setppccons;
   assert(setppccons != NULL);
   assert(setppccons->nvars == 0 || setppccons->vars != NULL);
   assert(0 <= setppccons->nfixedzeros && setppccons->nfixedzeros <= setppccons->nvars);
   assert(0 <= setppccons->nfixedones && setppccons->nfixedones <= setppccons->nvars);

   *addcut = FALSE;
   *mustcheck = FALSE;

   if( setppccons->nfixedones >= 2 )
   {
      /* at least two variables are fixed to 1:
       * - a set covering constraint is feasible anyway and can be disabled
       * - a set partitioning or packing constraint is infeasible
       */
      if( setppccons->setppctype == SCIP_SETPPCTYPE_COVERING )
      {
         CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
      }
      else
      {
         CHECK_OKAY( SCIPresetConsAge(scip, cons) );
         *cutoff = TRUE;
      }
   }
   else if( setppccons->nfixedones == 1 )
   {
      /* exactly one variable is fixed to 1:
       * - a set covering constraint is feasible anyway and can be disabled
       * - all other variables in a set partitioning or packing constraint must be zero
       */
      if( setppccons->setppctype == SCIP_SETPPCTYPE_COVERING )
      {
         CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
      }
      else
      {
         if( setppccons->nfixedzeros < setppccons->nvars - 1 )
         {
            VAR** vars;
            VAR* var;
            Bool fixedonefound;
            Bool fixed;
            int nvars;
            int v;

            /* unfixed variables exist: fix them to zero */
            vars = setppccons->vars;
            nvars = setppccons->nvars;
            fixedonefound = FALSE;
            fixed = FALSE;
            for( v = 0; v < nvars; ++v )
            {
               var = vars[v];
               assert(!fixedonefound || SCIPisZero(scip, SCIPvarGetLbLocal(var)));
               assert(SCIPisZero(scip, SCIPvarGetUbLocal(var)) || SCIPisEQ(scip, SCIPvarGetUbLocal(var), 1.0));
               if( SCIPvarGetLbLocal(var) < 0.5 )
               {
                  if( SCIPvarGetUbLocal(var) > 0.5 )
                  {
                     CHECK_OKAY( SCIPchgVarUb(scip, var, 0.0) );
                     fixed = TRUE;
                  }
               }
               else
                  fixedonefound = TRUE;
            }
            /* the fixed to one variable must have been found, and at least one variable must have been fixed */
            assert(fixedonefound && fixed);

            CHECK_OKAY( SCIPresetConsAge(scip, cons) );
            *reduceddom = TRUE;
         }

         /* now all other variables are fixed to zero:
          * the constraint is feasible, and if it's not modifiable, it is redundant
          */
         if( !setppccons->modifiable )
         {
            CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
         }
      }
   }
   else if( setppccons->nfixedzeros == setppccons->nvars )
   {
      /* all variables are fixed to zero:
       * - a set packing constraint is feasible anyway, and if it's unmodifiable, it can be disabled
       * - a set partitioning or covering constraint is infeasible, and if it's unmodifiable, the node
       *   can be cut off -- otherwise, the constraint must be added as a cut and further pricing must
       *   be performed
       */
      assert(setppccons->nfixedones == 0);
      
      if( setppccons->setppctype == SCIP_SETPPCTYPE_PACKING )
      {
         if( !setppccons->modifiable )
         {
            CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
         }
      }
      else
      {
         CHECK_OKAY( SCIPresetConsAge(scip, cons) );
         if( setppccons->modifiable )
            *addcut = TRUE;
         else
            *cutoff = TRUE;
      }
   }
   else if( setppccons->nfixedzeros == setppccons->nvars - 1 )
   {
      /* all variables except one are fixed to zero:
       * - a set packing constraint is feasible anyway, and if it's unmodifiable, it can be disabled
       * - an unmodifiable set partitioning or covering constraint is feasible and can be disabled after the
       *   remaining variable is fixed to one
       * - a modifiable set partitioning or covering constraint must be checked manually
       */
      assert(setppccons->nfixedones == 0);
      
      if( setppccons->setppctype == SCIP_SETPPCTYPE_PACKING )
      {
         if( !setppccons->modifiable )
         {
            CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
         }
      }
      else if( !setppccons->modifiable )
      {
         VAR** vars;
         VAR* var;
         Bool fixed;
         int nvars;
         int v;
         
         /* search the single variable that can be fixed */
         vars = setppccons->vars;
         nvars = setppccons->nvars;
         fixed = FALSE;
         for( v = 0; v < nvars && !fixed; ++v )
         {
            var = vars[v];
            assert(SCIPisZero(scip, SCIPvarGetLbLocal(var)));
            assert(SCIPisZero(scip, SCIPvarGetUbLocal(var)) || SCIPisEQ(scip, SCIPvarGetUbLocal(var), 1.0));
            if( SCIPvarGetUbLocal(var) > 0.5 )
            {
               CHECK_OKAY( SCIPchgVarLb(scip, var, 1.0) );
               fixed = TRUE;
            }
         }
         assert(fixed);
         
         CHECK_OKAY( SCIPdisableConsLocal(scip, cons) );
         *reduceddom = TRUE;
      }
      else
         *mustcheck = TRUE;
   }
   else
   {
      /* no variable is fixed to one, and at least two variables are not fixed to zero:
       * - the constraint must be checked manually
       */
      assert(setppccons->nfixedones == 0);
      assert(setppccons->nfixedzeros < setppccons->nvars - 1);

      *mustcheck = TRUE;
   }

   return SCIP_OKAY;
}

/** checks constraint for violation, returns TRUE iff constraint is feasible */
static
Bool check(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons,         /**< set partitioning / packing / covering constraint to be checked */
   SOL*             sol                 /**< primal CIP solution */
   )
{
   VAR** vars;
   Real solval;
   Real sum;
   int nvars;
   int v;
   
   /* calculate the constraint's activity */
   vars = setppccons->vars;
   nvars = setppccons->nvars;
   sum = 0.0;
   assert(SCIPfeastol(scip) < 0.1); /* to make the comparison against 1.1 working */
   for( v = 0; v < nvars && sum < 1.1; ++v )  /* if sum >= 1.1, the feasibility is clearly decided */
   {
      assert(SCIPvarGetType(vars[v]) == SCIP_VARTYPE_BINARY);
      solval = SCIPgetSolVal(scip, sol, vars[v]);
      assert(SCIPisFeasGE(scip, solval, 0.0) && SCIPisFeasLE(scip, solval, 1.0));
      sum += solval;
   }

   switch( setppccons->setppctype )
   {
   case SCIP_SETPPCTYPE_PARTITIONING:
      return SCIPisFeasEQ(scip, sum, 1.0);
   case SCIP_SETPPCTYPE_PACKING:
      return SCIPisFeasLE(scip, sum, 1.0);
   case SCIP_SETPPCTYPE_COVERING:
      return SCIPisFeasGE(scip, sum, 1.0);
   default:
      errorMessage("unknown setppc type");
      abort();
   }
}

/** checks constraint for violation, and adds it as a cut if possible */
static
RETCODE separate(
   SCIP*            scip,               /**< SCIP data structure */
   CONS*            cons,               /**< set partitioning / packing / covering constraint to be separated */
   Bool*            cutoff,             /**< pointer to store TRUE, if the node can be cut off */
   Bool*            separated,          /**< pointer to store TRUE, if a cut was found */
   Bool*            reduceddom          /**< pointer to store TRUE, if a domain reduction was found */
   )
{
   CONSDATA* consdata;
   SETPPCCONS* setppccons;
   Bool addcut;
   Bool mustcheck;

   assert(cons != NULL);
   assert(SCIPconsGetHdlr(cons) != NULL);
   assert(strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), CONSHDLR_NAME) == 0);
   assert(cutoff != NULL);
   assert(separated != NULL);
   assert(reduceddom != NULL);

   consdata = SCIPconsGetData(cons);
   assert(consdata != NULL);
   setppccons = consdata->setppccons;
   assert(setppccons != NULL);
   assert(setppccons->nvars == 0 || setppccons->vars != NULL);
   assert(0 <= setppccons->nfixedzeros && setppccons->nfixedzeros <= setppccons->nvars);
   assert(0 <= setppccons->nfixedones && setppccons->nfixedones <= setppccons->nvars);

   /* skip constraints already in the LP */
   if( consdata->row != NULL && SCIProwIsInLP(consdata->row) )
      return SCIP_OKAY;

   /* check constraint for violation only looking at the fixed variables, apply further fixings if possible */
   CHECK_OKAY( processFixings(scip, cons, cutoff, reduceddom, &addcut, &mustcheck) );

   if( mustcheck )
   {
      assert(!addcut);

      /* variable's fixings didn't give us any information -> we have to check the constraint */
      if( consdata->row != NULL )
      {
         assert(!SCIProwIsInLP(consdata->row));
         addcut = !SCIPisFeasible(scip, SCIPgetRowLPFeasibility(scip, consdata->row));
      }
      else
         addcut = !check(scip, setppccons, NULL);

      if( !addcut )
      {
         /* constraint was feasible -> increase age */
         CHECK_OKAY( SCIPincConsAge(scip, cons) );
      }
   }

   if( addcut )
   {
      if( consdata->row == NULL )
      {
         /* convert set partitioning constraint data into LP row */
         CHECK_OKAY( setppcconsToRow(scip, setppccons, SCIPconsGetName(cons), &consdata->row) );
      }
      assert(consdata->row != NULL);
      assert(!SCIProwIsInLP(consdata->row));
            
      /* insert LP row as cut */
      CHECK_OKAY( SCIPaddCut(scip, consdata->row, 1.0/(setppccons->nvars+1)) );
      CHECK_OKAY( SCIPresetConsAge(scip, cons) );
      *separated = TRUE;
   }

   return SCIP_OKAY;
}

/** enforces the pseudo solution on the given constraint */
static
RETCODE enforcePseudo(
   SCIP*            scip,               /**< SCIP data structure */
   CONS*            cons,               /**< set partitioning / packing / covering constraint to be separated */
   Bool*            cutoff,             /**< pointer to store TRUE, if the node can be cut off */
   Bool*            infeasible,         /**< pointer to store TRUE, if the constraint was infeasible */
   Bool*            reduceddom,         /**< pointer to store TRUE, if a domain reduction was found */
   Bool*            solvelp             /**< pointer to store TRUE, if the LP has to be solved */
   )
{
   Bool addcut;
   Bool mustcheck;

   assert(!SCIPhasActnodeLP(scip));
   assert(cons != NULL);
   assert(SCIPconsGetHdlr(cons) != NULL);
   assert(strcmp(SCIPconshdlrGetName(SCIPconsGetHdlr(cons)), CONSHDLR_NAME) == 0);
   assert(cutoff != NULL);
   assert(infeasible != NULL);
   assert(reduceddom != NULL);
   assert(solvelp != NULL);

   /* check constraint for violation only looking at the fixed variables, apply further fixings if possible */
   CHECK_OKAY( processFixings(scip, cons, cutoff, reduceddom, &addcut, &mustcheck) );

   if( mustcheck )
   {
      CONSDATA* consdata;
      SETPPCCONS* setppccons;

      assert(!addcut);

      consdata = SCIPconsGetData(cons);
      assert(consdata != NULL);
      setppccons = consdata->setppccons;
      assert(setppccons != NULL);

      if( check(scip, setppccons, NULL) )
      {
         /* constraint was feasible -> increase age */
         CHECK_OKAY( SCIPincConsAge(scip, cons) );
      }
      else
      {
         /* constraint was infeasible -> reset age */
         CHECK_OKAY( SCIPresetConsAge(scip, cons) );
         *infeasible = TRUE;
      }
   }

   if( addcut )
   {
      /* a cut must be added to the LP -> we have to solve the LP immediately */
      CHECK_OKAY( SCIPresetConsAge(scip, cons) );
      *solvelp = TRUE;
   }

   return SCIP_OKAY;
}




/*
 * Callback methods of constraint handler
 */

static
DECL_CONSFREE(consFreeSetppc)
{
   CONSHDLRDATA* conshdlrdata;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(scip != NULL);

   /* free constraint handler data */
   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   CHECK_OKAY( conshdlrdataFree(scip, &conshdlrdata) );

   SCIPconshdlrSetData(conshdlr, NULL);

   return SCIP_OKAY;
}

static
DECL_CONSDELETE(consDeleteSetppc)
{
   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(consdata != NULL);
   assert(*consdata != NULL);

   /* free LP row and setppc constraint */
   if( (*consdata)->row != NULL )
   {
      CHECK_OKAY( SCIPreleaseRow(scip, &(*consdata)->row) );
   }
   CHECK_OKAY( setppcconsFree(scip, &(*consdata)->setppccons) );

   /* free constraint data object */
   SCIPfreeBlockMemory(scip, consdata);

   return SCIP_OKAY;
}

static
DECL_CONSTRANS(consTransSetppc)
{
   CONSDATA* sourcedata;
   CONSDATA* targetdata;
   SETPPCCONS* setppccons;

   /*debugMessage("Trans method of setppc constraints\n");*/

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(SCIPstage(scip) == SCIP_STAGE_INITSOLVE);
   assert(sourcecons != NULL);
   assert(targetcons != NULL);

   sourcedata = SCIPconsGetData(sourcecons);
   assert(sourcedata != NULL);
   assert(sourcedata->row == NULL);  /* in original problem, there cannot be LP rows */
   assert(sourcedata->setppccons != NULL);

   /* create constraint data for target constraint */
   CHECK_OKAY( SCIPallocBlockMemory(scip, &targetdata) );

   setppccons = sourcedata->setppccons;

   CHECK_OKAY( setppcconsCreateTransformed(scip, &targetdata->setppccons, setppccons->nvars, setppccons->vars,
                  setppccons->setppctype, setppccons->local, setppccons->modifiable, setppccons->removeable) );
   targetdata->row = NULL;

   /* create target constraint */
   CHECK_OKAY( SCIPcreateCons(scip, targetcons, SCIPconsGetName(sourcecons), conshdlr, targetdata,
                  SCIPconsIsSeparated(sourcecons), SCIPconsIsEnforced(sourcecons), SCIPconsIsChecked(sourcecons),
                  SCIPconsIsPropagated(sourcecons)) );

   return SCIP_OKAY;
}

static
DECL_CONSSEPA(consSepaSetppc)
{
   CONSDATA* consdata;
   Bool cutoff;
   Bool separated;
   Bool reduceddom;
   int c;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(nconss == 0 || conss != NULL);
   assert(result != NULL);

   debugMessage("separating %d/%d set partitioning / packing / covering constraints\n", nusefulconss, nconss);

   *result = SCIP_DIDNOTFIND;

   cutoff = FALSE;
   separated = FALSE;
   reduceddom = FALSE;

   /* step 1: check all useful set partitioning / packing / covering constraints for feasibility */
   for( c = 0; c < nusefulconss && !cutoff && !reduceddom; ++c )
   {
      CHECK_OKAY( separate(scip, conss[c], &cutoff, &separated, &reduceddom) );
   }

   /* step 2: combine set partitioning / packing / covering constraints to get more cuts */
   todoMessage("further cuts of set partitioning / packing / covering constraints");

   /* step 3: if no cuts were found and we are in the root node, separate remaining constraints */
   if( SCIPgetActDepth(scip) == 0 )
   {
      for( c = nusefulconss; c < nconss && !cutoff && !separated && !reduceddom; ++c )
      {
         CHECK_OKAY( separate(scip, conss[c], &cutoff, &separated, &reduceddom) );
      }
   }

   /* return the correct result */
   if( cutoff )
      *result = SCIP_CUTOFF;
   else if( separated )
      *result = SCIP_SEPARATED;
   else if( reduceddom )
      *result = SCIP_REDUCEDDOM;

   return SCIP_OKAY;
}

/** if fractional variables exist, chooses a set S of them and branches on (i) x(S) == 0, and (ii) x(S) >= 1 */
static
RETCODE branchLP(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLR*        conshdlr,           /**< set partitioning / packing / covering constraint handler */
   RESULT*          result              /**< pointer to store the result SCIP_BRANCHED, if branching was applied */
   )
{
   CONSHDLRDATA* conshdlrdata;
   INTARRAY* varuses;
   VAR** lpcands;
   VAR** sortcands;
   VAR* var;
   Real branchweight;
   Real solval;
   int* uses;
   int nlpcands;
   int nsortcands;
   int nselcands;
   int actuses;
   int i;
   int j;

   todoMessage("use a better set partitioning / packing / covering branching on LP solution (use SOS branching)");

   assert(conshdlr != NULL);
   assert(result != NULL);

   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   varuses = conshdlrdata->varuses;
   assert(varuses != NULL);

   /* get fractional variables */
   CHECK_OKAY( SCIPgetLPBranchCands(scip, &lpcands, NULL, NULL, &nlpcands) );
   if( nlpcands == 0 )
      return SCIP_OKAY;

   /* get temporary memory */
   CHECK_OKAY( SCIPcaptureBufferArray(scip, &sortcands, nlpcands) );
   CHECK_OKAY( SCIPcaptureBufferArray(scip, &uses, nlpcands) );
   
   /* sort fractional variables by number of uses in enabled set partitioning / packing / covering constraints */
   nsortcands = 0;
   for( i = 0; i < nlpcands; ++i )
   {
      var = lpcands[i];
      actuses = SCIPgetIntarrayVal(scip, varuses, SCIPvarGetIndex(var));
      if( actuses > 0 )
      {
         for( j = nsortcands; j > 0 && actuses > uses[j-1]; --j )
         {
            sortcands[j] = sortcands[j-1];
            uses[j] = uses[j-1];
         }
         assert(0 <= j && j <= nsortcands);
         sortcands[j] = var;
         uses[j] = actuses;
         nsortcands++;
      }
   }
   assert(nsortcands <= nlpcands);

   /* if none of the fractional variables is member of a set partitioning / packing / covering constraint,
    * we are not responsible for doing the branching
    */
   if( nsortcands > 0 )
   {
      /* select the first variables from the sorted candidate list, until MAXBRANCHWEIGHT is reached;
       * then choose one less
       */
      branchweight = 0.0;
      for( nselcands = 0; nselcands < nsortcands && branchweight <= MAXBRANCHWEIGHT; ++nselcands )
      {
         solval = SCIPgetVarSol(scip, sortcands[nselcands]);
         assert(SCIPisFeasGE(scip, solval, 0.0) && SCIPisFeasLE(scip, solval, 1.0));
         branchweight += solval;
      }
      assert(nselcands > 0);
      nselcands--;
      branchweight -= solval;

      /* check, if we accumulated at least MIN and at most MAXBRANCHWEIGHT weight */
      if( MINBRANCHWEIGHT <= branchweight && branchweight <= MAXBRANCHWEIGHT )
      {
         NODE* node;

         /* perform the binary set branching on the selected variables */
         assert(nselcands <= nlpcands);
         
         /* create left child, fix x_i = 0 for all i \in S */
         CHECK_OKAY( SCIPcreateChild(scip, &node) );
         for( i = 0; i < nselcands; ++i )
         {
            CHECK_OKAY( SCIPchgVarUbNode(scip, node, sortcands[i], 0.0) );
         }

         /* create right child: add constraint x(S) >= 1 */
         CHECK_OKAY( SCIPcreateChild(scip, &node) );
         if( nselcands == 1 )
         {
            /* only one candidate selected: fix it to 1.0 */
            debugMessage("fixing variable <%s> to 1.0 in right child node\n", SCIPvarGetName(sortcands[0]));
            CHECK_OKAY( SCIPchgVarLbNode(scip, node, sortcands[0], 1.0) );
         }
         else
         {
            CONS* newcons;
            char name[MAXSTRLEN];
         
            /* add set covering constraint x(S) >= 1 */
            sprintf(name, "BSB%lld", SCIPgetNodenum(scip));

            CHECK_OKAY( SCIPcreateConsSetcover(scip, &newcons, name, nselcands, sortcands,
                           TRUE, TRUE, FALSE, TRUE, FALSE, TRUE) );
            CHECK_OKAY( SCIPaddConsNode(scip, node, newcons) );
            CHECK_OKAY( SCIPreleaseCons(scip, &newcons) );
         }
      
         *result = SCIP_BRANCHED;
         
#ifdef DEBUG
         debugMessage("binary set branching: nselcands=%d/%d, weight(S)=%g, A={", nselcands, nlpcands, branchweight);
         for( i = 0; i < nselcands; ++i )
            printf(" %s[%g]", SCIPvarGetName(sortcands[i]), SCIPgetSolVal(scip, NULL, sortcands[i]));
         printf(" }\n");
#endif
      }
   }

   /* free temporary memory */
   CHECK_OKAY( SCIPreleaseBufferArray(scip, &uses) );
   CHECK_OKAY( SCIPreleaseBufferArray(scip, &sortcands) );

   return SCIP_OKAY;
}

/** if unfixed variables exist, chooses a set S of them and creates |S|+1 child nodes:
 *   - for each variable i from S, create child node with x_0 = ... = x_i-1 = 0, x_i = 1
 *   - create an additional child node x_0 = ... = x_n-1 = 0
 */
static
RETCODE branchPseudo(
   SCIP*            scip,               /**< SCIP data structure */
   CONSHDLR*        conshdlr,           /**< set partitioning / packing / covering constraint handler */
   RESULT*          result              /**< pointer to store the result SCIP_BRANCHED, if branching was applied */
   )
{
   CONSHDLRDATA* conshdlrdata;
   INTARRAY* varuses;
   VAR** pseudocands;
   VAR** branchcands;
   VAR* var;
   NODE* node;
   int* canduses;
   int npseudocands;
   int maxnbranchcands;
   int nbranchcands;
   int uses;
   int i;
   int j;

   todoMessage("use a better set partitioning / packing / covering branching on pseudo solution (use SOS branching)");

   assert(conshdlr != NULL);
   assert(result != NULL);

   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   varuses = conshdlrdata->varuses;
   assert(varuses != NULL);

   /* get fractional variables */
   CHECK_OKAY( SCIPgetPseudoBranchCands(scip, &pseudocands, &npseudocands) );
   if( npseudocands == 0 )
      return SCIP_OKAY;

   /* choose the maximal number of branching variables */
   maxnbranchcands = conshdlrdata->npseudobranches-1;
   assert(maxnbranchcands >= 1);

   /* get temporary memory */
   CHECK_OKAY( SCIPcaptureBufferArray(scip, &branchcands, maxnbranchcands) );
   CHECK_OKAY( SCIPcaptureBufferArray(scip, &canduses, maxnbranchcands) );
   
   /* sort fractional variables by number of uses in enabled set partitioning / packing / covering constraints */
   nbranchcands = 0;
   for( i = 0; i < npseudocands; ++i )
   {
      var = pseudocands[i];
      uses = SCIPgetIntarrayVal(scip, varuses, SCIPvarGetIndex(var));
      if( uses > 0 )
      {
         if( nbranchcands < maxnbranchcands || uses > canduses[nbranchcands-1] )
         {
            for( j = MIN(nbranchcands, maxnbranchcands-1); j > 0 && uses > canduses[j-1]; --j )
            {
               branchcands[j] = branchcands[j-1];
               canduses[j] = canduses[j-1];
            }
            assert(0 <= j && j <= nbranchcands && j < maxnbranchcands);
            branchcands[j] = var;
            canduses[j] = uses;
            if( nbranchcands < maxnbranchcands )
               nbranchcands++;
         }
      }
   }
   assert(nbranchcands <= maxnbranchcands);

   /* if none of the unfixed variables is member of a set partitioning / packing / covering constraint,
    * we are not responsible for doing the branching
    */
   if( nbranchcands > 0 )
   {
      /* branch on the first part of the sorted candidates:
       * - for each of these variables i, create a child node x_0 = ... = x_i-1 = 0, x_i = 1
       * - create an additional child node x_0 = ... = x_n-1 = 0
       */
      for( i = 0; i < nbranchcands; ++i )
      {            
         /* create child with x_0 = ... = x_i-1 = 0, x_i = 1 */
         CHECK_OKAY( SCIPcreateChild(scip, &node) );
         for( j = 0; j < i; ++j )
         {
            CHECK_OKAY( SCIPchgVarUbNode(scip, node, branchcands[j], 0.0) );
         }
         CHECK_OKAY( SCIPchgVarLbNode(scip, node, branchcands[i], 1.0) );
      }
      /* create child with x_0 = ... = x_n = 0 */
      CHECK_OKAY( SCIPcreateChild(scip, &node) );
      for( i = 0; i < nbranchcands; ++i )
      {
         CHECK_OKAY( SCIPchgVarUbNode(scip, node, branchcands[i], 0.0) );
      }

      *result = SCIP_BRANCHED;

#ifdef DEBUG
      {
         int nchildren;
         CHECK_OKAY( SCIPgetChildren(scip, NULL, &nchildren) );
         debugMessage("branched on pseudo solution: %d children\n", nchildren);
      }
#endif
   }

   /* free temporary memory */
   CHECK_OKAY( SCIPreleaseBufferArray(scip, &canduses) );
   CHECK_OKAY( SCIPreleaseBufferArray(scip, &branchcands) );

   return SCIP_OKAY;
}



static
DECL_CONSENFOLP(consEnfolpSetppc)
{
   Bool cutoff;
   Bool separated;
   Bool reduceddom;
   int c;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(nconss == 0 || conss != NULL);
   assert(result != NULL);

   debugMessage("LP enforcing %d set partitioning / packing / covering constraints\n", nconss);

   *result = SCIP_FEASIBLE;

   cutoff = FALSE;
   separated = FALSE;
   reduceddom = FALSE;

   /* step 1: check all useful set partitioning / packing / covering constraints for feasibility */
   for( c = 0; c < nusefulconss && !cutoff && !reduceddom; ++c )
   {
      CHECK_OKAY( separate(scip, conss[c], &cutoff, &separated, &reduceddom) );
   }

   if( !cutoff && !separated && !reduceddom )
   {
      /* step 2: if solution is not integral, choose a variable set to branch on */
      CHECK_OKAY( branchLP(scip, conshdlr, result) );
      if( *result != SCIP_FEASIBLE )
         return SCIP_OKAY;
      
      /* step 3: check all obsolete set partitioning / packing / covering constraints for feasibility */
      for( c = nusefulconss; c < nconss && !cutoff && !separated && !reduceddom; ++c )
      {
         CHECK_OKAY( separate(scip, conss[c], &cutoff, &separated, &reduceddom) );
      }
   }

   /* return the correct result */
   if( cutoff )
      *result = SCIP_CUTOFF;
   else if( separated )
      *result = SCIP_SEPARATED;
   else if( reduceddom )
      *result = SCIP_REDUCEDDOM;

   return SCIP_OKAY;
}

static
DECL_CONSENFOPS(consEnfopsSetppc)
{
   Bool cutoff;
   Bool infeasible;
   Bool reduceddom;
   Bool solvelp;
   int c;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(nconss == 0 || conss != NULL);
   assert(result != NULL);

   /* if the solution is infeasible anyway due to objective value, skip the constraint processing and branch directly */
   if( objinfeasible )
   {
      *result = SCIP_DIDNOTRUN;
      CHECK_OKAY( branchPseudo(scip, conshdlr, result) );
      return SCIP_OKAY;
   }

   debugMessage("pseudo enforcing %d set partitioning / packing / covering constraints\n", nconss);

   *result = SCIP_FEASIBLE;

   cutoff = FALSE;
   infeasible = FALSE;
   reduceddom = FALSE;
   solvelp = FALSE;

   /* check all set partitioning / packing / covering constraints for feasibility */
   for( c = 0; c < nconss && !cutoff && !reduceddom && !solvelp; ++c )
   {
      CHECK_OKAY( enforcePseudo(scip, conss[c], &cutoff, &infeasible, &reduceddom, &solvelp) );
   }

   if( cutoff )
      *result = SCIP_CUTOFF;
   else if( reduceddom )
      *result = SCIP_REDUCEDDOM;
   else if( solvelp )
      *result = SCIP_SOLVELP;
   else if( infeasible )
   {
      *result = SCIP_INFEASIBLE;
      
      /* at least one constraint is violated by pseudo solution and we didn't find a better way to resolve this:
       * -> branch on pseudo solution
       */
      CHECK_OKAY( branchPseudo(scip, conshdlr, result) );
   }
   
   return SCIP_OKAY;
}

static
DECL_CONSCHECK(consCheckSetppc)
{
   CONSDATA* consdata;
   CONS* cons;
   SETPPCCONS* setppccons;
   VAR** vars;
   Real solval;
   int nvars;
   int c;
   int v;
   Bool found;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(nconss == 0 || conss != NULL);
   assert(result != NULL);

   *result = SCIP_FEASIBLE;

   /* check all set partitioning / packing / covering constraints for feasibility */
   for( c = 0; c < nconss; ++c )
   {
      cons = conss[c];
      consdata = SCIPconsGetData(cons);
      assert(consdata != NULL);
      setppccons = consdata->setppccons;
      assert(setppccons != NULL);
      if( checklprows || consdata->row == NULL || !SCIProwIsInLP(consdata->row) )
      {
         if( !check(scip, setppccons, sol) )
         {
            /* constraint is violated */
            CHECK_OKAY( SCIPresetConsAge(scip, cons) );
            *result = SCIP_INFEASIBLE;
            return SCIP_OKAY;
         }
         else
         {
            CHECK_OKAY( SCIPincConsAge(scip, cons) );
         }
      }
   }
   
   return SCIP_OKAY;
}




/*
 * Presolving
 */

/** deletes all zero-fixed variables */
static
RETCODE setppcconsApplyFixings(
   SCIP*            scip,               /**< SCIP data structure */
   SETPPCCONS*      setppccons          /**< set partitioning / packing / covering constraint object */
   )
{
   VAR* var;
   int v;

   assert(setppccons != NULL);

   if( setppccons->nfixedzeros >= 1 )
   {
      assert(setppccons->vars != NULL);

      v = 0;
      while( v < setppccons->nvars )
      {
         var = setppccons->vars[v];
         if( SCIPisZero(scip, SCIPvarGetUbGlobal(var)) )
         {
            CHECK_OKAY( setppcconsDelCoefPos(scip, setppccons, v) );
         }
         else
            ++v;
      }
   }

   return SCIP_OKAY;
}

static
DECL_CONSPRESOL(consPresolSetppc)
{
   CONS* cons;
   CONSDATA* consdata;
   SETPPCCONS* setppccons;
   Bool infeasible;
   int c;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(scip != NULL);
   assert(result != NULL);

   *result = SCIP_DIDNOTFIND;

   /* process constraints */
   for( c = 0; c < nconss && *result != SCIP_CUTOFF; ++c )
   {
      cons = conss[c];
      assert(cons != NULL);
      consdata = SCIPconsGetData(cons);
      assert(consdata != NULL);
      setppccons = consdata->setppccons;
      assert(setppccons != NULL);

      if( !setppccons->changed )
         continue;

      debugMessage("presolving set partitioning / packing / covering constraint <%s>\n", SCIPconsGetName(cons));

      /* remove all variables that are fixed to zero */
      CHECK_OKAY( setppcconsApplyFixings(scip, setppccons) );

      if( setppccons->nfixedones >= 2 )
      {
         /* at least two variables are fixed to 1:
          * - a set covering constraint is feasible anyway and can be deleted
          * - a set partitioning or packing constraint is infeasible
          */
         if( setppccons->setppctype == SCIP_SETPPCTYPE_COVERING )
         {
            debugMessage("set covering constraint <%s> is redundant\n", SCIPconsGetName(cons));
            CHECK_OKAY( SCIPdelCons(scip, cons) );
            (*ndelconss)++;
            *result = SCIP_SUCCESS;
            continue;
         }
         else
         {
            debugMessage("set partitioning / packing constraint <%s> is infeasible\n", SCIPconsGetName(cons));
            *result = SCIP_CUTOFF;
            return SCIP_OKAY;
         }
      }
      else if( setppccons->nfixedones == 1 )
      {
         /* exactly one variable is fixed to 1:
          * - a set covering constraint is feasible anyway and can be disabled
          * - all other variables in a set partitioning or packing constraint must be zero
          */
         if( setppccons->setppctype == SCIP_SETPPCTYPE_COVERING )
         {
            debugMessage("set covering constraint <%s> is redundant\n", SCIPconsGetName(cons));
            CHECK_OKAY( SCIPdelCons(scip, cons) );
            (*ndelconss)++;
            *result = SCIP_SUCCESS;
            continue;
         }
         else
         {
            VAR* var;
            int v;
            
            debugMessage("set partitioning / packing constraint <%s> has a variable fixed to 1.0\n", SCIPconsGetName(cons));
            for( v = 0; v < setppccons->nvars; ++v )
            {
               var = setppccons->vars[v];
               if( SCIPisZero(scip, SCIPvarGetLbGlobal(var)) && !SCIPisZero(scip, SCIPvarGetUbGlobal(var)) )
               {
                  CHECK_OKAY( SCIPfixVar(scip, var, 0.0, &infeasible) );
                  assert(!infeasible);
                  (*nfixedvars)++;
                  *result = SCIP_SUCCESS;
               }
            }

            /* now all other variables are fixed to zero:
             * the constraint is feasible, and if it's not modifiable, it is redundant
             */
            if( !setppccons->modifiable )
            {
               debugMessage("set partitioning / packing constraint <%s> is redundant\n", SCIPconsGetName(cons));
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
         }
      }
      else if( !setppccons->modifiable )
      {
         /* all other preprocessings can only be done on non-modifiable constraints */
         if( setppccons->nfixedzeros == setppccons->nvars )
         {
            /* all variables are fixed to zero:
             * - a set packing constraint is feasible anyway and can be deleted
             * - a set partitioning or covering constraint is infeasible, and so is the whole problem
             */
            assert(setppccons->nfixedones == 0);
            
            if( setppccons->setppctype == SCIP_SETPPCTYPE_PACKING )
            {
               debugMessage("set packing constraint <%s> is redundant\n", SCIPconsGetName(cons));
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
            else
            {
               debugMessage("set partitioning / covering constraint <%s> is infeasible\n", SCIPconsGetName(cons));
               *result = SCIP_CUTOFF;
               return SCIP_OKAY;
            }
         }
         else if( setppccons->nfixedzeros == setppccons->nvars - 1 )
         {
            /* all variables except one are fixed to zero:
             * - a set packing constraint is feasible anyway, and can be deleted
             * - a set partitioning or covering constraint is feasible and can be deleted after the
             *   remaining variable is fixed to one
             */
            assert(setppccons->nfixedones == 0);
         
            if( setppccons->setppctype == SCIP_SETPPCTYPE_PACKING )
            {
               debugMessage("set packing constraint <%s> is redundant\n", SCIPconsGetName(cons));
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
            else
            {
               VAR* var;
               Bool found;
               int v;
               
               debugMessage("set partitioning / covering constraint <%s> has only one variable not fixed to 0.0\n",
                  SCIPconsGetName(cons));
               
               /* search unfixed variable */
               found = FALSE;
               for( v = 0; v < setppccons->nvars && !found; ++v )
               {
                  var = setppccons->vars[v];
                  found = !SCIPisZero(scip, SCIPvarGetUbGlobal(var));
               }
               assert(found);

               CHECK_OKAY( SCIPfixVar(scip, var, 1.0, &infeasible) );
               assert(!infeasible);
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*nfixedvars)++;
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
         }
         else if( setppccons->nfixedzeros == setppccons->nvars - 2
            && setppccons->setppctype == SCIP_SETPPCTYPE_PARTITIONING )
         {
            VAR* var;
            VAR* var1;
            VAR* var2;
            int v;

            /* aggregate variable and delete constraint, if set partitioning constraint consists only of two
             * non-fixed variables
             */
            
            /* search unfixed variable */
            var1 = NULL;
            var2 = NULL;
            for( v = 0; v < setppccons->nvars && var2 == NULL; ++v )
            {
               var = setppccons->vars[v];
               if( !SCIPisZero(scip, SCIPvarGetUbGlobal(var)) )
               {
                  if( var1 == NULL )
                     var1 = var;
                  else
                     var2 = var;
               }
            }
            assert(var1 != NULL && var2 != NULL);
            if( SCIPvarGetStatus(var1) != SCIP_VARSTATUS_AGGREGATED )
            {
               debugMessage("set partitioning constraint <%s>: aggregate <%s> == 1 - <%s>\n",
                  SCIPconsGetName(cons), SCIPvarGetName(var1), SCIPvarGetName(var2));
               CHECK_OKAY( SCIPaggregateVar(scip, var1, var2, -1.0, 1.0, &infeasible) );
               if( infeasible )
               {
                  debugMessage("set partitioning constraint <%s>: infeasible aggregation <%s> == 1 - <%s>\n",
                     SCIPconsGetName(cons), SCIPvarGetName(var1), SCIPvarGetName(var2));
                  *result = SCIP_CUTOFF;
                  return SCIP_OKAY;
               }
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*naggrvars)++;
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
            else if( SCIPvarGetStatus(var2) != SCIP_VARSTATUS_AGGREGATED )
            {
               debugMessage("set partitioning constraint <%s>: aggregate <%s> == 1 - <%s>\n",
                  SCIPconsGetName(cons), SCIPvarGetName(var2), SCIPvarGetName(var1));
               CHECK_OKAY( SCIPaggregateVar(scip, var2, var1, -1.0, 1.0, &infeasible) );
               if( infeasible )
               {
                  debugMessage("set partitioning constraint <%s>: infeasible aggregation <%s> == 1 - <%s>\n",
                     SCIPconsGetName(cons), SCIPvarGetName(var1), SCIPvarGetName(var2));
                  *result = SCIP_CUTOFF;
                  return SCIP_OKAY;
               }
               CHECK_OKAY( SCIPdelCons(scip, cons) );
               (*naggrvars)++;
               (*ndelconss)++;
               *result = SCIP_SUCCESS;
               continue;
            }
         }
      }

      setppccons->changed = FALSE;
   }
   
   return SCIP_OKAY;
}




/*
 * variable usage counting
 */

static
DECL_CONSENABLE(consEnableSetppc)
{
   CONSHDLRDATA* conshdlrdata;
   CONSDATA* consdata;
   SETPPCCONS* setppccons;
   int v;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(cons != NULL);

   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   consdata = SCIPconsGetData(cons);
   assert(consdata != NULL);
   setppccons = consdata->setppccons;
   assert(setppccons != NULL);

   debugMessage("enabling information method of set partitioning / packing / covering constraint handler\n");

   /* increase the number of uses for each variable in the constraint */
   for( v = 0; v < setppccons->nvars; ++v )
   {
      CHECK_OKAY( conshdlrdataIncVaruses(scip, conshdlrdata, setppccons->vars[v]) );
   }

   return SCIP_OKAY;
}

static
DECL_CONSDISABLE(consDisableSetppc)
{
   CONSHDLRDATA* conshdlrdata;
   CONSDATA* consdata;
   SETPPCCONS* setppccons;
   int v;

   assert(conshdlr != NULL);
   assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
   assert(cons != NULL);

   conshdlrdata = SCIPconshdlrGetData(conshdlr);
   assert(conshdlrdata != NULL);

   consdata = SCIPconsGetData(cons);
   assert(consdata != NULL);
   setppccons = consdata->setppccons;
   assert(setppccons != NULL);

   debugMessage("disabling information method of set partitioning / packing / covering constraint handler\n");

   /* decrease the number of uses for each variable in the constraint */
   for( v = 0; v < setppccons->nvars; ++v )
   {
      CHECK_OKAY( conshdlrdataDecVaruses(scip, conshdlrdata, setppccons->vars[v]) );
   }

   return SCIP_OKAY;
}

/** creates and captures a set partitioning / packing / covering constraint */
static
RETCODE createConsSetppc(
   SCIP*            scip,               /**< SCIP data structure */
   CONS**           cons,               /**< pointer to hold the created constraint */
   const char*      name,               /**< name of constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< array with variables of constraint entries */
   SETPPCTYPE       setppctype,         /**< type of constraint: set partitioning, packing, or covering constraint */
   Bool             separate,           /**< should the constraint be separated during LP processing? */
   Bool             enforce,            /**< should the constraint be enforced during node processing? */
   Bool             check,              /**< should the constraint be checked for feasibility? */
   Bool             local,              /**< is set partitioning constraint only valid locally? */
   Bool             modifiable,         /**< is row modifiable during node processing (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   CONSHDLR* conshdlr;
   CONSDATA* consdata;

   assert(scip != NULL);

   /* find the set partitioning constraint handler */
   conshdlr = SCIPfindConsHdlr(scip, CONSHDLR_NAME);
   if( conshdlr == NULL )
   {
      errorMessage("set partitioning / packing / covering constraint handler not found");
      return SCIP_INVALIDCALL;
   }

   /* create the constraint specific data */
   CHECK_OKAY( SCIPallocBlockMemory(scip, &consdata) );
   if( SCIPstage(scip) == SCIP_STAGE_PROBLEM )
   {
      if( local )
      {
         errorMessage("problem constraint cannot be local");
         return SCIP_INVALIDDATA;
      }

      /* create constraint in original problem */
      CHECK_OKAY( setppcconsCreate(scip, &consdata->setppccons, nvars, vars, setppctype,
                     modifiable, removeable) );
   }
   else
   {
      /* create constraint in transformed problem */
      CHECK_OKAY( setppcconsCreateTransformed(scip, &consdata->setppccons, nvars, vars, setppctype,
                     local, modifiable, removeable) );
   }
   consdata->row = NULL;

   /* create constraint (propagation is never used for set partitioning / packing / covering constraints) */
   CHECK_OKAY( SCIPcreateCons(scip, cons, name, conshdlr, consdata, separate, enforce, check, FALSE) );

   return SCIP_OKAY;
}

/** creates and captures a normalized (with all coefficients +1) setppc constraint */
static
RETCODE createNormalizedSetppc(
   SCIP*            scip,               /**< SCIP data structure */
   CONS**           cons,               /**< pointer to hold the created constraint */
   const char*      name,               /**< name of constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< array with variables of constraint entries */
   Real*            vals,               /**< array with coefficients (+1.0 or -1.0) */
   int              mult,               /**< multiplier on the coefficients(+1 or -1) */
   SETPPCTYPE       setppctype,         /**< type of constraint: set partitioning, packing, or covering constraint */
   Bool             separate,           /**< should the constraint be separated during LP processing? */
   Bool             enforce,            /**< should the constraint be enforced during node processing? */
   Bool             check,              /**< should the constraint be checked for feasibility? */
   Bool             local,              /**< is set partitioning constraint only valid locally? */
   Bool             modifiable,         /**< is row modifiable during node processing (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   VAR** transvars;
   int v;

   assert(nvars == 0 || vars != NULL);
   assert(nvars == 0 || vals != NULL);
   assert(mult == +1 || mult == -1);

   /* get temporary memory */
   CHECK_OKAY( SCIPcaptureBufferArray(scip, &transvars, nvars) );

   /* negate positive or negative variables */
   for( v = 0; v < nvars; ++v )
   {
      if( mult * vals[v] > 0.0 )
         transvars[v] = vars[v];
      else
      {
         CHECK_OKAY( SCIPgetNegatedVar(scip, vars[v], &transvars[v]) );
      }
      assert(transvars[v] != NULL);
   }

   /* create the constraint */
   CHECK_OKAY( createConsSetppc(scip, cons, name, nvars, transvars, setppctype,
      separate, enforce, check, local, modifiable, removeable) );

   /* release temporary memory */
   CHECK_OKAY( SCIPreleaseBufferArray(scip, &transvars) );

   return SCIP_OKAY;
}

static
DECL_LINCONSUPGD(linconsUpgdSetppc)
{
   assert(upgdcons != NULL);

   /* check, if linear constraint can be upgraded to set partitioning, packing, or covering constraint
    * - all set partitioning / packing / covering constraints consist only of binary variables with a
    *   coefficient of +1.0 or -1.0 (variables with -1.0 coefficients can be negated):
    *        lhs     <= x1 + ... + xp - y1 - ... - yn <= rhs
    * - negating all variables y = (1-Y) with negative coefficients gives:
    *        lhs + n <= x1 + ... + xp + Y1 + ... + Yn <= rhs + n
    * - negating all variables x = (1-X) with positive coefficients and multiplying with -1 gives:
    *        p - rhs <= X1 + ... + Xp + y1 + ... + yn <= p - lhs
    * - a set partitioning constraint has left hand side of +1.0, and right hand side of +1.0 : x(S) == 1.0
    *    -> without negations:  lhs == rhs == 1 - n  or  lhs == rhs == p - 1
    * - a set packing constraint has left hand side of -infinity, and right hand side of +1.0 : x(S) <= 1.0
    *    -> without negations:  (lhs == -inf  and  rhs == 1 - n)  or  (lhs == p - 1  and  rhs = +inf)
    * - a set covering constraint has left hand side of +1.0, and right hand side of +infinity: x(S) >= 1.0
    *    -> without negations:  (lhs == 1 - n  and  rhs == +inf)  or  (lhs == -inf  and  rhs = p - 1)
    */
   if( nposbin + nnegbin == nvars && ncoeffspone + ncoeffsnone == nvars )
   {
      int mult;

      if( SCIPisEQ(scip, lhs, rhs) && (SCIPisEQ(scip, lhs, 1 - ncoeffsnone) || SCIPisEQ(scip, lhs, ncoeffspone - 1)) )
      {
         debugMessage("upgrading constraint <%s> to set partitioning constraint\n", SCIPconsGetName(cons));
         
         /* check, if we have to multiply with -1 (negate the positive vars) or with +1 (negate the negative vars) */
         mult = SCIPisEQ(scip, lhs, 1 - ncoeffsnone) ? +1 : -1;

         /* create the set partitioning constraint (an automatically upgraded constraint is always unmodifiable) */
         CHECK_OKAY( createNormalizedSetppc(scip, upgdcons, SCIPconsGetName(cons), nvars, vars, vals, mult,
                        SCIP_SETPPCTYPE_PARTITIONING,
                        SCIPconsIsSeparated(cons), SCIPconsIsEnforced(cons), SCIPconsIsChecked(cons),
                        local, FALSE, removeable) );
      }
      else if( (SCIPisInfinity(scip, -lhs) && SCIPisEQ(scip, rhs, 1 - ncoeffsnone))
         || (SCIPisEQ(scip, lhs, ncoeffspone - 1) && SCIPisInfinity(scip, rhs)) )
      {
         debugMessage("upgrading constraint <%s> to set packing constraint\n", SCIPconsGetName(cons));
         
         /* check, if we have to multiply with -1 (negate the positive vars) or with +1 (negate the negative vars) */
         mult = SCIPisInfinity(scip, -lhs) ? +1 : -1;

         /* create the set packing constraint (an automatically upgraded constraint is always unmodifiable) */
         CHECK_OKAY( createNormalizedSetppc(scip, upgdcons, SCIPconsGetName(cons), nvars, vars, vals, mult,
                        SCIP_SETPPCTYPE_PACKING,
                        SCIPconsIsSeparated(cons), SCIPconsIsEnforced(cons), SCIPconsIsChecked(cons),
                        local, FALSE, removeable) );
      }
      else if( (SCIPisEQ(scip, lhs, 1 - ncoeffsnone) && SCIPisInfinity(scip, rhs))
         || (SCIPisInfinity(scip, -lhs) && SCIPisEQ(scip, rhs, ncoeffspone - 1)) )
      {
         debugMessage("upgrading constraint <%s> to set covering constraint\n", SCIPconsGetName(cons));
         
         /* check, if we have to multiply with -1 (negate the positive vars) or with +1 (negate the negative vars) */
         mult = SCIPisInfinity(scip, rhs) ? +1 : -1;

         /* create the set covering constraint (an automatically upgraded constraint is always unmodifiable) */
         CHECK_OKAY( createNormalizedSetppc(scip, upgdcons, SCIPconsGetName(cons), nvars, vars, vals, mult,
                        SCIP_SETPPCTYPE_COVERING,
                        SCIPconsIsSeparated(cons), SCIPconsIsEnforced(cons), SCIPconsIsChecked(cons),
                        local, FALSE, removeable) );
      }
   }

   return SCIP_OKAY;
}




/*
 * Callback methods of event handler
 */

static
DECL_EVENTEXEC(eventExecSetppc)
{
   SETPPCCONS* setppccons;
   EVENTTYPE eventtype;

   assert(eventhdlr != NULL);
   assert(eventdata != NULL);
   assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENTHDLR_NAME) == 0);
   assert(event != NULL);

   debugMessage("Exec method of bound change event handler for set partitioning / packing / covering constraints\n");

   setppccons = (SETPPCCONS*)eventdata;
   assert(setppccons != NULL);

   eventtype = SCIPeventGetType(event);
   switch( eventtype )
   {
   case SCIP_EVENTTYPE_LBTIGHTENED:
      setppccons->nfixedones++;
      break;
   case SCIP_EVENTTYPE_LBRELAXED:
      setppccons->nfixedones--;
      break;
   case SCIP_EVENTTYPE_UBTIGHTENED:
      setppccons->nfixedzeros++;
      break;
   case SCIP_EVENTTYPE_UBRELAXED:
      setppccons->nfixedzeros--;
      break;
   default:
      errorMessage("invalid event type");
      abort();
   }
   assert(0 <= setppccons->nfixedzeros && setppccons->nfixedzeros <= setppccons->nvars);
   assert(0 <= setppccons->nfixedones && setppccons->nfixedones <= setppccons->nvars);

   setppccons->changed = TRUE;

   debugMessage(" -> constraint has %d zero-fixed and %d one-fixed of %d variables\n", 
      setppccons->nfixedzeros, setppccons->nfixedones, setppccons->nvars);

   return SCIP_OKAY;
}




/*
 * constraint specific interface methods
 */

/** creates the handler for set partitioning / packing / covering constraints and includes it in SCIP */
RETCODE SCIPincludeConsHdlrSetppc(
   SCIP*            scip                /**< SCIP data structure */
   )
{
   CONSHDLRDATA* conshdlrdata;

   /* create event handler for bound change events */
   CHECK_OKAY( SCIPincludeEventhdlr(scip, EVENTHDLR_NAME, EVENTHDLR_DESC,
                  NULL, NULL, NULL,
                  NULL, eventExecSetppc,
                  NULL) );

   /* create constraint handler data */
   CHECK_OKAY( conshdlrdataCreate(scip, &conshdlrdata) );

   /* include constraint handler */
   CHECK_OKAY( SCIPincludeConsHdlr(scip, CONSHDLR_NAME, CONSHDLR_DESC,
                  CONSHDLR_SEPAPRIORITY, CONSHDLR_ENFOPRIORITY, CONSHDLR_CHECKPRIORITY,
                  CONSHDLR_SEPAFREQ, CONSHDLR_PROPFREQ,
                  CONSHDLR_NEEDSCONS,
                  consFreeSetppc, NULL, NULL,
                  consDeleteSetppc, consTransSetppc, 
                  consSepaSetppc, consEnfolpSetppc, consEnfopsSetppc, consCheckSetppc, NULL, consPresolSetppc,
                  consEnableSetppc, consDisableSetppc,
                  conshdlrdata) );

   /* include the linear constraint to set partitioning constraint upgrade in the linear constraint handler */
   CHECK_OKAY( SCIPincludeLinconsUpgrade(scip, linconsUpgdSetppc, LINCONSUPGD_PRIORITY) );

   /* set partitioning constraint handler parameters */
   CHECK_OKAY( SCIPaddIntParam(scip,
                  "conshdlr/setppc/npseudobranches", 
                  "number of children created in pseudo branching",
                  &conshdlrdata->npseudobranches, DEFAULT_NPSEUDOBRANCHES, 2, INT_MAX, NULL, NULL) );
   
   return SCIP_OKAY;
}


/** creates and captures a set partitioning constraint */
RETCODE SCIPcreateConsSetpart(
   SCIP*            scip,               /**< SCIP data structure */
   CONS**           cons,               /**< pointer to hold the created constraint */
   const char*      name,               /**< name of constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< array with variables of constraint entries */
   Bool             separate,           /**< should the constraint be separated during LP processing? */
   Bool             enforce,            /**< should the constraint be enforced during node processing? */
   Bool             check,              /**< should the constraint be checked for feasibility? */
   Bool             local,              /**< is set partitioning constraint only valid locally? */
   Bool             modifiable,         /**< is row modifiable during node processing (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   return createConsSetppc(scip, cons, name, nvars, vars, SCIP_SETPPCTYPE_PARTITIONING,
      separate, enforce, check, local, modifiable, removeable);
}

/** creates and captures a set packing constraint */
RETCODE SCIPcreateConsSetpack(
   SCIP*            scip,               /**< SCIP data structure */
   CONS**           cons,               /**< pointer to hold the created constraint */
   const char*      name,               /**< name of constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< array with variables of constraint entries */
   Bool             separate,           /**< should the constraint be separated during LP processing? */
   Bool             enforce,            /**< should the constraint be enforced during node processing? */
   Bool             check,              /**< should the constraint be checked for feasibility? */
   Bool             local,              /**< is set partitioning constraint only valid locally? */
   Bool             modifiable,         /**< is row modifiable during node processing (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   return createConsSetppc(scip, cons, name, nvars, vars, SCIP_SETPPCTYPE_PACKING,
      separate, enforce, check, local, modifiable, removeable);
}

/** creates and captures a set covering constraint */
RETCODE SCIPcreateConsSetcover(
   SCIP*            scip,               /**< SCIP data structure */
   CONS**           cons,               /**< pointer to hold the created constraint */
   const char*      name,               /**< name of constraint */
   int              nvars,              /**< number of variables in the constraint */
   VAR**            vars,               /**< array with variables of constraint entries */
   Bool             separate,           /**< should the constraint be separated during LP processing? */
   Bool             enforce,            /**< should the constraint be enforced during node processing? */
   Bool             check,              /**< should the constraint be checked for feasibility? */
   Bool             local,              /**< is set partitioning constraint only valid locally? */
   Bool             modifiable,         /**< is row modifiable during node processing (subject to column generation)? */
   Bool             removeable          /**< should the row be removed from the LP due to aging or cleanup? */
   )
{
   return createConsSetppc(scip, cons, name, nvars, vars, SCIP_SETPPCTYPE_COVERING,
      separate, enforce, check, local, modifiable, removeable);
}

