#pragma once

enum
{
	ID,
	// keywords
	TYPE_CHAR,
	TYPE_DOUBLE,
	ELSE,
	IF,
	TYPE_INT,
	RETURN,
	STRUCT,
	VOID,
	WHILE,
	// delimiters
	COMMA,
	SEMICOLON,
	L_PARENTHESES,
	R_PARENTHESES,
	L_BRACKET,
	R_BRACKET,
	L_ACCOLADE,
	R_ACCOLADE,
	// operators
	ADD,
	SUB,
	MUL,
	DIV,
	DOT,
	AND,
	OR,
	NOT,
	ASSIGN,
	EQUAL,
	NOT_EQ,
	LESS,
	LESS_EQ,
	GREATER,
	GREATER_EQ,
	// constants
	INT,
	DOUBLE,
	CHAR,
	STRING,
	// special
	END
};

typedef struct Token
{
	int code;
	int line_num;
	char *source_text;
	union
	{
		char *text;
		int i;
		char c;
		double d;
	};
	struct Token *next;
} Token;

Token *tokenize(const char *pch);
void showTokens(const Token *tokens);
