/*
 * @file probdata_spa.c
 *
 *  Created on: Dec 1, 2015
 *      Author: bzfeifle
 */
#include "probdata_spa.h"
#include "scip/cons_linear.h"
#include "scip/cons_logicor.h"
#include "scip/var.h"

#include <math.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>


struct SCIP_ProbData
{
   SCIP_VAR****          edgevars;         /**< variables for the edges cut by a partioning */
   SCIP_VAR***           binvars;          /**< variable-matrix belonging to the bin-cluster assignment */
   SCIP_Real**           cmatrix;          /**< matrix to save the transition matrix */
   SCIP_Real             scale;        /**< The weight for the coherence in the objective function */
   SCIP_Real             coherence;
   char                  model_variant;     /**< The model that is used. w for weighted objective, e for normal edge-representation */
   int                   nbins;
   int                   ncluster;
};


/** Creates all the variables for the problem. The constraints are added later, depending on the model that is used */
static
SCIP_RETCODE createVariables(
   SCIP*                 scip,               /**< SCIP Data Structure */
   SCIP_PROBDATA*        probdata            /**< The problem data */
)
{
   int i;
   int j;
   int c;
   int edgetype;
   int nbins = probdata->nbins;
   int ncluster = probdata->ncluster;
   char varname[SCIP_MAXSTRLEN];


   /* create variables for bins */
   SCIP_CALL( SCIPsetObjsense(scip, SCIP_OBJSENSE_MAXIMIZE) );
   SCIP_CALL( SCIPallocMemoryArray(scip, &(probdata->binvars), nbins) );
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &(probdata->binvars[i]), ncluster) );
   }

   for( i = 0; i < nbins; ++i )
   {
      for( c = 0; c < ncluster; ++c )
      {
         (void)SCIPsnprintf(varname, SCIP_MAXSTRLEN, "x_%d_%d", i, c);
         SCIP_CALL( SCIPcreateVarBasic(scip, &probdata->binvars[i][c], varname, 0.0, 1.0, 0.0, SCIP_VARTYPE_BINARY) );
         SCIP_CALL( SCIPaddVar(scip, probdata->binvars[i][c]) );
         SCIP_CALL( SCIPvarChgBranchPriority(probdata->binvars[i][c], 5) );
      }
   }
   /* fix one bin now to reduce symmetry */
   SCIP_CALL( SCIPchgVarLbGlobal(scip, probdata->binvars[nbins - 1][0], 1.0) );


   /* create variables for the edges in each cluster combination. Index 0 are edges within cluster, 1 edges between consequtive clusters and 2 edges between non-consequtive clusters */
   SCIP_CALL( SCIPallocClearMemoryArray(scip, &(probdata->edgevars), nbins) );
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocClearMemoryArray(scip, &(probdata->edgevars[i]), nbins) );
      for( j = 0; j < nbins; ++j )
      {
         if( SCIPisZero(scip, (probdata->cmatrix[i][j] - probdata->cmatrix[j][i])) && SCIPisZero(scip, (probdata->cmatrix[i][j] + probdata->cmatrix[j][i])) )
            continue;
         SCIP_CALL( SCIPallocMemoryArray(scip, &(probdata->edgevars[i][j]), 3) );
         for( edgetype = 0; edgetype < 3; ++edgetype )
         {
            (void)SCIPsnprintf(varname, SCIP_MAXSTRLEN, "y_%d_%d_%d", i, j, edgetype );
            SCIP_CALL( SCIPcreateVarBasic(scip, &probdata->edgevars[i][j][edgetype], varname, 0.0, 1.0, 0.0, SCIP_VARTYPE_IMPLINT) );
            SCIP_CALL( SCIPaddVar(scip, probdata->edgevars[i][j][edgetype]) );
         }
      }
   }

   return SCIP_OKAY;
}

/** create the problem without variable amount of clusters */
static
SCIP_RETCODE createProbSimplified(
   SCIP*                 scip,               /**< SCIP Data Structure */
   SCIP_PROBDATA*        probdata            /**< The problem data */
)
{
   int i;
   int j;
   int c1;
   int c2;
   char consname[SCIP_MAXSTRLEN];
   SCIP_CONS* temp;
   int nbins = probdata->nbins;
   int ncluster = probdata->ncluster;
   SCIP_Real scale;

   SCIPgetRealParam(scip, "scale_coherence", &scale);
   probdata->scale = scale;

   /*
    *  create constraints
    */

   /* create the set-partitioning constraints of the bins */
   for( i = 0; i < nbins; ++i )
   {
      (void)SCIPsnprintf(consname, SCIP_MAXSTRLEN, "setpart_%d", i+1 );
      SCIP_CALL( SCIPcreateConsSetpart(scip, &temp, consname, 0, NULL, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE) );
      for ( c1 = 0; c1 < ncluster; ++c1 )
      {
         SCIP_CALL( SCIPaddCoefSetppc(scip, temp, probdata->binvars[i][c1]) );
      }
      SCIP_CALL( SCIPaddCons(scip, temp) );
      SCIP_CALL( SCIPreleaseCons(scip, &temp) );
   }

   /* create constraints for the edge-cut variables */
   SCIPinfoMessage(scip, NULL, "Using edge-representation with simplified structure. No variable amount of cluster. \n");
   for( i = 0; i < nbins; ++i )
   {
      for( j = 0; j < i; ++j )
      {
         if( SCIPisZero(scip, (probdata->cmatrix[i][j] - probdata->cmatrix[j][i])) && SCIPisZero(scip, (probdata->cmatrix[i][j] + probdata->cmatrix[j][i]) * scale ) )
            continue;
         /* set the objective weight for the edge-variables */

         /* these edges are not within a cluster */
         SCIP_CALL( SCIPchgVarObj(scip, probdata->edgevars[i][j][0], (probdata->cmatrix[i][j] + probdata->cmatrix[j][i]) * scale ) );
         /* these are the edges that are between consequtive clusters */
         SCIP_CALL( SCIPchgVarObj(scip, probdata->edgevars[i][j][1], (probdata->cmatrix[i][j] - probdata->cmatrix[j][i])) );
         SCIP_CALL( SCIPchgVarObj(scip, probdata->edgevars[j][i][1], (probdata->cmatrix[j][i] - probdata->cmatrix[i][j])) );

         /* create constraints that determine when the edge-variables have to be non-zero*/
         for( c1 = 0; c1 < ncluster; ++c1 )
         {
            /* constraints for edges within clusters*/
            (void)SCIPsnprintf(consname, SCIP_MAXSTRLEN, "bins_%d_%d_incluster_%d", i+1, j+1, c1+1 );
            SCIP_CALL( SCIPcreateConsLinear(scip, &temp, consname, 0, NULL, NULL, -SCIPinfinity(scip), 1.0, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE ) );
            SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->edgevars[i][j][0], -1.0) );
            SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->binvars[i][c1], 1.0) );
            SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->binvars[j][c1], 1.0) );
            SCIP_CALL( SCIPaddCons(scip, temp) );
            SCIP_CALL( SCIPreleaseCons(scip, &temp) );

            /* constraints for edges between clusters*/
            for( c2 = 0; c2 < ncluster; ++c2 )
            {
               SCIP_VAR* var;
               if( c2 == c1 )
                  continue;
               if( c2 == c1 + 1 || ( c2 == 0 && c1 == ncluster -1) )
                  var = probdata->edgevars[i][j][1];
               else if( c2 == c1 - 1 || ( c1 == 0 && c2 == ncluster -1) )
                  var = probdata->edgevars[j][i][1];
               else
                  var = probdata->edgevars[j][i][2];

               /*f two bins are in a different cluster, then the corresponding edge must be cut*/
               (void)SCIPsnprintf(consname, SCIP_MAXSTRLEN, "bins_%d_%d_inclusters_%d_%d", i+1, j+1, c1+1, c2+1 );
               SCIP_CALL( SCIPcreateConsLinear(scip, &temp, consname, 0, NULL, NULL, -SCIPinfinity(scip), 1.0, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE ) );
               SCIP_CALL( SCIPaddCoefLinear(scip, temp, var, -1.0) );
               SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->binvars[i][c1], 1.0) );
               SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->binvars[j][c2], 1.0) );
               SCIP_CALL( SCIPaddCons(scip, temp) );
               SCIP_CALL( SCIPreleaseCons(scip, &temp) );

            }
         }
      }
   }

   /*  only one cluster-pair at the same time can be active for an edge*/
   for( i = 0; i < nbins; ++i )
   {
      for( j = 0; j < i; ++j )
      {
         if( NULL == probdata->edgevars[i][j] || SCIPisZero(scip, (probdata->cmatrix[i][j] - probdata->cmatrix[j][i])) )
            continue;
         (void)SCIPsnprintf(consname, SCIP_MAXSTRLEN, "sumedge_%d_%d",  i+1, j+1 );
         SCIP_CALL( SCIPcreateConsBasicLinear(scip, &temp, consname, 0, NULL, NULL, 0.0, 1.0 ) );
         for( c1 = 0; c1 < 3; ++c1 )
         {
            SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->edgevars[i][j][c1], 1.0) );
         }
         SCIP_CALL( SCIPaddCoefLinear(scip, temp, probdata->edgevars[j][i][1], 1.0) );

         SCIP_CALL( SCIPaddCons(scip, temp) );
         SCIP_CALL( SCIPreleaseCons(scip, &temp) );
      }
   }

   /* add constraint that ensures that each cluster is used */
   for( c1 = 0; c1 < ncluster; ++c1 )
   {
      (void)SCIPsnprintf(consname, SCIP_MAXSTRLEN, "cluster_%d_used", c1+1 );
      SCIP_CALL( SCIPcreateConsBasicLogicor(scip, &temp, consname, 0, NULL) );
      /*      SCIP_CALL( SCIPaddCoefSetppc(scip, temp, probdata->indicatorvar[0]) );*/
      for( i = 0; i < nbins; ++i )
      {
         SCIP_CALL( SCIPaddCoefLogicor(scip, temp, probdata->binvars[i][c1]) );
      }
      SCIP_CALL( SCIPaddCons(scip, temp) );
      SCIP_CALL( SCIPreleaseCons(scip, &temp) );
   }

   return SCIP_OKAY;
}

/** Scip callback to transform the problem */
static
SCIP_DECL_PROBTRANS(probtransSpa)
{
   int i;
   int j;
   int c;
   int edgetype;
   int nbins = sourcedata->nbins;
   int ncluster = sourcedata->ncluster;
   assert(scip != NULL);
   assert(sourcedata != NULL);
   assert(targetdata != NULL);

   /* allocate memory */
   SCIP_CALL( SCIPallocMemory(scip, targetdata) );

   (*targetdata)->nbins = sourcedata->nbins;
   (*targetdata)->ncluster = sourcedata->ncluster;
   (*targetdata)->coherence = sourcedata->coherence;
   (*targetdata)->model_variant = sourcedata->model_variant;
   (*targetdata)->scale = sourcedata->scale;


   /* allocate memory */
   SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->cmatrix), nbins) );
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->cmatrix[i]), nbins) );
   }
   /* copy the matrizes */
   for ( i = 0; i < nbins; ++i )
   {
      for ( j = 0; j < nbins; ++j )
      {
         (*targetdata)->cmatrix[i][j] = sourcedata->cmatrix[i][j];
      }
   }

   /* copy the variables */
   SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->binvars), nbins) );
   SCIP_CALL( SCIPallocClearMemoryArray(scip, &((*targetdata)->edgevars), nbins) );

   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocClearMemoryArray(scip, &((*targetdata)->edgevars[i]), nbins) );
      for( j = 0; j < nbins; ++j )
      {
         if( sourcedata->edgevars[i][j] == NULL )
            continue;
         SCIP_CALL( SCIPallocClearMemoryArray(scip, &((*targetdata)->edgevars[i][j]), 3) );
         for( edgetype = 0; edgetype < 3; ++edgetype )
         {
            if( sourcedata->edgevars[i][j][edgetype] != NULL )
            {
               SCIP_CALL( SCIPtransformVar(scip, sourcedata->edgevars[i][j][edgetype], &((*targetdata)->edgevars[i][j][edgetype])) );
            }
            else
               ((*targetdata)->edgevars[i][j][edgetype]) = NULL;
         }
      }
   }
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->binvars[i]), ncluster) );

      for( c = 0; c < ncluster; ++c )
      {
         if( sourcedata->binvars[i][c] != NULL )
         {
            SCIP_CALL( SCIPtransformVar(scip, sourcedata->binvars[i][c], &((*targetdata)->binvars[i][c])) );
         }
         else
            (*targetdata)->binvars[i][c] = NULL;
      }
   }

   return SCIP_OKAY;
}

/** delete-callback method of scip */
static
SCIP_DECL_PROBDELORIG(probdelorigSpa)
{
   int c;
   int edgetype;
   int i;
   int j;

   assert(probdata != NULL);
   assert(*probdata != NULL);

   /* release all the variables */

   /* binvars */
   for ( c = 0; c < (*probdata)->nbins; ++c )
   {
      for ( i = 0; i < (*probdata)->ncluster; ++i )
      {
         if( (*probdata)->binvars[c][i] != NULL )
            SCIP_CALL( SCIPreleaseVar(scip, &((*probdata)->binvars[c][i])) );
      }
      SCIPfreeMemoryArray( scip, &((*probdata)->binvars[c]) );
   }

   /* cut-edge vars */

   for ( i = 0; i < (*probdata)->nbins; ++i )
   {
      for( j = 0; j < (*probdata)->nbins; ++j )
      {
         if( (*probdata)->edgevars[i][j] != NULL )
         {
            for ( edgetype = 0; edgetype < 3; ++edgetype )
            {
               if( (*probdata)->edgevars[i][j][edgetype] != NULL )
                  SCIP_CALL( SCIPreleaseVar( scip, &((*probdata)->edgevars[i][j][edgetype])) );
            }
            SCIPfreeMemoryArray( scip, &((*probdata)->edgevars[i][j]) );
         }
      }
      SCIPfreeMemoryArray( scip, &((*probdata)->edgevars[i]) );
   }

   SCIPfreeMemoryArray( scip, &((*probdata)->edgevars) );
   SCIPfreeMemoryArray(scip, &((*probdata)->binvars));
   for ( i = 0; i < (*probdata)->nbins; ++i )
   {
      SCIPfreeMemoryArray(scip, &((*probdata)->cmatrix[i]));
   }
   SCIPfreeMemoryArray(scip, &(*probdata)->cmatrix);

   SCIPfreeMemory(scip, probdata);

   return SCIP_OKAY;
}


/** scip-callback to delete the transformed problem */
static
SCIP_DECL_PROBDELORIG(probdeltransSpa)
{
   int c;
   int edgetype;
   int i;
   int j;

   assert(probdata != NULL);
   assert(*probdata != NULL);

   /* release all the variables */

   /* binvars */
   for ( i = 0; i < (*probdata)->nbins; ++i )
   {
      for ( c = 0; c < (*probdata)->ncluster; ++c )
      {
         if( (*probdata)->binvars[i][c] != NULL )
            SCIP_CALL( SCIPreleaseVar(scip, &((*probdata)->binvars[i][c])) );
      }
      SCIPfreeMemoryArray( scip, &((*probdata)->binvars[i]) );
   }

   /* cut-edge vars */

   for ( i = 0; i < (*probdata)->nbins; ++i )
   {
      for( j = 0; j < (*probdata)->nbins; ++j )
      {
         if( (*probdata)->edgevars[i][j] != NULL )
         {
            for ( edgetype = 0; edgetype < 3; ++edgetype )
            {
               if( (*probdata)->edgevars[i][j][edgetype] != NULL )
                  SCIP_CALL( SCIPreleaseVar( scip, &((*probdata)->edgevars[i][j][edgetype])) );
            }
            SCIPfreeMemoryArray( scip, &((*probdata)->edgevars[i][j]) );
         }
      }
      SCIPfreeMemoryArray( scip, &((*probdata)->edgevars[i]) );
   }

   SCIPfreeMemoryArray( scip, &((*probdata)->edgevars) );
   SCIPfreeMemoryArray(scip, &((*probdata)->binvars));

   for ( i = 0; i < (*probdata)->nbins; ++i )
   {
      SCIPfreeMemoryArray(scip, &((*probdata)->cmatrix[i]));
   }
   SCIPfreeMemoryArray(scip, &(*probdata)->cmatrix);

   SCIPfreeMemory(scip, probdata);

   return SCIP_OKAY;
}

/** callback method of scip for copying the probdata  */
static
SCIP_DECL_PROBCOPY(probcopySpa)
{
   int nbins;
   int ncluster;
   int i;
   int j;
   int c;
   int edgetype;
   SCIP_Bool success;
   SCIP_VAR* var;

   assert(scip != NULL);
   assert(sourcescip != NULL);
   assert(sourcedata != NULL);
   assert(targetdata != NULL);

   /* set up data */
   SCIP_CALL( SCIPallocMemory(scip, targetdata) );

   nbins = sourcedata->nbins;
   ncluster = sourcedata->ncluster;

   (*targetdata)->nbins = nbins;
   (*targetdata)->ncluster = ncluster;
   (*targetdata)->coherence = sourcedata->coherence;
   (*targetdata)->model_variant = sourcedata->model_variant;
   (*targetdata)->scale = sourcedata->scale;


   /* allocate memory */
   SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->cmatrix), nbins) );
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->cmatrix[i]), nbins) );
   }
   /* copy the matrizes */
   for ( i = 0; i < nbins; ++i )
   {
      for ( j = 0; j < nbins; ++j )
      {
         (*targetdata)->cmatrix[i][j] = sourcedata->cmatrix[i][j];
      }
   }

   /* copy the variables */
   SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->binvars), nbins) );
   SCIP_CALL( SCIPallocClearMemoryArray(scip, &((*targetdata)->edgevars), nbins) );


   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocClearMemoryArray(scip, &((*targetdata)->edgevars[i]), nbins) );
      for( j = 0; j < nbins; ++j )
      {
         if( (sourcedata)->edgevars[i][j] == NULL )
            continue;
         SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->edgevars[i][j]), 3) );

         for( edgetype = 0; edgetype < 3; ++edgetype )
         {
            if( sourcedata->edgevars[i][j][edgetype] != NULL )
            {
               SCIP_CALL( SCIPgetTransformedVar(sourcescip, sourcedata->edgevars[i][j][edgetype], &var) );
               if( SCIPvarIsActive(var) )
               {
                  SCIP_CALL( SCIPgetVarCopy(sourcescip, scip, var, &((*targetdata)->edgevars[i][j][edgetype]), varmap, consmap, global, &success) );
                  assert(success);
                  assert((*targetdata)->edgevars[i][j][edgetype] != NULL);
                  SCIP_CALL( SCIPcaptureVar(scip, (*targetdata)->edgevars[i][j][edgetype]) );
               }
               else
                  (*targetdata)->edgevars[i][j][edgetype] = NULL;
            }
            else
               (*targetdata)->edgevars[i][j][edgetype] = NULL;
         }
      }
   }
   for( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &((*targetdata)->binvars[i]), ncluster) );

      for( c = 0; c < ncluster; ++c )
      {
         if( sourcedata->binvars[i][c] != NULL )
         {
            SCIP_CALL( SCIPgetTransformedVar(sourcescip, sourcedata->binvars[i][c], &var) );
            if( SCIPvarIsActive(var) )
            {
               SCIP_CALL( SCIPgetVarCopy(sourcescip, scip, var, &((*targetdata)->binvars[i][c]), varmap, consmap, global, &success) );
               assert(success);
               assert((*targetdata)->binvars[i][c] != NULL);

               SCIP_CALL( SCIPcaptureVar(scip, (*targetdata)->binvars[i][c]) );
            }
            else
               (*targetdata)->binvars[i][c] = NULL;
         }
         else
            (*targetdata)->binvars[i][c] = NULL;
      }
   }

   assert(success);

   *result = SCIP_SUCCESS;

   return SCIP_OKAY;
}


/**
 * Create the probdata for an spa-clustering problem
 */
SCIP_RETCODE SCIPcreateProbSpa(
   SCIP*                 scip,               /**< SCIP data structure */
   const char*           name,               /**< problem name */
   int                   nbins,              /**< number of bins */
   SCIP_Real**           cmatrix             /**< The transition matrix */
)
{
   SCIP_PROBDATA* probdata = NULL;
   int i;
   int j;
   SCIP_Real epsC;
   int ncluster;

   assert(nbins > 0);  /* at least one node */

   SCIP_CALL( SCIPcreateProbBasic(scip, name) );

   /* get the parameters for the coherence bound */
   SCIP_CALL( SCIPgetRealParam( scip, "coherence_bound", &epsC ) );

   /* get the maximal amount of clusters */
   SCIP_CALL( SCIPgetIntParam(scip, "ncluster", &ncluster) );
   assert( ncluster <= nbins);

   /* Set up the problem */
   SCIP_CALL( SCIPallocMemory( scip, &probdata) );

   /* allocate memory for the matrizes and create them from the edge-arrays */
   SCIP_CALL( SCIPallocMemoryArray(scip, &(probdata->cmatrix), nbins) );
   for ( i = 0; i < nbins; ++i )
   {
      SCIP_CALL( SCIPallocMemoryArray(scip, &(probdata->cmatrix[i]), nbins) );
      for( j = 0; j < nbins; ++j )
      {
         probdata->cmatrix[i][j] = cmatrix[i][j];
      }
   }

   assert(epsC >= 0 && epsC <= 1.0);
   probdata->coherence = epsC;

   SCIPinfoMessage(scip, NULL, "Creating problem: %s \n", name);

   /* create variables */
   probdata->nbins=nbins;
   probdata->ncluster=ncluster;

   SCIP_CALL( createVariables(scip, probdata) );

   /* create constraints depending on model selection */

   SCIP_CALL( createProbSimplified(scip, probdata) );

   /** add callback methods to scip */
   SCIP_CALL( SCIPsetProbDelorig(scip, probdelorigSpa) );
   SCIP_CALL( SCIPsetProbCopy(scip, probcopySpa) );
   SCIP_CALL( SCIPsetProbData(scip, probdata) );
   SCIP_CALL( SCIPsetProbTrans(scip, probtransSpa) );
   SCIP_CALL( SCIPsetProbDeltrans(scip, probdeltransSpa) );

   return SCIP_OKAY;
}



/**  Getter methods for the various parts of the probdata */
SCIP_Real** SCIPspaGetCmatrix(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;

   assert(scip != NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   return probdata->cmatrix;
}

int SCIPspaGetNrBins(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip != NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   return probdata->nbins;
}

int SCIPspaGetNrCluster(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip!= NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   return probdata->ncluster;
}

SCIP_VAR*** SCIPspaGetBinvars(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip!= NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);
   assert(probdata->binvars != NULL);

   return probdata->binvars;
}

SCIP_Real SCIPspaGetCoherence(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip!= NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   return probdata->coherence;
}

SCIP_Real SCIPspaGetScale(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip!= NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   return probdata->scale;
}

SCIP_VAR**** SCIPspaGetEdgevars(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_PROBDATA* probdata;
   assert(scip!= NULL);
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);
   assert(probdata->edgevars != NULL);

   return probdata->edgevars;
}

/** print the model-values like coherence in the clusters and transition-probabilities between clusters that are not evident from the scip-solution */
SCIP_RETCODE SCIPspaPrintSolutionValues(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOL*             sol                 /**< The solution containg the values */
)
{
   SCIP_PROBDATA* probdata;
   SCIP_Real value;
   SCIP_Real objvalue = 0;
   SCIP_Real flow = 0;
   SCIP_Real coherence = 0;
   int c1;
   int c2;
   int c3;
   int i;
   int j;

   assert( scip!= NULL );
   probdata = SCIPgetProbData(scip);
   assert(probdata != NULL);

   for( c1  = 0; c1 < probdata->ncluster; ++c1 )
   {
      value = 0;
      for( i = 0; i < probdata->nbins; ++i )
      {
         for( j = 0; j < probdata->nbins; ++j )
         {
            if( j == i )
               continue;
            value+= probdata->cmatrix[i][j] * SCIPgetSolVal(scip, sol, probdata->binvars[i][c1]) * SCIPgetSolVal(scip, sol, probdata->binvars[j][c1]);
         }
      }
      SCIPinfoMessage(scip, NULL, "Coherence in cluster %d is %f \n", c1 + 1, value);
      coherence += value;
      objvalue += probdata->scale * value;
   }

   for( c1 = 0; c1 < probdata->ncluster; ++c1 )
   {
      for( c2 = 0; c2 < probdata->ncluster; ++c2 )
      {
         value = 0;
         for( i = 0; i < probdata->nbins; ++i )
         {
            for( j = 0; j < probdata->nbins; ++j )
            {
               value+= probdata->cmatrix[i][j] * SCIPgetSolVal(scip, sol, probdata->binvars[i][c1]) * SCIPgetSolVal(scip, sol, probdata->binvars[j][c2]);
            }
         }
      }
      c3 = (c1 + 1) % probdata->ncluster;
      value = 0;
      for( i = 0; i < probdata->nbins; ++i )
      {
         for( j = 0; j < probdata->nbins; ++j )
         {
            value+= (probdata->cmatrix[i][j] - probdata->cmatrix[j][i]) * SCIPgetSolVal(scip, sol, probdata->binvars[i][c1]) * SCIPgetSolVal(scip, sol, probdata->binvars[j][c3]);
         }
      }
      SCIPinfoMessage(scip, NULL, "irrev_%d_%d is %f \n", c1 + 1, (c1 + 2) % probdata->ncluster, value);
      flow += value;
      objvalue += value;
   }
   SCIPinfoMessage(scip, NULL, "objvalue is %f \n",  objvalue);
   SCIPinfoMessage(scip, NULL, "Total coherence is %f \n",  coherence);
   SCIPinfoMessage(scip, NULL, "Total net flow is %f \n",  flow);
   return SCIP_OKAY;
}

