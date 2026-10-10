#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "tokenizer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void token_list_init(TokenList *list) {
    if (!list) return;
    list->count = 0;
    list->capacity = 16;
    list->tokens = safe_malloc(sizeof(Token) * list->capacity);
}

void token_list_add(TokenList *list, TokenType type, const char *value, bool sq, bool dq, int line, int col) {
    if (!list) return;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->tokens = safe_realloc(list->tokens, sizeof(Token) * list->capacity);
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? safe_strdup(value) : NULL;
    tok->single_quoted = sq;
    tok->double_quoted = dq;
    tok->line = line;
    tok->col = col;
}

void token_list_free(TokenList *list) {
    if (!list || !list->tokens) return;
    for (size_t i = 0; i < list->count; i++) {
        if (list->tokens[i].value) {
            free(list->tokens[i].value);
            list->tokens[i].value = NULL;
        }
    }
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_WORD: return "WORD";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_REDIRECT_IN: return "REDIRECT_IN";
        case TOKEN_REDIRECT_OUT: return "REDIRECT_OUT";
        case TOKEN_REDIRECT_APPEND: return "REDIRECT_APPEND";
        case TOKEN_REDIRECT_STDERR: return "REDIRECT_STDERR";
        case TOKEN_REDIRECT_STDERR_APPEND: return "REDIRECT_STDERR_APPEND";
        case TOKEN_REDIRECT_STDERR_TO_STDOUT: return "REDIRECT_STDERR_TO_STDOUT";
        case TOKEN_BACKGROUND: return "BACKGROUND";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void token_list_debug_print(const TokenList *list) {
    if (!list) return;
    debug_print("--- Token List (%zu tokens) ---", list->count);
    for (size_t i = 0; i < list->count; i++) {
        debug_print("[%zu] Type: %-22s Val: '%s' (sq=%d, dq=%d)",
                    i,
                    token_type_name(list->tokens[i].type),
                    list->tokens[i].value ? list->tokens[i].value : "",
                    list->tokens[i].single_quoted,
                    list->tokens[i].double_quoted);
    }
}

/* Helper to check if char can separate words */
static inline bool is_delimiter(char c) {
    return isspace((unsigned char)c) || c == '|' || c == '<' || c == '>' ||
           c == '&' || c == ';' || c == '(' || c == ')' || c == '\0';
}

bool tokenize_line(const char *input, TokenList *list, char *err_msg, size_t err_size) {
    if (!input || !list) return false;
    token_list_init(list);

    const char *p = input;
    int line = 1;
    int col = 1;

    /* Dynamic buffer for word accumulator */
    size_t buf_cap = 64;
    char *buf = safe_malloc(buf_cap);
    size_t buf_len = 0;

    while (*p) {
        /* Skip leading whitespace */
        if (isspace((unsigned char)*p)) {
            if (*p == '\n') { line++; col = 1; }
            else { col++; }
            p++;
            continue;
        }

        /* Comment: # outside quotes ignores rest of the line */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        int tok_col = col;

        /* Check multi-character and redirection operators */
        if (p[0] == '2' && p[1] == '>' && p[2] == '&' && p[3] == '1') {
            token_list_add(list, TOKEN_REDIRECT_STDERR_TO_STDOUT, "2>&1", false, false, line, tok_col);
            p += 4; col += 4;
            continue;
        }
        if (p[0] == '2' && p[1] == '>' && p[2] == '>') {
            token_list_add(list, TOKEN_REDIRECT_STDERR_APPEND, "2>>", false, false, line, tok_col);
            p += 3; col += 3;
            continue;
        }
        if (p[0] == '2' && p[1] == '>') {
            token_list_add(list, TOKEN_REDIRECT_STDERR, "2>", false, false, line, tok_col);
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '&' && p[1] == '&') {
            token_list_add(list, TOKEN_AND, "&&", false, false, line, tok_col);
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '|' && p[1] == '|') {
            token_list_add(list, TOKEN_OR, "||", false, false, line, tok_col);
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '>' && p[1] == '>') {
            token_list_add(list, TOKEN_REDIRECT_APPEND, ">>", false, false, line, tok_col);
            p += 2; col += 2;
            continue;
        }

        /* Single-char operators */
        if (*p == '|') {
            token_list_add(list, TOKEN_PIPE, "|", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == '<') {
            token_list_add(list, TOKEN_REDIRECT_IN, "<", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == '>') {
            token_list_add(list, TOKEN_REDIRECT_OUT, ">", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == '&') {
            token_list_add(list, TOKEN_BACKGROUND, "&", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == ';') {
            token_list_add(list, TOKEN_SEMICOLON, ";", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == '(') {
            token_list_add(list, TOKEN_LPAREN, "(", false, false, line, tok_col);
            p++; col++;
            continue;
        }
        if (*p == ')') {
            token_list_add(list, TOKEN_RPAREN, ")", false, false, line, tok_col);
            p++; col++;
            continue;
        }

        /* Parse a WORD token (may contain quotes, escapes, etc.) */
        buf_len = 0;
        bool has_single_quote = false;
        bool has_double_quote = false;

        while (*p) {
            if (*p == '\'') {
                /* Single quoted section */
                has_single_quote = true;
                p++; col++;
                while (*p && *p != '\'') {
                    if (buf_len + 2 >= buf_cap) {
                        buf_cap *= 2;
                        buf = safe_realloc(buf, buf_cap);
                    }
                    buf[buf_len++] = *p++;
                    col++;
                }
                if (*p != '\'') {
                    if (err_msg && err_size > 0) {
                        snprintf(err_msg, err_size, "syntax error: unmatched single quote at col %d", col);
                    }
                    free(buf);
                    return false;
                }
                p++; col++; /* Consume closing quote */
            } else if (*p == '"') {
                /* Double quoted section */
                has_double_quote = true;
                p++; col++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (*(p + 1) == '"' || *(p + 1) == '\\' || *(p + 1) == '$' || *(p + 1) == '`')) {
                        /* Escaped character inside double quotes */
                        p++; col++;
                    }
                    if (buf_len + 2 >= buf_cap) {
                        buf_cap *= 2;
                        buf = safe_realloc(buf, buf_cap);
                    }
                    buf[buf_len++] = *p++;
                    col++;
                }
                if (*p != '"') {
                    if (err_msg && err_size > 0) {
                        snprintf(err_msg, err_size, "syntax error: unmatched double quote at col %d", col);
                    }
                    free(buf);
                    return false;
                }
                p++; col++; /* Consume closing quote */
            } else if (*p == '\\') {
                /* Backslash escaping outside quotes */
                p++; col++;
                if (*p) {
                    if (buf_len + 2 >= buf_cap) {
                        buf_cap *= 2;
                        buf = safe_realloc(buf, buf_cap);
                    }
                    buf[buf_len++] = *p++;
                    col++;
                }
            } else if (p[0] == '$' && p[1] == '(') {
                /* Command substitution $(...) -> keep intact for expansion */
                if (buf_len + 4 >= buf_cap) {
                    buf_cap *= 2;
                    buf = safe_realloc(buf, buf_cap);
                }
                buf[buf_len++] = '$';
                buf[buf_len++] = '(';
                p += 2; col += 2;
                int paren_depth = 1;
                while (*p && paren_depth > 0) {
                    if (*p == '(') paren_depth++;
                    else if (*p == ')') paren_depth--;
                    
                    if (buf_len + 2 >= buf_cap) {
                        buf_cap *= 2;
                        buf = safe_realloc(buf, buf_cap);
                    }
                    buf[buf_len++] = *p++;
                    col++;
                }
                if (paren_depth != 0) {
                    if (err_msg && err_size > 0) {
                        snprintf(err_msg, err_size, "syntax error: unmatched parenthesis in $(...)");
                    }
                    free(buf);
                    return false;
                }
            } else if (is_delimiter(*p)) {
                /* Reached delimiter outside quotes -> end of word */
                break;
            } else {
                if (buf_len + 2 >= buf_cap) {
                    buf_cap *= 2;
                    buf = safe_realloc(buf, buf_cap);
                }
                buf[buf_len++] = *p++;
                col++;
            }
        }

        buf[buf_len] = '\0';
        token_list_add(list, TOKEN_WORD, buf, has_single_quote, has_double_quote, line, tok_col);
    }

    free(buf);
    token_list_add(list, TOKEN_EOF, NULL, false, false, line, col);
    return true;
}
