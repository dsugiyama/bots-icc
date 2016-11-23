/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/**********************************************************************************************/
/*
 * Copyright (c) 2007 The Unbalanced Tree Search (UTS) Project Team:
 * -----------------------------------------------------------------
 *
 *  This file is part of the unbalanced tree search benchmark.  This
 *  project is licensed under the MIT Open Source license.  See the LICENSE
 *  file for copyright and licensing information.
 *
 *  UTS is a collaborative project between researchers at the University of
 *  Maryland, the University of North Carolina at Chapel Hill, and the Ohio
 *  State University.
 *
 * University of Maryland:
 *   Chau-Wen Tseng(1)  <tseng at cs.umd.edu>
 *
 * University of North Carolina, Chapel Hill:
 *   Jun Huan         <huan,
 *   Jinze Liu         liu,
 *   Stephen Olivier   olivier,
 *   Jan Prins*        prins at cs.umd.edu>
 *
 * The Ohio State University:
 *   James Dinan      <dinan,
 *   Gerald Sabin      sabin,
 *   P. Sadayappan*    saday at cse.ohio-state.edu>
 *
 * Supercomputing Research Center
 *   D. Pryor
 *
 * (1) - indicates project PI
 *
 * UTS Recursive Depth-First Search (DFS) version developed by James Dinan
 *
 * Adapted for OpenMP 3.0 Task-based version by Stephen Olivier
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <abt.h>
#include <sys/time.h>

#include "app-desc.h"
#include "bots.h"
#include "uts.h"

/***********************************************************
 *  Global state                                           *
 ***********************************************************/
unsigned long long nLeaves = 0;
int maxTreeDepth = 0;
/***********************************************************
 * Tree generation strategy is controlled via various      *
 * parameters set from the command line.  The parameters   *
 * and their default values are given below.               *
 * Trees are generated using a Galton-Watson process, in   *
 * which the branching factor of each node is a random     *
 * variable.                                               *
 *                                                         *
 * The random variable follow a binomial distribution.     *
 ***********************************************************/
double b_0   = 4.0; // default branching factor at the root
int   rootId = 0;   // default seed for RNG state at root
/***********************************************************
 *  The branching factor at the root is specified by b_0.
 *  The branching factor below the root follows an
 *     identical binomial distribution at all nodes.
 *  A node has m children with prob q, or no children with
 *     prob (1-q).  The expected branching factor is q * m.
 *
 *  Default parameter values
 ***********************************************************/
int    nonLeafBF   = 4;            // m
double nonLeafProb = 15.0 / 64.0;  // q
/***********************************************************
 * compute granularity - number of rng evaluations per
 * tree node
 ***********************************************************/
int computeGranularity = 1;
/***********************************************************
 * expected results for execution
 ***********************************************************/
unsigned long long  exp_tree_size = 0;
int        exp_tree_depth = 0;
unsigned long long  exp_num_leaves = 0;
/***********************************************************
 *  FUNCTIONS                                              *
 ***********************************************************/

// Interpret 32 bit positive integer as value on [0,1)
double rng_toProb(int n)
{
  if (n < 0) {
    printf("*** toProb: rand n = %d out of range\n",n);
  }
  return ((n<0)? 0.0 : ((double) n)/2147483648.0);
}

void uts_initRoot(Node * root)
{
   root->height = 0;
   root->numChildren = -1;      // means not yet determined
   rng_init(root->state.state, rootId);

   bots_message("Root node at %p\n", root);
}


int uts_numChildren_bin(Node * parent)
{
  // distribution is identical everywhere below root
  int    v = rng_rand(parent->state.state);
  double d = rng_toProb(v);

  return (d < nonLeafProb) ? nonLeafBF : 0;
}

int uts_numChildren(Node *parent)
{
  int numChildren = 0;

  /* Determine the number of children */
  if (parent->height == 0) numChildren = (int) floor(b_0);
  else numChildren = uts_numChildren_bin(parent);

  // limit number of children
  // only a BIN root can have more than MAXNUMCHILDREN
  if (parent->height == 0) {
    int rootBF = (int) ceil(b_0);
    if (numChildren > rootBF) {
      bots_debug("*** Number of children of root truncated from %d to %d\n", numChildren, rootBF);
      numChildren = rootBF;
    }
  }
  else {
    if (numChildren > MAXNUMCHILDREN) {
      bots_debug("*** Number of children truncated from %d to %d\n", numChildren, MAXNUMCHILDREN);
      numChildren = MAXNUMCHILDREN;
    }
  }

  return numChildren;
}

/***********************************************************
 * Recursive depth-first implementation                    *
 ***********************************************************/

ABT_xstream xstreams[256];

void setup_abt(void)
{
    ABT_init(0, NULL);

    const char *env = getenv("ABT_NUM_ES");
    int num_threads = atoi(env);

    ABT_pool pools[num_threads];
    for (int i = 0; i < num_threads; i++) {
        ABT_pool_create_basic(ABT_POOL_DEQUE, ABT_POOL_ACCESS_SPMC, ABT_TRUE, &pools[i]);
    }

    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_RANDWS, num_threads, pools);

    for (int i = 1; i < num_threads; i++) {
        ABT_pool tmp;
        tmp = pools[0];
        pools[0] = pools[i];
        pools[i] = tmp;
        ABT_xstream_create_basic(ABT_SCHED_RANDWS, num_threads, pools, ABT_SCHED_CONFIG_NULL, &xstreams[i]);
    }
}

struct args_ret
{
    int depth;
    Node *parent;
    int numChildren;
    unsigned long long *num_nodes;
};

unsigned long long parallel_uts ( Node *root )
{
   setup_abt();

   unsigned long long num_nodes = 0 ;
   root->numChildren = uts_numChildren(root);

   bots_message("Computing Unbalance Tree Search algorithm ");

   num_nodes = parTreeSearch( 0, root, root->numChildren );

   bots_message(" completed!");

   return num_nodes;
}

struct divconq_args
{
    void (*loop_func)(int from, int to_exclusive, void *args);
    int from;
    int to_exclusive;
    void *func_args;
};

void loop_divide_conquer(struct divconq_args *a)
{
    if (a->to_exclusive - a->from <= 1) {
        a->loop_func(a->from, a->to_exclusive, a->func_args);
        return;
    }

    struct divconq_args args_call = {
        .loop_func = a->loop_func,
        .from = a->from,
        .to_exclusive = a->from + ((a->to_exclusive - a->from) >> 1),
        .func_args = a->func_args
    };
    struct divconq_args args_spawn = {
        .loop_func = a->loop_func,
        .from = args_call.to_exclusive,
        .to_exclusive = a->to_exclusive,
        .func_args = a->func_args
    };

    ABT_xstream self;
    ABT_xstream_self(&self);
    ABT_thread spawn_thread;
    ABT_thread_create_on_xstream(self, loop_divide_conquer, &args_spawn, ABT_THREAD_ATTR_NULL, &spawn_thread);

    loop_divide_conquer(&args_call);
    ABT_thread_join(spawn_thread);
    ABT_thread_free(&spawn_thread);
}

struct loop_args
{
    Node *n;
    Node *parent;
    unsigned long long *partialCount;
    int depth;
};

void parTreeSearch_loop(int from, int to_exclusive, struct loop_args *a)
{
    Node *n = a->n;
    Node *parent = a->parent;
    unsigned long long *partialCount = a->partialCount;
    int depth = a->depth;

    for (int i = from; i < to_exclusive; i++) {
        Node *nodePtr = &n[i];

        nodePtr->height = parent->height + 1;

        // The following line is the work (one or more SHA-1 ops)
        for (int j = 0; j < computeGranularity; j++) {
            rng_spawn(parent->state.state, nodePtr->state.state, i);
        }

        nodePtr->numChildren = uts_numChildren(nodePtr);

        partialCount[i] = parTreeSearch(depth+1, nodePtr, nodePtr->numChildren);
    }
}

unsigned long long parTreeSearch(int depth, Node *parent, int numChildren)
{
  Node n[numChildren], *nodePtr;
  int i, j;
  unsigned long long subtreesize = 1, partialCount[numChildren];

  // Recurse on the children
  struct loop_args la = { .n = n, .parent = parent, .partialCount = partialCount, .depth = depth };
  struct divconq_args dca = {
      .loop_func = parTreeSearch_loop,
      .from = 0,
      .to_exclusive = numChildren,
      .func_args = &la
  };
  loop_divide_conquer(&dca);

  for (i = 0; i < numChildren; i++) {
     subtreesize += partialCount[i];
  }

  return subtreesize;
}

void uts_read_file ( char *filename )
{
   FILE *fin;

   if ((fin = fopen(filename, "r")) == NULL) {
      bots_message("Could not open input file (%s)\n", filename);
      exit (-1);
   }
   fscanf(fin,"%lf %lf %d %d %d %llu %d %llu",
             &b_0,
             &nonLeafProb,
             &nonLeafBF,
             &rootId,
             &computeGranularity,
             &exp_tree_size,
             &exp_tree_depth,
             &exp_num_leaves
   );
   fclose(fin);

   computeGranularity = max(1,computeGranularity);

   // Printing input data
   bots_message("\n");
   bots_message("Root branching factor                = %f\n", b_0);
   bots_message("Root seed (0 <= 2^31)                = %d\n", rootId);
   bots_message("Probability of non-leaf node         = %f\n", nonLeafProb);
   bots_message("Number of children for non-leaf node = %d\n", nonLeafBF);
   bots_message("E(n)                                 = %f\n", (double) ( nonLeafProb * nonLeafBF ) );
   bots_message("E(s)                                 = %f\n", (double) ( 1.0 / (1.0 - nonLeafProb * nonLeafBF) ) );
   bots_message("Compute granularity                  = %d\n", computeGranularity);
   bots_message("Random number generator              = "); rng_showtype();
}

void uts_show_stats( void )
{
   int nPes = atoi(bots_resources);
   int chunkSize = 0;

   bots_message("\n");
   bots_message("Tree size                            = %llu\n", (unsigned long long)  bots_number_of_tasks );
   bots_message("Maximum tree depth                   = %d\n", maxTreeDepth );
   bots_message("Chunk size                           = %d\n", chunkSize );
   bots_message("Number of leaves                     = %llu (%.2f%%)\n", nLeaves, nLeaves/(float)bots_number_of_tasks*100.0 );
   bots_message("Number of PE's                       = %.4d threads\n", nPes );
   bots_message("Wallclock time                       = %.3f sec\n", bots_time_program );
   bots_message("Overall performance                  = %.0f nodes/sec\n", (bots_number_of_tasks / bots_time_program) );
   bots_message("Performance per PE                   = %.0f nodes/sec\n", (bots_number_of_tasks / bots_time_program / nPes) );
}

int uts_check_result ( void )
{
   int answer = BOTS_RESULT_SUCCESSFUL;

   if ( bots_number_of_tasks != exp_tree_size ) {
      answer = BOTS_RESULT_UNSUCCESSFUL;
      bots_message("Incorrect tree size result (%llu instead of %llu).\n", bots_number_of_tasks, exp_tree_size);
   }

   return answer;
}
