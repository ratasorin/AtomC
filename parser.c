#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "parser.h"

/**
 Position reached in the list: `tokens` extracted by `lexer.c`

 If the program's syntax is correct, `current_tok` should advance to END

 Example of failure point:
```
	tokens: [ IF ] → [ L_PARENTHESIS ] → [ EXPR ] → [ R_BRACKET ] → ... → END

	current_tok: [ IF ]
	ifExpression() → consume(IF_CODE) = true

	current_tok: [ L_PARENTHESIS ]
	ifExpression() → consume(L_PARENTHESIS_CODE) = true

	current_tok: [ EXPR ]
	ifExpression() → consume(EXPR_CODE) = true

	current_tok: [ R_BRACKET ]
	ifExpression() → consume(R_PARENTHESIS_CODE) = false → Error
```
 */
Token *current_tok;
Token *last_consumed_tok; // the last consumed token

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
		- Accepts primitive types: int, double, char
		- Accepts struct types only if followed by an identifier (e.g., struct Point)

	Grammar rule:
```
	[typeBase] := [TYPE_INT] | [TYPE_DOUBLE] | [TYPE_CHAR] | [STRUCT] [ID]
```
*/
bool typeBase()
{
	Token *start = current_tok;

	if (consume(TYPE_INT))
	{
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			return true;
		}
		parseErr("missing identifier after `struct`");
	}

	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[arrayDecl] := [L_BRACKET] [INT]? [R_BRACKET]
```
*/
bool arrayDecl()
{
	Token *start = current_tok;
	if (consume(L_BRACKET))
	{
		consume(INT);
		if (consume(R_BRACKET))
		{
			return true;
		}
		parseErr("missing `]` in array declaration");
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[varDef] := [typeBase] [ID] [arrayDecl]? [SEMICOLON]
```
*/
bool variableDef()
{
	Token *start = current_tok;
	if (typeBase())
	{
		if (consume(ID))
		{
			arrayDecl();
			if (consume(SEMICOLON))
			{
				return true;
			}
			parseErr("missing `;` after variable declaration");
		}
		parseErr("missing identifier in variable declaration");
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[structDef] := [STRUCT] [ID] [L_ACCOLADE] [varDef]* [R_ACCOLADE] [SEMICOLON]
```
*/
bool structDef()
{
	Token *start = current_tok;
	if (consume(STRUCT))
	{
		if (!consume(ID))
			parseErr("missing struct name");
		if (!consume(L_ACCOLADE))
		{
			current_tok = start;
			return false;
		}

		while (variableDef())
		{
		}

		if (!consume(R_ACCOLADE))
			parseErr("missing `}` at end of struct declaration");
		if (!consume(SEMICOLON))
			parseErr("missing `;` after struct declaration");
		return true;
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[fnParam] := [typeBase] [ID] [arrayDecl]?
```
*/
bool fnParam()
{
	Token *start = current_tok;
	if (typeBase())
	{
		if (consume(ID))
		{
			arrayDecl();
			return true;
		}
		parseErr("missing parameter name");
	}
	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[fnDef] := ( [typeBase] | [VOID] ) [ID]
			 [L_PARENTHESES] ( [fnParam] ( [COMMA] [fnParam] )* )? [R_PARENTHESES]
			 [stmCompound]
```
*/
bool functionDef()
{
	Token *start = current_tok;
	if (typeBase() || consume(VOID))
	{
		if (!consume(ID))
			parseErr("missing function name");

		// Keep varDef/fnDef disambiguation: if there is no `(`, this is not fnDef.
		if (!consume(L_PARENTHESES))
		{
			current_tok = start;
			return false;
		}

		if (fnParam())
		{
			while (consume(COMMA))
			{
				if (!fnParam())
					parseErr("missing parameter after `,`");
			}
		}

		if (!consume(R_PARENTHESES))
			parseErr("missing `)` after function parameters");
		if (!stmCompound())
			parseErr("missing function body");
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
	if (stmCompound())
		return true;

	current_tok = start;
	if (consume(IF))
	{
		if (!consume(L_PARENTHESES))
			parseErr("missing `(` after `if`");
		if (!expr())
			parseErr("missing/invalid condition in `if`");
		if (!consume(R_PARENTHESES))
			parseErr("missing `)` after `if` condition");
		if (!stm())
			parseErr("missing statement after `if`");
		if (consume(ELSE))
		{
			if (!stm())
				parseErr("missing statement after `else`");
		}
		return true;
	}

	current_tok = start;
	if (consume(WHILE))
	{
		if (!consume(L_PARENTHESES))
			parseErr("missing `(` after `while`");
		if (!expr())
			parseErr("missing/invalid condition in `while`");
		if (!consume(R_PARENTHESES))
			parseErr("missing `)` after `while` condition");
		if (!stm())
			parseErr("missing statement after `while`");
		return true;
	}

	current_tok = start;
	if (consume(RETURN))
	{
		expr();
		if (!consume(SEMICOLON))
			parseErr("missing `;` after `return`");
		return true;
	}

	current_tok = start;
	expr();
	if (consume(SEMICOLON))
		return true;

	current_tok = start;
	return false;
}

/*
	Grammar rule:
```
	[stmCompound] := [L_ACCOLADE] ( [varDef] | [stm] )* [R_ACCOLADE]
```
*/
bool stmCompound()
{
	Token *start = current_tok;
	if (consume(L_ACCOLADE))
	{
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
			parseErr("missing `}` at end of block");
		return true;
	}
	current_tok = start;
	return false;
}

// Internal helpers for the left-recursion eliminated expression rules.
static bool exprOrRest(void);
static bool exprAndRest(void);
static bool exprEqRest(void);
static bool exprRelRest(void);
static bool exprAddRest(void);
static bool exprMulRest(void);
static bool exprPostfixRest(void);

bool expr()
{
	return exprAssign();
}

/*
	Grammar rule:
```
	[exprAssign] := [exprUnary] [ASSIGN] [exprAssign] | [exprOr]
```
*/
bool exprAssign()
{
	Token *start = current_tok;
	if (exprUnary())
	{
		if (consume(ASSIGN))
		{
			if (exprAssign())
				return true;
			parseErr("missing expression after assignment `=`");
		}
	}

	current_tok = start;
	return exprOr();
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
	if (!exprAnd())
		return false;
	return exprOrRest();
}

static bool exprOrRest()
{
	if (consume(OR))
	{
		if (!exprAnd())
			parseErr("missing expression after `||`");
		return exprOrRest();
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
	if (!exprEq())
		return false;
	return exprAndRest();
}

static bool exprAndRest()
{
	if (consume(AND))
	{
		if (!exprEq())
			parseErr("missing expression after `&&`");
		return exprAndRest();
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
	if (!exprRel())
		return false;
	return exprEqRest();
}

static bool exprEqRest()
{
	if (consume(EQUAL) || consume(NOT_EQ))
	{
		if (!exprRel())
			parseErr("missing expression after equality operator");
		return exprEqRest();
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
	if (!exprAdd())
		return false;
	return exprRelRest();
}

static bool exprRelRest()
{
	if (consume(LESS) || consume(LESS_EQ) || consume(GREATER) || consume(GREATER_EQ))
	{
		if (!exprAdd())
			parseErr("missing expression after relational operator");
		return exprRelRest();
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
	if (!exprMul())
		return false;
	return exprAddRest();
}

static bool exprAddRest()
{
	if (consume(ADD) || consume(SUB))
	{
		if (!exprMul())
			parseErr("missing expression after additive operator");
		return exprAddRest();
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
	if (!exprCast())
		return false;
	return exprMulRest();
}

static bool exprMulRest()
{
	if (consume(MUL) || consume(DIV))
	{
		if (!exprCast())
			parseErr("missing expression after multiplicative operator");
		return exprMulRest();
	}
	return true;
}

/*
	Grammar rule:
```
	[exprCast] := [L_PARENTHESES] [typeBase] [arrayDecl]? [R_PARENTHESES] [exprCast] | [exprUnary]
```
*/
bool exprCast()
{
	Token *start = current_tok;
	if (consume(L_PARENTHESES))
	{
		if (typeBase())
		{
			arrayDecl();
			if (!consume(R_PARENTHESES))
				parseErr("missing `)` in cast expression");
			if (!exprCast())
				parseErr("missing expression after cast");
			return true;
		}
	}

	current_tok = start;
	return exprUnary();
}

/*
	Grammar rule:
```
	[exprUnary] := ( [SUB] | [NOT] ) [exprUnary] | [exprPostfix]
```
*/
bool exprUnary()
{
	Token *start = current_tok;
	if (consume(SUB) || consume(NOT))
	{
		if (!exprUnary())
			parseErr("missing unary expression");
		return true;
	}

	current_tok = start;
	return exprPostfix();
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
	if (!exprPrimary())
		return false;
	return exprPostfixRest();
}

static bool exprPostfixRest()
{
	if (consume(L_BRACKET))
	{
		if (!expr())
			parseErr("missing index expression inside `[]`");
		if (!consume(R_BRACKET))
			parseErr("missing `]` after index expression");
		return exprPostfixRest();
	}
	if (consume(DOT))
	{
		if (!consume(ID))
			parseErr("missing field name after `.`");
		return exprPostfixRest();
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
	Token *start = current_tok;

	if (consume(ID))
	{
		if (consume(L_PARENTHESES))
		{
			if (expr())
			{
				while (consume(COMMA))
				{
					if (!expr())
						parseErr("missing expression after `,` in function call");
				}
			}
			if (!consume(R_PARENTHESES))
				parseErr("missing `)` after function call arguments");
		}
		return true;
	}

	current_tok = start;
	if (consume(INT) || consume(DOUBLE) || consume(CHAR) || consume(STRING))
	{
		return true;
	}

	current_tok = start;
	if (consume(L_PARENTHESES))
	{
		if (!expr())
			parseErr("missing expression after `(`");
		if (!consume(R_PARENTHESES))
			parseErr("missing `)`");
		return true;
	}

	current_tok = start;
	return false;
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
