/*-------------------------------------------------------------------------
 *
 * format_type.c
 *	  Display type names "nicely".
 *
 *
 * Portions Copyright (c) 1996-2000, PostgreSQL, Inc
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $Header$
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "fmgr.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/syscache.h"

#define MAX_INT32_LEN 11
#define _textin(str) DirectFunctionCall1(textin, CStringGetDatum(str))


static char *format_type_internal(Oid type_oid, int32 typemod);


static char *
psnprintf(size_t len, const char * fmt, ...)
{
	va_list ap;
	char * buf;

	buf = palloc(len);

	va_start(ap, fmt);
	vsnprintf(buf, len, fmt, ap);
	va_end(ap);

	return buf;
}


/*
 * SQL function: format_type(type_oid, typemod)
 *
 * `type_oid' is from pg_type.oid, `typemod' is from
 * pg_attribute.atttypmod. This function will get the type name and
 * format it and the modifier to canonical SQL format, if the type is
 * a standard type. Otherwise you just get pg_type.typname back,
 * double quoted if it contains funny characters.
 *
 * If typemod is null (in the SQL sense) then you won't get any
 * "..(x)" type qualifiers. The result is not technically correct,
 * because the various types interpret missing type modifiers
 * differently, but it can be used as a convenient way to format
 * system catalogs, e.g., pg_aggregate, in psql.
 */
Datum
format_type(PG_FUNCTION_ARGS)
{
	Oid			type_oid;
	int32		typemod;
	char	   *result;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	type_oid = PG_GETARG_OID(0);

	if (!PG_ARGISNULL(1))
		typemod = PG_GETARG_INT32(1);
	else
		typemod = -1;			/* default typmod */

	result = format_type_internal(type_oid, typemod);

	PG_RETURN_DATUM(_textin(result));
}


static char *
format_type_internal(Oid type_oid, int32 typemod)
{
	bool		with_typemod = (typemod >= 0);
	HeapTuple	tuple;
	Oid			array_base_type;
	int16		typlen;
	bool		is_array;
	char	   *name;
	char	   *buf;

	if (type_oid == InvalidOid)
		return pstrdup("-");

	tuple = SearchSysCacheTuple(TYPEOID, ObjectIdGetDatum(type_oid),
								0, 0, 0);

	if (!HeapTupleIsValid(tuple))
		return pstrdup("???");

	array_base_type = ((Form_pg_type) GETSTRUCT(tuple))->typelem;
	typlen = ((Form_pg_type) GETSTRUCT(tuple))->typlen;
	if (array_base_type != 0 && typlen < 0)
	{
		tuple = SearchSysCacheTuple(TYPEOID,
									ObjectIdGetDatum(array_base_type),
									0, 0, 0);
		if (!HeapTupleIsValid(tuple))
			return pstrdup("???[]");
		is_array = true;
		type_oid = array_base_type;
	}
	else
		is_array = false;

	switch (type_oid)
	{
		case BOOLOID:
			buf = pstrdup("boolean");
			break;

		case BPCHAROID:
			if (with_typemod)
				buf = psnprintf(11 + MAX_INT32_LEN + 1, "character(%d)",
								(int) (typemod - VARHDRSZ));
			else
				buf = pstrdup("character");
			break;

		case CHAROID:
			/* This char type is the single-byte version. You have to
			 * double-quote it to get at it in the parser.
			 */
			buf = pstrdup("\"char\"");
			break;

		case FLOAT4OID:
			buf = pstrdup("real");
			break;

		case FLOAT8OID:
			buf = pstrdup("double precision");
			break;

		case INT2OID:
			buf = pstrdup("smallint");
			break;

		case INT4OID:
			buf = pstrdup("integer");
			break;

		case INT8OID:
			buf = pstrdup("bigint");
			break;

		case NUMERICOID:
			if (with_typemod)
				buf = psnprintf(10 + 2 * MAX_INT32_LEN + 1, "numeric(%d,%d)",
								((typemod - VARHDRSZ) >> 16) & 0xffff,
								(typemod - VARHDRSZ) & 0xffff);
			else
				buf = pstrdup("numeric");
			break;

		case TIMETZOID:
			buf = pstrdup("time with time zone");
			break;

		case VARBITOID:
			if (with_typemod)
				buf = psnprintf(13 + MAX_INT32_LEN + 1, "bit varying(%d)",
								(int) typemod);
			else
				buf = pstrdup("bit varying");
			break;

		case VARCHAROID:
			if (with_typemod)
				buf = psnprintf(19 + MAX_INT32_LEN + 1,
								"character varying(%d)",
								(int) (typemod - VARHDRSZ));
			else
				buf = pstrdup("character varying");
			break;

		case ZPBITOID:
			if (with_typemod)
				buf = psnprintf(5 + MAX_INT32_LEN + 1, "bit(%d)",
								(int) typemod);
			else
				buf = pstrdup("bit");
			break;

		default:
			name = NameStr(((Form_pg_type) GETSTRUCT(tuple))->typname);
			if (strspn(name, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(name)
				|| isdigit((int) name[0]))
				buf = psnprintf(strlen(name) + 3, "\"%s\"", name);
			else
				buf = pstrdup(name);
			break;
	}

	if (is_array)
		buf = psnprintf(strlen(buf) + 3, "%s[]", buf);

	return buf;
}



/*
 * oidvectortypes			- converts a vector of type OIDs to "typname" list
 *
 * The interface for this function is wrong: it should be told how many
 * OIDs are significant in the input vector, so that trailing InvalidOid
 * argument types can be recognized.
 */
Datum
oidvectortypes(PG_FUNCTION_ARGS)
{
	Oid		   *oidArray = (Oid *) PG_GETARG_POINTER(0);
	char	   *result;
	int			numargs;
	int			num;
	size_t		total;
	size_t		left;

	/* Try to guess how many args there are :-( */
	numargs = 0;
	for (num = 0; num < FUNC_MAX_ARGS; num++)
	{
		if (oidArray[num] != InvalidOid)
			numargs = num + 1;
	}

	total = 20 * numargs + 1;
	result = palloc(total);
	result[0] = '\0';
	left = total - 1;
					
	for (num = 0; num < numargs; num++)
	{
		char * typename = format_type_internal(oidArray[num], -1);

		if (left < strlen(typename) + 2)
		{
			total += strlen(typename) + 2;
			result = repalloc(result, total);
			left += strlen(typename) + 2;
		}

		if (num > 0)
		{
			strcat(result, ", ");
			left -= 2;
		}
		strcat(result, typename);
		left -= strlen(typename);
	}

	PG_RETURN_DATUM(_textin(result));
}
