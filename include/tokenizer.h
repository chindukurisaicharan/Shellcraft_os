#ifndef SHELLCRAFT_TOKENIZER_H
#define SHELLCRAFT_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,                      /* | */
    TOKEN_REDIRECT_IN,               /* < */
    TOKEN_REDIRECT_OUT,              /* > */
    TOKEN_REDIRECT_APPEND,           /* >> */
    TOKEN_REDIRECT_STDERR,           /* 2> */
    TOKEN_REDIRECT_STDERR_APPEND,    /* 2>> */
    TOKEN_REDIRECT_STDERR_TO_STDOUT, /* 2>&1 */
    TOKEN_BACKGROUND,                /* & */
    TOKEN_SEMICOLON,                 /* ; */
    TOKEN_AND,                       /* && */
    TOKEN_OR,                        /* || */
    TOKEN_LPAREN,                    /* ( */
    TOKEN_RPAREN,                    /* ) */
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    bool single_quoted;
    bool double_quoted;
    int line;
    int col;
} Token;

typedef struct TokenList {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenList;

/* Tokenizer lifecycle and operations */
void token_list_init(TokenList *list);
void token_list_add(TokenList *list, TokenType type, const char *value, bool sq, bool dq, int line, int col);
void token_list_free(TokenList *list);

/* Tokenizes raw input string into structured TokenList */
bool tokenize_line(const char *input, TokenList *list, char *err_msg, size_t err_size);

/* Token inspection helpers */
const char *token_type_name(TokenType type);
void token_list_debug_print(const TokenList *list);

#endif /* SHELLCRAFT_TOKENIZER_H */
