#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "utils.h"

Token *tokens;
Token *lastTk;

int linecount = 1;

Token *addTk(int code)
{
	Token *tk = safeAlloc(sizeof(Token));
	tk->code = code;
	tk->line_num = linecount;
	tk->source_text = NULL;
	tk->next = NULL;
	if (lastTk)
	{
		lastTk->next = tk;
	}
	else
	{
		tokens = tk;
	}
	lastTk = tk;
	return tk;
}

char *extract(const char *begin, const char *end)
{
	int len = (int)(end - begin);
	char *p = safeAlloc((size_t)len + 1);
	memcpy(p, begin, (size_t)len);
	p[len] = '\0';
	return p;
}

typedef struct
{
	int code;
	const char *literal;
} TokenType;

TokenType punctuators[] = {
	{.code = COMMA, .literal = ","},
	{.code = SEMICOLON, .literal = ";"},
	{.code = L_PARENTHESES, .literal = "("},
	{.code = R_PARENTHESES, .literal = ")"},
	{.code = L_BRACKET, .literal = "["},
	{.code = R_BRACKET, .literal = "]"},
	{.code = L_ACCOLADE, .literal = "{"},
	{.code = R_ACCOLADE, .literal = "}"},

	{.code = AND, .literal = "&&"},
	{.code = OR, .literal = "||"},
	{.code = EQUAL, .literal = "=="},
	{.code = NOT_EQ, .literal = "!="},
	{.code = LESS_EQ, .literal = "<="},
	{.code = GREATER_EQ, .literal = ">="},
	{
		.code = ADD,
		.literal = "+",
	},
	{
		.code = SUB,
		.literal = "-",
	},
	{
		.code = MUL,
		.literal = "*",
	},
	{
		.code = DIV,
		.literal = "/",
	},
	{
		.code = DOT,
		.literal = ".",
	},
	{
		.code = NOT,
		.literal = "!",
	},
	{
		.code = ASSIGN,
		.literal = "=",
	},
	{
		.code = LESS,
		.literal = "<",
	},
	{
		.code = GREATER,
		.literal = ">",
	}};

TokenType keywords[] = {
	{.literal = "char", .code = TYPE_CHAR},
	{.literal = "double", .code = TYPE_DOUBLE},
	{.literal = "else", .code = ELSE},
	{.literal = "if", .code = IF},
	{.literal = "int", .code = TYPE_INT},
	{.literal = "return", .code = RETURN},
	{.literal = "struct", .code = STRUCT},
	{.literal = "void", .code = VOID},
	{.literal = "while", .code = WHILE}};

const size_t PUNCTUATORS_COUNT = sizeof(punctuators) / sizeof(punctuators[0]);
const size_t KEYWORDS_COUNT = sizeof(keywords) / sizeof(keywords[0]);

void maybeAdvanceLineCounter(const char *text)
{
	if (*text == '\n')
		linecount++;
}

static int matchLiteral(const char *buffer, const char *literal)
{
	int n = strlen(literal);
	return strncmp(buffer, literal, n) == 0 ? n : 0;
}

/**
 * An identifier cannot start with a number, special character (\, /, ?, ., &)
 * @return
 * - "0" when cannot be identifier (i.e: 00my_num, #my_char)
 *
 * - "1" when it may be an identifier (i.e.: puts, _character)
 */
static int isIdentifierStart(char ch)
{
	return isalpha(ch) || ch == '_';
}

static int isIdentifierChar(char ch)
{
	return isalnum(ch) || ch == '_';
}

static int matchDigits(const char *buffer)
{
	int len = 0;
	while (isdigit((unsigned char)buffer[len]))
		len++;
	return len;
}

int isPunctuator(int code)
{
	switch (code)
	{
	case AND:
	case OR:
	case EQUAL:
	case NOT_EQ:
	case LESS_EQ:
	case GREATER_EQ:
	case ADD:
	case SUB:
	case MUL:
	case DIV:
	case DOT:
	case NOT:
	case ASSIGN:
	case LESS:
	case GREATER:
	case COMMA:
	case SEMICOLON:
	case L_PARENTHESES:
	case R_PARENTHESES:
	case L_BRACKET:
	case R_BRACKET:
	case L_ACCOLADE:
	case R_ACCOLADE:
		return 1;
	default:
		return 0;
	}
}

int matchPunctuator(TokenType *tok, const char *text, int *found_len)
{
	if (isPunctuator(tok->code))
	{
		*found_len = matchLiteral(text, tok->literal);
		return *found_len > 0;
	}

	*found_len = 0;
	return 0;
}

int matchKeyword(const char *buffer, const char *keyword_literal)
{
	int len = matchLiteral(buffer, keyword_literal);

	// tried matching the buffer with the wrong keyword:
	// - buffer: "inker......\0", keyword: "int"
	// - buffer: "does.......\0", keyword: "double"
	// - etc.
	if (len == 0)
		return 0;

	// make sure strings like "integer" or "character" are not matched as "int" or "char" just because they have the same start
	if (isIdentifierChar(buffer[len]))
		return 0;

	return len;
}

int tryKeywords(const char *buffer, int *found_keyword_code, int *found_keyword_len)
{
	for (size_t i = 0; i < KEYWORDS_COUNT; i++)
	{
		int matched_token_len = matchKeyword(buffer, keywords[i].literal);
		if (matched_token_len == 0)
			continue;

		*found_keyword_code = keywords[i].code;
		*found_keyword_len = matched_token_len;
		return 1;
	}

	return 0;
}

/**
 * An "identifier" is the name of a variable/function that we can find starting at the current buffer
 * @return The length of the identifier
 */
int matchIdentifier(const char *buffer)
{
	int len = 0;
	if (!isIdentifierStart(buffer[len]))
		return 0;

	len++;
	while (isIdentifierChar(buffer[len]))
		len++;

	return len;
}

static int matchDouble(const char *buffer, int *len, double *value)
{
	int intLen = matchDigits(buffer);
	if (intLen == 0)
		return 0;

	int pos = intLen;
	int sawDot = 0;
	int sawExponent = 0;

	if (buffer[pos] == '.')
	{
		sawDot = 1;
		pos++;
		int fracLen = matchDigits(buffer + pos);
		if (fracLen == 0)
			err("Invalid DOUBLE syntax: Missing digits after decimal point at line %d", linecount);
		pos += fracLen;
	}

	if (buffer[pos] == 'e' || buffer[pos] == 'E')
	{
		sawExponent = 1;
		pos++;
		if (buffer[pos] == '+' || buffer[pos] == '-')
			pos++;
		if (!isdigit((unsigned char)buffer[pos]))
			err("Invalid DOUBLE syntax: Missing exponent digits at line %d", linecount);
		pos += matchDigits(buffer + pos);
	}

	if (!sawDot && !sawExponent)
		return 0;

	if (isIdentifierChar(buffer[pos]))
		err("Invalid DOUBLE syntax: Invalid suffix starting with '%c' at line %d", buffer[pos], linecount);

	char *text = extract(buffer, buffer + pos);
	*value = strtod(text, NULL);
	free(text);
	*len = pos;
	return 1;
}

static int matchInt(const char *buffer, int *len, int *value)
{
	int intLen = matchDigits(buffer);
	if (intLen == 0)
		return 0;

	if (isIdentifierChar(buffer[intLen]))
		err("Invalid INT syntax: Invalid suffix starting with '%c' at line %d", buffer[intLen], linecount);

	char *text = extract(buffer, buffer + intLen);
	*value = atoi(text);
	free(text);
	*len = intLen;
	return 1;
}

static int decodeEscapeChar(char ch, char *decoded)
{
	switch (ch)
	{
	case 'a':
		*decoded = '\a';
		return 1;
	case 'b':
		*decoded = '\b';
		return 1;
	case 'f':
		*decoded = '\f';
		return 1;
	case 'n':
		*decoded = '\n';
		return 1;
	case 'r':
		*decoded = '\r';
		return 1;
	case 't':
		*decoded = '\t';
		return 1;
	case 'v':
		*decoded = '\v';
		return 1;
	case '\\':
		*decoded = '\\';
		return 1;
	case '\'':
		*decoded = '\'';
		return 1;
	case '"':
		*decoded = '"';
		return 1;
	case '0':
		*decoded = '\0';
		return 1;
	default:
		return 0;
	}
}

static int matchCharLiteral(const char *buffer, int *len, char *value)
{
	/**
	 * Don't bother considering the buffer starting here a char if it's first character is not a literal '
	 * Implemented group: [']
	 */
	if (buffer[0] != '\'')
		return 0;

	if (buffer[1] == '\\')
	{
		char decoded;
		if (buffer[2] == '\0')
			err("Invalid CHAR syntax: Unterminated escape sequence at line %d", linecount);
		if (!decodeEscapeChar(buffer[2], &decoded))
			err("Invalid CHAR syntax: Illegal escape sequence \\%c at line %d", buffer[2], linecount);
		if (buffer[3] != '\'')
		{
			if (buffer[3] == '\0')
				err("Invalid CHAR syntax: Missing closing ' before End Of File at line %d", linecount);
			if (buffer[3] == '\n')
				err("Invalid CHAR syntax: Atom C doesn't allow newlines inside chars at line %d", linecount);
			err("Invalid CHAR syntax: Missing closing ' at line %d", linecount);
		}
		*value = decoded;
		*len = 4;
		return 1;
	}

	if (buffer[1] == '\0')
		err("Invalid CHAR syntax: Missing closing ' before End Of File at line %d", linecount);

	if (buffer[1] == '\'')
		err("Invalid CHAR syntax: Missing CHAR content/payload at line %d", linecount);

	if (buffer[1] == '\n')
		err("Invalid CHAR syntax: Atom C doesn't allow newlines inside chars at line %d", linecount);

	if (buffer[2] != '\'')
	{
		if (buffer[2] == '\0')
			err("Invalid CHAR syntax: Missing closing ' before End Of File at line %d", linecount);
		if (buffer[2] == '\n')
			err("Invalid CHAR syntax: Atom C doesn't allow newlines inside chars at line %d", linecount);
		err("Invalid CHAR syntax: CHAR literals must contain exactly one character at line %d", linecount);
	}

	*value = buffer[1];
	*len = 3;
	return 1;
}

static int matchStringLiteral(const char *buffer, int *len, char **value)
{
	if (buffer[0] != '"')
		return 0;

	const char *p = buffer + 1;
	char *out = safeAlloc(strlen(buffer) + 1);
	int outLen = 0;

	while (*p && *p != '"')
	{
		if (*p == '\n')
		{
			free(out);
			err("Invalid STRING syntax: Atom C doesn't allow newlines inside strings at line %d", linecount);
		}

		if (*p == '\\')
		{
			char decoded;
			if (p[1] == '\0')
			{
				free(out);
				err("Invalid STRING syntax: Unterminated escape sequence at line %d", linecount);
			}
			if (!decodeEscapeChar(p[1], &decoded))
			{
				free(out);
				err("Invalid STRING syntax: Illegal escape sequence \\%c at line %d", p[1], linecount);
			}
			out[outLen++] = decoded;
			p += 2;
			continue;
		}

		out[outLen++] = *p;
		p++;
	}

	if (*p != '"')
	{
		free(out);
		err("Invalid STRING syntax: Missing closing \" before End Of File at line %d", linecount);
	}

	out[outLen] = '\0';
	*len = (int)(p - buffer) + 1;
	*value = out;
	return 1;
}

Token *tokenize(const char *pch)
{
	tokens = NULL;
	lastTk = NULL;
	linecount = 1;

	for (;;)
	{
		if (isspace((unsigned char)*pch))
		{
			maybeAdvanceLineCounter(pch);
			pch++;
			continue;
		}

		if (*pch == '\0')
		{
			addTk(END);
			return tokens;
		}

		int code = 0;
		int len = 0;
		if (tryKeywords(pch, &code, &len))
		{
			Token *tk = addTk(code);
			tk->source_text = extract(pch, pch + len);
			// len = keyword length
			pch += len;
			continue;
		}

		len = matchIdentifier(pch);
		if (len > 0)
		{
			Token *tk = addTk(ID);
			tk->source_text = extract(pch, pch + len);
			// len = identifier length
			pch += len;
			continue;
		}

		double doubleValue = 0;
		if (matchDouble(pch, &len, &doubleValue))
		{
			Token *tk = addTk(DOUBLE);
			tk->source_text = extract(pch, pch + len);
			tk->d = doubleValue;
			pch += len;
			continue;
		}

		int intValue = 0;
		if (matchInt(pch, &len, &intValue))
		{
			Token *tk = addTk(INT);
			tk->source_text = extract(pch, pch + len);
			tk->i = intValue;
			pch += len;
			continue;
		}

		char charValue = '\0';
		if (matchCharLiteral(pch, &len, &charValue))
		{
			Token *tk = addTk(CHAR);
			tk->source_text = extract(pch, pch + len);
			tk->c = charValue;
			pch += len;
			continue;
		}

		char *stringValue = NULL;
		if (matchStringLiteral(pch, &len, &stringValue))
		{
			Token *tk = addTk(STRING);
			tk->source_text = stringValue;
			pch += len;
			continue;
		}

		for (size_t i = 0; i < PUNCTUATORS_COUNT; i++)
		{
			if (!matchPunctuator(&punctuators[i], pch, &len))
				continue;

			Token *tk = addTk(punctuators[i].code);
			tk->source_text = extract(pch, pch + len);
			pch += len;
			code = 1;
			break;
		}

		if (code)
			continue;

		err("invalid char: %c (%d)", *pch, *pch);
	}
}

const char *tokenNames[] = {
	"ID",
	"TYPE_CHAR", "TYPE_DOUBLE", "ELSE", "IF", "TYPE_INT", "RETURN", "STRUCT", "VOID", "WHILE",
	"COMMA", "SEMICOLON", "L_PARENTHESES", "R_PARENTHESES", "LBRACKET", "RBRACKET", "L_ACCOLADE", "R_ACCOLADE",
	"ADD", "SUB", "MUL", "DIV", "DOT", "AND", "OR", "NOT", "ASSIGN", "EQUAL", "NOT_EQ", "LESS", "LESS_EQ", "GREATER", "GREATER_EQ",
	"INT", "DOUBLE", "CHAR", "STRING",
	"END"};

void showTokens(const Token *tokens)
{
	for (const Token *tk = tokens; tk; tk = tk->next)
	{
		printf("%d\t%s", tk->line_num, tokenNames[tk->code]);
		if ((tk->code == ID || tk->code == STRING) && tk->source_text)
		{
			printf(":%s", tk->source_text);
		}
		else if (tk->code == INT)
		{
			printf(":%d", tk->i);
		}
		else if (tk->code == DOUBLE)
		{
			printf(":%.15g", tk->d);
		}
		else if (tk->code == CHAR)
		{
			printf(":%c", tk->c);
		}
		printf("\n");
	}
}
