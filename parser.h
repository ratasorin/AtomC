#pragma once
#include "lexer.h"
#include "stdbool.h"

void parse(Token *tokens);
bool structDef();
bool functionDef();
bool variableDef();
bool arrayDecl();
bool fnParam();
bool stm();
bool stmCompound();
bool expr();
bool exprAssign();
bool exprOr();
bool exprAnd();
bool exprEq();
bool exprRel();
bool exprAdd();
bool exprMul();
bool exprCast();
bool exprUnary();
bool exprPostfix();
bool exprPrimary();