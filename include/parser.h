#ifndef SHELLCRAFT_PARSER_H
#define SHELLCRAFT_PARSER_H

#include "tokenizer.h"
#include "ast.h"

typedef struct Parser {
    const TokenList *tokens;
    size_t pos;
    char *error_msg;
    size_t error_size;
    bool has_error;
} Parser;

/* Parses tokens into an Abstract Syntax Tree */
AstProgram *parse_tokens(const TokenList *tokens, char *err_msg, size_t err_size);

#endif /* SHELLCRAFT_PARSER_H */
