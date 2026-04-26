#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "domain.h"
#include "types.h"
#include "parser.h"
#include "utils.h"

/**
 Position reached in the list: `tokens` extracted by `lexer.c`

 If the program's syntax is correct, `current_tok` should advance to END

 Example of failure point inside an `ifExpression` function:
```
	tokens: [ IF ] → [ L_PARENTHESIS ] → [ EXPR ] → [ R_BRACKET ] → ... → END

	current_tok: [ IF ]
	ifExpression() @ consume(IF_CODE) = true

	current_tok: [ L_PARENTHESIS ]
	ifExpression() @ consume(L_PARENTHESIS_CODE) = true

	current_tok: [ EXPR ]
	ifExpression() @ consume(EXPR_CODE) = true

	current_tok: [ R_BRACKET ]
	ifExpression() @ consume(R_PARENTHESIS_CODE) = false → Error
```
 */
Token *current_tok;
Token *last_consumed_tok; // the last consumed token

/**
  - structDef() sets `owner = s` after creating a struct symbol.
  - functionDef() sets `owner = fn`after creating a function symbol.
  - They both clear it back to `NULL` when leaving that declaration.
 */
Symbol *owner = NULL;

static Return makeRet(Type type, bool lval, bool ct)
{
	Return r;
	r.type = type;
	r.lval = lval;
	r.ct = ct;
	return r;
}

static bool canCastForCastExpr(Type *src, Type *dst)
{
	if (src->base == TB_STRUCT || dst->base == TB_STRUCT)
		return false;
	return canCast(src, dst);
}

void parseErr(const char *fmt, ...)
{
	int line = 0;
	if (current_tok)
		line = current_tok->line_num;
	else if (last_consumed_tok)
		line = last_consumed_tok->line_num;

	fprintf(stderr, "[Parser] At line %d: ", line);
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

static const char *tokenText(int code)
{
	switch (code)
	{
	case COMMA:
		return ",";
	case SEMICOLON:
		return ";";
	case L_PARENTHESES:
		return "(";
	case R_PARENTHESES:
		return ")";
	case L_BRACKET:
		return "[";
	case R_BRACKET:
		return "]";
	case L_ACCOLADE:
		return "{";
	case R_ACCOLADE:
		return "}";
	case DOT:
		return ".";
	case IF:
		return "if";
	case ELSE:
		return "else";
	case WHILE:
		return "while";
	case RETURN:
		return "return";
	case STRUCT:
		return "struct";
	case ASSIGN:
		return "=";
	case OR:
		return "||";
	case AND:
		return "&&";
	case EQUAL:
		return "==";
	case NOT_EQ:
		return "!=";
	case LESS:
		return "<";
	case LESS_EQ:
		return "<=";
	case GREATER:
		return ">";
	case GREATER_EQ:
		return ">=";
	case ADD:
		return "+";
	case SUB:
		return "-";
	case MUL:
		return "*";
	case DIV:
		return "/";
	case NOT:
		return "!";
	default:
		return "token";
	}
}

static void parseErrMissingAfterToken(int missingCode, int afterCode)
{
	parseErr("missing `%s` after `%s`", tokenText(missingCode), tokenText(afterCode));
}

static void parseErrMissingName(const char *kind)
{
	parseErr("missing %s name", kind);
}

static bool exprAssignR(Return *r);

/**
	Used by various checkers: `ifExpression`, `funcDefinition`, `structDefinition` to iteratively validate their
	specific sequence of tokens against the global `current_tok`

	Example usage:
```C
	bool ifExpression() {
		if(consume(IF))
		{
			if(consume(L_PARENTHESIS))
			{
				if(expression())
				{
					if(consume(R_PARENTHESIS))
					{
						return true;
					}
				}
			}
		}

		return false;
	}
```
	Parameter: `code` - The value that a checker expects `current_tok` to be
	Returns: `boolean` - "true" if the expectation is met
 */
bool consume(int code)
{
	if (current_tok->code == code)
	{
		last_consumed_tok = current_tok;
		current_tok = current_tok->next;
		return true;
	}
	return false;
}

/*
	Behavior:
		- Accepts primitive types and defined struct types

	Grammar rule:
```
	[typeBase] := [TYPE_INT] | [TYPE_DOUBLE] | [TYPE_CHAR] | [STRUCT] [ID]
```
*/
bool typeBase(Type *t)
{
	t->arrsize = -1;
	t->sym = NULL;

	if (consume(TYPE_INT))
	{
		t->base = TB_INT;
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		t->base = TB_DOUBLE;
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		t->base = TB_CHAR;
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *tkName = last_consumed_tok;
			t->base = TB_STRUCT;
			t->sym = findSymbol(tkName->text);
			if (!t->sym || t->sym->kind != SK_STRUCT)
				parseErr("undefined structure: %s", tkName->text);
			return true;
		}
		parseErrMissingName("struct");
	}
	return false;
}

/*
	Grammar rule:
```
	[arrayDecl[inout Type *t]] := [L_BRACKET] [INT]? [R_BRACKET]
```
*/
bool arrayDecl(Type *t)
{
	if (consume(L_BRACKET))
	{
		if (consume(INT))
		{
			Token *tkSize = last_consumed_tok;
			t->arrsize = tkSize->i;
		}
		else
		{
			t->arrsize = 0;
		}
		if (consume(R_BRACKET))
		{
			return true;
		}
		parseErr("missing `%s` after array size", tokenText(R_BRACKET));
	}
	return false;
}

/*
	Grammar rule:
```
		[varDef] := {Type t;} [typeBase[&t]] [ID[tkName]] ( [arrayDecl[&t]] )? [SEMICOLON]
```
*/
bool variableDef()
{
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *tkName = last_consumed_tok;
			if (arrayDecl(&t))
			{
				if (t.arrsize == 0)
					parseErr("array variable declarations need an explicit size");
			}
			if (consume(SEMICOLON))
			{
				Symbol *var = findSymbolInDomain(currentDomain, tkName->text);
				if (var)
					parseErr("symbol redefinition: %s", tkName->text);
				var = newSymbol(tkName->text, SK_VAR);
				var->type = t;
				var->owner = owner;
				addSymbolToDomain(currentDomain, var);
				if (owner)
				{
					switch (owner->kind)
					{
					case SK_FN:
						var->varIdx = symbolsLen(owner->fn.locals);
						addSymbolToList(&owner->fn.locals, dupSymbol(var));
						break;
					case SK_STRUCT:
						var->varIdx = typeSize(&owner->type);
						addSymbolToList(&owner->structMembers, dupSymbol(var));
						break;
					default:
						break;
					}
				}
				else
				{
					var->varMem = safeAlloc(typeSize(&t));
				}
				return true;
			}
			parseErr("missing `%s` after variable declaration", tokenText(SEMICOLON));
		}
		parseErrMissingName("variable");
	}
	return false;
}

/*
	Grammar rule:
```
	[structDef] := [STRUCT] [ID[tkName]] [L_ACCOLADE] [varDef]* [R_ACCOLADE] [SEMICOLON]
```
*/
bool structDef()
{
	Token *start = current_tok;
	if (consume(STRUCT))
	{
		if (!consume(ID))
			parseErrMissingName("struct");
		Token *tkName = last_consumed_tok;
		if (consume(L_ACCOLADE))
		{
		}
		else
		{
			current_tok = start;
			return false;
		}
		Symbol *s = findSymbolInDomain(currentDomain, tkName->text);
		if (s)
			parseErr("symbol redefinition: %s", tkName->text);
		s = addSymbolToDomain(currentDomain, newSymbol(tkName->text, SK_STRUCT));
		s->type.base = TB_STRUCT;
		s->type.sym = s;
		s->type.arrsize = -1;
		pushDomain();
		owner = s;

		while (variableDef())
		{
		}

		if (!consume(R_ACCOLADE))
			parseErr("missing `%s` at end of struct declaration", tokenText(R_ACCOLADE));
		if (!consume(SEMICOLON))
			parseErr("missing `%s` after struct declaration", tokenText(SEMICOLON));
		owner = NULL;
		dropDomain();
		return true;
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[fnParam] := {Type t;} [typeBase[&t]] [ID[tkName]] ( [arrayDecl[&t]] {t.arrsize = 0;} )?
```
*/
bool fnParam()
{
	Type t;
	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *tkName = last_consumed_tok;
			if (arrayDecl(&t))
				t.arrsize = 0;
			Symbol *param = findSymbolInDomain(currentDomain, tkName->text);
			if (param)
				parseErr("symbol redefinition: %s", tkName->text);
			param = newSymbol(tkName->text, SK_PARAM);
			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			addSymbolToDomain(currentDomain, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
			return true;
		}
		parseErrMissingName("parameter");
	}
	return false;
}

/*
	Grammar rule:
```
	[fnDef] := {Type t;}
				( [typeBase[&t]] | [VOID] {t.base = TB_VOID; t.sym = NULL; t.arrsize = -1;} )
				[ID[tkName]]
				[L_PARENTHESES]
				( [fnParam] ( [COMMA] [fnParam] )* )? [R_PARENTHESES]
				[stmCompound[false]]
```
*/
bool functionDef()
{
	Token *start = current_tok;
	Type t;
	bool isVoid = false;
	if (typeBase(&t) || (isVoid = consume(VOID)))
	{
		// Treat `void` in a special way because it is not a regular type for standard variables: TYPE_INT | TYPE_CHAR | TYPE_DOUBLE | STRUCT
		if (isVoid)
		{
			t.base = TB_VOID;
			t.sym = NULL;
			t.arrsize = -1;
		}

		if (!consume(ID))
			parseErrMissingName("function");
		Token *tkName = last_consumed_tok;

		// [varDef]/[fnDef] disambiguation: if there is no `(`, this is not [fnDef].
		if (!consume(L_PARENTHESES))
		{
			// "All Or Nothing" policy: This is not a [fnDef], revert so we can find the [varDef]
			current_tok = start;
			return false;
		}

		Symbol *fn = findSymbolInDomain(currentDomain, tkName->text);
		if (fn)
			parseErr("symbol redefinition: %s", tkName->text);
		fn = newSymbol(tkName->text, SK_FN);
		fn->type = t;
		addSymbolToDomain(currentDomain, fn);
		owner = fn;
		pushDomain();

		if (fnParam())
		{
			while (consume(COMMA))
			{
				if (!fnParam())
					parseErr("missing/invalid parameter after `%s`", tokenText(COMMA));
			}
		}

		if (!consume(R_PARENTHESES))
			parseErr("missing `%s` after function parameters", tokenText(R_PARENTHESES));
		if (!stmCompound(false))
			parseErr("missing function body after parameter list");
		dropDomain();
		owner = NULL;
		return true;
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[stm] := [stmCompound]
		 | [IF] [L_PARENTHESES] [expr] [R_PARENTHESES] [stm] ( [ELSE] [stm] )?
		 | [WHILE] [L_PARENTHESES] [expr] [R_PARENTHESES] [stm]
		 | [RETURN] [expr]? [SEMICOLON]
		 | [expr]? [SEMICOLON]
```
*/
bool stm()
{
	Token *start = current_tok;
	if (stmCompound(true))
		return true;

	current_tok = start;
	if (consume(IF))
	{
		if (!consume(L_PARENTHESES))
			parseErrMissingAfterToken(L_PARENTHESES, IF);
		Return rCond;
		if (!exprAssignR(&rCond))
			parseErr("missing or invalid condition after `%s`", tokenText(IF));
		if (!canBeScalar(&rCond))
			parseErr("the if condition must be a scalar value");
		if (!consume(R_PARENTHESES))
			parseErr("missing `%s` after `if` condition", tokenText(R_PARENTHESES));
		if (!stm())
			parseErr("missing statement after `%s`", tokenText(IF));
		if (consume(ELSE))
		{
			if (!stm())
				parseErr("missing statement after `%s`", tokenText(ELSE));
		}
		return true;
	}

	current_tok = start;
	if (consume(WHILE))
	{
		if (!consume(L_PARENTHESES))
			parseErrMissingAfterToken(L_PARENTHESES, WHILE);
		Return rCond;
		if (!exprAssignR(&rCond))
			parseErr("missing or invalid condition after `%s`", tokenText(WHILE));
		if (!canBeScalar(&rCond))
			parseErr("the while condition must be a scalar value");
		if (!consume(R_PARENTHESES))
			parseErr("missing `%s` after `while` condition", tokenText(R_PARENTHESES));
		if (!stm())
			parseErr("missing statement after `%s`", tokenText(WHILE));
		return true;
	}

	current_tok = start;
	if (consume(RETURN))
	{
		if (consume(SEMICOLON))
		{
			if (owner->type.base != TB_VOID)
				parseErr("a non-void function must return a value");
			return true;
		}

		Return rExpr;
		if (!exprAssignR(&rExpr))
			parseErr("missing expression after `%s`", tokenText(RETURN));
		if (owner->type.base == TB_VOID)
			parseErr("a void function cannot return a value");
		if (!canBeScalar(&rExpr))
			parseErr("the return value must be a scalar value");
		if (!canCast(&rExpr.type, &owner->type))
			parseErr("cannot convert the return expression type to the function return type");
		if (!consume(SEMICOLON))
			parseErrMissingAfterToken(SEMICOLON, RETURN);
		return true;
	}

	current_tok = start;
	{
		Return rExpr;
		exprAssignR(&rExpr);
	}
	if (consume(SEMICOLON))
		return true;

	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[stmCompound[in bool newDomain]] := [L_ACCOLADE] ( [varDef] | [stm] )* [R_ACCOLADE]
```
*/
bool stmCompound(bool newDomain)
{
	Token *start = current_tok;
	if (consume(L_ACCOLADE))
	{
		if (newDomain)
			pushDomain();
		while (true)
		{
			if (variableDef())
			{
			}
			else if (stm())
			{
			}
			else
				break;
		}
		if (!consume(R_ACCOLADE))
			parseErr("missing `%s` at end of block", tokenText(R_ACCOLADE));
		if (newDomain)
			dropDomain();
		return true;
	}
	current_tok = start;
	return false;
}

// Internal helpers for the left-recursion eliminated expression rules.
static bool exprAssignR(Return *r);
static bool exprOrR(Return *r);
static bool exprOrRestR(Return *r);
static bool exprAndR(Return *r);
static bool exprAndRestR(Return *r);
static bool exprEqR(Return *r);
static bool exprEqRestR(Return *r);
static bool exprRelR(Return *r);
static bool exprRelRestR(Return *r);
static bool exprAddR(Return *r);
static bool exprAddRestR(Return *r);
static bool exprMulR(Return *r);
static bool exprMulRestR(Return *r);
static bool exprCastR(Return *r);
static bool exprUnaryR(Return *r);
static bool exprPostfixR(Return *r);
static bool exprPostfixRestR(Return *r);
static bool exprPrimaryR(Return *r);

bool expr()
{
	Return r;
	return exprAssignR(&r);
}

/*
	Grammar rule:
```
	[exprAssign] := [exprUnary] [ASSIGN] [exprAssign] | [exprOr]
```
*/
bool exprAssign()
{
	Return r;
	return exprAssignR(&r);
}

/*
	Grammar Rule:
```
	[exprOr] := [exprOr] [OR] [exprAnd] | [exprAnd]

	[exprOr] := [exprAnd] [exprOrRest]
	[exprOrRest] := [OR] [exprAnd] [exprOrRest] | E
```
*/
bool exprOr()
{
	Return r;
	return exprOrR(&r);
}

static bool exprOrR(Return *r)
{
	if (!exprAndR(r))
		return false;
	return exprOrRestR(r);
}

static bool exprOrRestR(Return *r)
{
	if (consume(OR))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprAndR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for ||");
		*r = makeRet((Type){TB_INT, NULL, -1}, false, true);
		return exprOrRestR(r);
	}
	return true;
}

/*
	Grammar Rule:
```
	[exprAnd] := [exprAnd] [AND] [exprEq] | [exprEq]

	[exprAnd] := [exprEq] [exprAndRest]
	[exprAndRest] := [AND] [exprEq] [exprAndRest] | E
```
*/
bool exprAnd()
{
	Return r;
	return exprAndR(&r);
}

static bool exprAndR(Return *r)
{
	if (!exprEqR(r))
		return false;
	return exprAndRestR(r);
}

static bool exprAndRestR(Return *r)
{
	if (consume(AND))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprEqR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for &&");
		*r = makeRet((Type){TB_INT, NULL, -1}, false, true);
		return exprAndRestR(r);
	}
	return true;
}

/*
	Grammar Rule:
```
	[exprEq] := [exprEq] ([EQUAL] | [NOT_EQ]) [exprRel] | [exprRel]

	[exprEq] := [exprRel] [exprEqRest]
	[exprEqRest] := ([EQUAL] | [NOT_EQ]) [exprRel] [exprEqRest] | E
```
*/
bool exprEq()
{
	Return r;
	return exprEqR(&r);
}

static bool exprEqR(Return *r)
{
	if (!exprRelR(r))
		return false;
	return exprEqRestR(r);
}

static bool exprEqRestR(Return *r)
{
	if (consume(EQUAL) || consume(NOT_EQ))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprRelR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for == or !=");
		*r = makeRet((Type){TB_INT, NULL, -1}, false, true);
		return exprEqRestR(r);
	}
	return true;
}

/*
	Grammar Rule:
```
	[exprRel] := [exprRel] ([LESS] | [LESS_EQ] | [GREATER] | [GREATER_EQ]) [exprAdd] | [exprAdd]

	[exprRel] := [exprAdd] [exprRelRest]
	[exprRelRest] := ([LESS] | [LESS_EQ] | [GREATER] | [GREATER_EQ]) [exprAdd] [exprRelRest] | E
```
*/
bool exprRel()
{
	Return r;
	return exprRelR(&r);
}

static bool exprRelR(Return *r)
{
	if (!exprAddR(r))
		return false;
	return exprRelRestR(r);
}

static bool exprRelRestR(Return *r)
{
	if (consume(LESS) || consume(LESS_EQ) || consume(GREATER) || consume(GREATER_EQ))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprAddR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for <, <=, >, >=");
		*r = makeRet((Type){TB_INT, NULL, -1}, false, true);
		return exprRelRestR(r);
	}
	return true;
}

/*
	Grammar Rule:
```
	[exprAdd] := [exprAdd] ([ADD] | [SUB]) [exprMul] | [exprMul]

	[exprAdd] := [exprMul] [exprAddRest]
	[exprAddRest] := ([ADD] | [SUB]) [exprMul] [exprAddRest] | E
```
*/
bool exprAdd()
{
	Return r;
	return exprAddR(&r);
}

static bool exprAddR(Return *r)
{
	if (!exprMulR(r))
		return false;
	return exprAddRestR(r);
}

static bool exprAddRestR(Return *r)
{
	if (consume(ADD) || consume(SUB))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprMulR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for + or -");
		*r = makeRet(tDst, false, true);
		return exprAddRestR(r);
	}
	return true;
}

/*
	Grammar Rule:
```
	[exprMul] := [exprMul] ([MUL] | [DIV]) [exprCast] | [exprCast]

	[exprMul] := [exprCast] [exprMulRest]
	[exprMulRest] := ([MUL] | [DIV]) [exprCast] [exprMulRest] | E
```
*/
bool exprMul()
{
	Return r;
	return exprMulR(&r);
}

static bool exprMulR(Return *r)
{
	if (!exprCastR(r))
		return false;
	return exprMulRestR(r);
}

static bool exprMulRestR(Return *r)
{
	if (consume(MUL) || consume(DIV))
	{
		int op = last_consumed_tok->code;
		Return right;
		if (!exprCastR(&right))
			parseErr("missing expression after `%s`", tokenText(op));
		Type tDst;
		if (!arithmeticCast(&r->type, &right.type, &tDst))
			parseErr("invalid operand type for * or /");
		*r = makeRet(tDst, false, true);
		return exprMulRestR(r);
	}
	return true;
}

/*
	Grammar rule:
```
	[exprCast] := [L_PARENTHESES] {Type t;} [typeBase] [arrayDecl]? [R_PARENTHESES] [exprCast] | [exprUnary]
```
*/
bool exprCast()
{
	Return r;
	return exprCastR(&r);
}

static bool exprCastR(Return *r)
{
	Token *start = current_tok;
	if (consume(L_PARENTHESES))
	{
		Type t;
		if (typeBase(&t))
		{
			arrayDecl(&t);
			if (!consume(R_PARENTHESES))
				parseErr("missing `%s` in cast expression", tokenText(R_PARENTHESES));
			Return op;
			if (!exprCastR(&op))
				parseErr("missing expression after cast type");
			if (op.type.base == TB_STRUCT)
				parseErr("cannot convert a struct");
			if (t.base == TB_STRUCT)
				parseErr("cannot convert to a struct type");
			if (op.type.arrsize >= 0 && t.arrsize < 0)
				parseErr("an array can be converted only to another array");
			if (op.type.arrsize < 0 && t.arrsize >= 0)
				parseErr("a scalar can be converted only to another scalar");
			if (!canCastForCastExpr(&op.type, &t))
				parseErr("invalid cast");
			*r = makeRet(t, false, true);
			return true;
		}
	}

	current_tok = start;
	return exprUnaryR(r);
}

/*
	Grammar rule:
```
	[exprUnary] := ( [SUB] | [NOT] ) [exprUnary] | [exprPostfix]
```
*/
bool exprUnary()
{
	Return r;
	return exprUnaryR(&r);
}

static bool exprUnaryR(Return *r)
{
	Token *start = current_tok;
	if (consume(SUB) || consume(NOT))
	{
		int op = last_consumed_tok->code;
		Return operand;
		if (!exprUnaryR(&operand))
			parseErr("missing unary expression after `%s`", tokenText(op));
		if (!canBeScalar(&operand))
			parseErr("unary - or ! must have a scalar operand");
		*r = operand;
		r->lval = false;
		r->ct = true;
		if (op == NOT)
			r->type = (Type){TB_INT, NULL, -1};
		return true;
	}

	current_tok = start;
	return exprPostfixR(r);
}

/*
	Grammar Rule:
```
	[exprPostfix] := [exprPostfix] [L_BRACKET] [expr] [R_BRACKET]
			 | [exprPostfix] [DOT] [ID]
			 | [exprPrimary]

	[exprPostfix] := [exprPrimary] [exprPostfixRest]
	[exprPostfixRest] := ([DOT] [ID] | [L_BRACKET] [expr] [R_BRACKET]) [exprPostfixRest] | E
```
*/
bool exprPostfix()
{
	Return r;
	return exprPostfixR(&r);
}

static bool exprPostfixR(Return *r)
{
	if (!exprPrimaryR(r))
		return false;
	return exprPostfixRestR(r);
}

static bool exprPostfixRestR(Return *r)
{
	if (consume(L_BRACKET))
	{
		Return idx;
		if (!exprAssignR(&idx))
			parseErr("missing index expression after `%s`", tokenText(L_BRACKET));
		if (!consume(R_BRACKET))
			parseErr("missing `%s` after index expression", tokenText(R_BRACKET));
		if (r->type.arrsize < 0)
			parseErr("only an array can be indexed");
		Type tInt = {TB_INT, NULL, -1};
		if (!canCast(&idx.type, &tInt))
			parseErr("the index is not convertible to int");
		r->type.arrsize = -1;
		r->lval = true;
		r->ct = false;
		return exprPostfixRestR(r);
	}
	if (consume(DOT))
	{
		if (!consume(ID))
			parseErr("missing field name after `%s`", tokenText(DOT));
		Token *tkName = last_consumed_tok;
		if (r->type.base != TB_STRUCT || r->type.sym == NULL)
			parseErr("a field can only be selected from a struct");
		Symbol *s = findSymbolInList(r->type.sym->structMembers, tkName->text);
		if (!s)
			parseErr("the structure %s does not have a field %s", r->type.sym->name, tkName->text);
		*r = makeRet(s->type, true, s->type.arrsize >= 0);
		return exprPostfixRestR(r);
	}
	return true;
}

/*
	Grammar rule:
```
	[exprPrimary] := [ID] ( [L_PARENTHESES] ( [expr] ( [COMMA] [expr] )* )? [R_PARENTHESES] )?
			   | [INT] | [DOUBLE] | [CHAR] | [STRING] | [L_PARENTHESES] [expr] [R_PARENTHESES]
```
*/
bool exprPrimary()
{
	Return r;
	return exprPrimaryR(&r);
}

static bool exprPrimaryR(Return *r)
{
	Token *start = current_tok;

	if (consume(ID))
	{
		Token *tkName = last_consumed_tok;
		Symbol *s = findSymbol(tkName->text);
		if (!s)
			parseErr("undefined id: %s", tkName->text);
		if (consume(L_PARENTHESES))
		{
			if (s->kind != SK_FN)
				parseErr("only a function can be called");
			Symbol *param = s->fn.params;
			if (!consume(R_PARENTHESES))
			{
				while (true)
				{
					Return rArg;
					if (!exprAssignR(&rArg))
						parseErr("missing expression after `%s` in function call", tokenText(L_PARENTHESES));
					if (!param)
						parseErr("too many arguments in function call");
					if (!canCast(&rArg.type, &param->type))
						parseErr("in call, cannot convert the argument type to the parameter type");
					param = param->next;
					if (consume(COMMA))
						continue;
					if (!consume(R_PARENTHESES))
						parseErr("missing `%s` after function call arguments", tokenText(R_PARENTHESES));
					break;
				}
			}
			if (param)
				parseErr("too few arguments in function call");
			*r = makeRet(s->type, false, true);
			return true;
		}
		if (s->kind == SK_FN)
			parseErr("a function can only be called");
		*r = makeRet(s->type, true, s->type.arrsize >= 0);
		return true;
	}

	current_tok = start;
	if (consume(INT))
	{
		*r = makeRet((Type){TB_INT, NULL, -1}, false, true);
		return true;
	}
	current_tok = start;
	if (consume(DOUBLE))
	{
		*r = makeRet((Type){TB_DOUBLE, NULL, -1}, false, true);
		return true;
	}
	current_tok = start;
	if (consume(CHAR))
	{
		*r = makeRet((Type){TB_CHAR, NULL, -1}, false, true);
		return true;
	}
	current_tok = start;
	if (consume(STRING))
	{
		*r = makeRet((Type){TB_CHAR, NULL, 0}, false, true);
		return true;
	}

	current_tok = start;
	if (consume(L_PARENTHESES))
	{
		if (!exprAssignR(r))
			parseErr("missing expression after `%s`", tokenText(L_PARENTHESES));
		if (!consume(R_PARENTHESES))
			parseErr("missing `%s` after parenthesized expression", tokenText(R_PARENTHESES));
		return true;
	}

	current_tok = start;
	return false;
}

static bool exprAssignR(Return *r)
{
	Token *start = current_tok;
	Return left;
	if (exprUnaryR(&left))
	{
		if (consume(ASSIGN))
		{
			int op = last_consumed_tok->code;
			if (!exprAssignR(r))
				parseErr("missing expression after `%s`", tokenText(op));
			if (!left.lval)
				parseErr("the assign destination must be a left-value");
			if (left.ct)
				parseErr("the assign destination cannot be constant");
			if (!canBeScalar(&left))
				parseErr("the assign destination must be scalar");
			if (!canBeScalar(r))
				parseErr("the assign source must be scalar");
			if (!canCast(&r->type, &left.type))
				parseErr("the assign source cannot be converted to destination");
			r->lval = false;
			r->ct = true;
			return true;
		}
	}

	current_tok = start;
	return exprOrR(r);
}

/*
	Behavior:
		- Repeatedly attempts to parse top-level declarations: `struct`, `functions` or `variable`
		- Continues parsing as long as one of these matches
		- Stops when no valid declaration is found
		- Finally expects and consumes an END token

	Grammar rule:
```
	[unit] := ( [structDef] | [fnDef] | [varDef] )* [END]
```
*/
bool unit()
{
	while (true)
	{
		if (structDef())
		{
			// struct consumed
		}
		else if (functionDef())
		{
			// function consumed
		}
		else if (variableDef())
		{
			// variable consumed
		}
		else
		{
			// No more top-level declarations found
			break;
		}
	}

	// After top-level declarations are done there should be nothing else (END).
	if (consume(END))
	{
		return true;
	}

	return false;
}

void parse(Token *tokens)
{
	current_tok = tokens;
	if (!unit())
		parseErr("unknown syntax error");
}
