#include <stdio.h>
#include "lexer.h"
#include "utils.h"
#include "parser.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        err("Usage: %s <file>", argv[0]);
    }

    char *text = loadFile(argv[1]);
    Token *tokens = tokenize(text);
    pushDomain();
    parse(tokens);
    showDomain(currentDomain, "global");
    dropDomain();

    return 0;
}
