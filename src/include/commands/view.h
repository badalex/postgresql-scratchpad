/*-------------------------------------------------------------------------
 *
 * view.h
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
#ifndef VIEW_H
#define VIEW_H

#include "nodes/parsenodes.h"

extern void DefineView(RangeVar *view, Query *view_parse, bool replace);
extern void RemoveView(const RangeVar *view, DropBehavior behavior);

#endif   /* VIEW_H */
