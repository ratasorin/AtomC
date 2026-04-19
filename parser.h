#pragma once
#include "ad.h"
#include "lexer.h"
#include "stdbool.h"

void parse(Token *tokens);
bool structDef();
bool functionDef();
bool variableDef();
bool typeBase(Type *t);
bool arrayDecl(Type *t);
bool fnParam();
bool stm();
bool stmCompound(bool newDomain);
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
