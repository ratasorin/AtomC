#include <string.h>
#include "types.h"

bool canBeScalar(Return *r)
{
	Type *t = &r->type;
	if (t->arrsize >= 0)
		return false;
	if (t->base == TB_STRUCT)
		return false;
	if (t->base == TB_VOID)
		return false;
	return true;
}

bool canCast(Type *src, Type *dest)
{
	// Arrays can only be cast to other array types, not scalars/structs
	if (src->arrsize >= 0)
	{
		if (dest->arrsize >= 0)
			return true;

		// "src" in an array, but "dest" is NOT an array
		return false;
	}

	// "src" is NOT an array, but "dest" is an array
	if (dest->arrsize >= 0)
		return false;

	// a struct can be converted only to itself
	if (src->base == TB_STRUCT && dest->base == TB_STRUCT && src->sym == dest->sym)
	{
		return true;
	}

	// "int", "char", "double" can be freely converted between them
	switch (src->base)
	{
	case TB_INT:
	case TB_DOUBLE:
	case TB_CHAR:
		switch (dest->base)
		{
		case TB_INT:
		case TB_CHAR:
		case TB_DOUBLE:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}

	return false;
}

bool arithmeticCast(Type *t1, Type *t2, Type *dest)
{
	// there are no arithmetic operations with pointers
	if (t1->arrsize >= 0 || t2->arrsize >= 0)
		return false;

	// the result of an arithmetic operation cannot be pointer or struct
	dest->sym = NULL;
	dest->arrsize = -1;
	switch (t1->base)
	{
	case TB_INT:
		switch (t2->base)
		{
		case TB_INT:
		case TB_CHAR:
			dest->base = TB_INT;
			return true;
		case TB_DOUBLE:
			dest->base = TB_DOUBLE;
			return true;
		default:
			return false;
		}
	case TB_DOUBLE:
		switch (t2->base)
		{
		case TB_INT:
		case TB_DOUBLE:
		case TB_CHAR:
			dest->base = TB_DOUBLE;
			return true;
		default:
			return false;
		}
	case TB_CHAR:
		switch (t2->base)
		{
		case TB_INT:
		case TB_DOUBLE:
		case TB_CHAR:
			dest->base = t2->base;
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}

Symbol *findSymbolInList(Symbol *list, const char *name)
{
	for (Symbol *s = list; s; s = s->next)
	{
		if (!strcmp(s->name, name))
			return s;
	}
	return NULL;
}
