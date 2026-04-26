#pragma once
#include <stdbool.h>
#include "domain.h"

typedef struct
{
	Type type; // the returned type
	bool lval; // true if left-value
	bool ct;   // true if constant
} Return;

// returns true if r->type can be converted
// to a scalar value: int, double, char or address
bool canBeScalar(Return *r);

// verifies if the source type can be converted to the destination type
// if yes, returns true
bool canCast(Type *src, Type *dst);

// sets in dst the resulted type of an arithmetic operation
// having as operands the types t1 and t2
// returns true if t1 and t2 can be operands for an arithmetic operation
// ex: double + int -> double
bool arithmeticCast(Type *t1, Type *t2, Type *dst);

// searches a name in a list of symbols
// if it finds it, returns the correspondent symbol, else NULL
Symbol *findSymbolInList(Symbol *list, const char *name);
