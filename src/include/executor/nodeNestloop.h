/*-------------------------------------------------------------------------
 *
 * nodeNestloop.h
 *
 *
 *
 * Portions Copyright (c) 1996-2007, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODENESTLOOP_H
#define NODENESTLOOP_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsNestLoop(NestLoop *node);
extern NestLoopState *ExecInitNestLoop(NestLoop *node, EState *estate, int eflags);
extern TupleTableSlot *ExecNestLoop(NestLoopState *node);
extern void ExecEndNestLoop(NestLoopState *node);
extern void ExecReScanNestLoop(NestLoopState *node, ExprContext *exprCtxt);

#endif   /* NODENESTLOOP_H */
