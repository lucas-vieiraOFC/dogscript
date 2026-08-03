#include "lexer.h"
#include "utils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *kw;
    TokenType type;
} Keyword;

static const Keyword g_keywords[] = {
    {"func", TOK_FUNC},       {"create", TOK_CREATE}, {"exec", TOK_EXEC},
    {"var", TOK_VAR},         {"class", TOK_CLASS},   {"extends", TOK_EXTENDS},
    {"if", TOK_IF},           {"elif", TOK_ELIF},     {"else", TOK_ELSE},
    {"then", TOK_THEN},       {"while", TOK_WHILE},   {"for", TOK_FOR},
    {"in", TOK_IN},           {"do", TOK_DO},         {"return", TOK_RETURN},
    {"use", TOK_USE},         {"new", TOK_NEW},       {"try", TOK_TRY},
    {"catch", TOK_CATCH},     {"finally", TOK_FINALLY},{"lambda", TOK_LAMBDA},
    {"cast", TOK_CAST},       {"const", TOK_CONST},   {"null", TOK_NULL},
    {"true", TOK_TRUE},       {"false", TOK_FALSE}
};

static int lx_peek(const Lexer *lx, int off) {
    size_t p = lx->pos + (size_t)off;
    if (p >= lx->len) {
        return '\0';
    }
    return (unsigned char)lx->src[p];
}

static int lx_advance(Lexer *lx) {
    int c = lx_peek(lx, 0);
    if (c == '\0') {
        return c;
    }
    lx->pos++;
    if (c == '\n') {
        lx->line++;
        lx->column = 1;
    } else {
        lx->column++;
    }
    return c;
}

static Token make_tok(TokenType t, const char *start, size_t n, int line, int col) {
    Token tok;
    tok.type = t;
    tok.lexeme = start ? ds_strndup(start, n) : ds_strdup("");
    tok.line = line;
    tok.column = col;
    return tok;
}

static void skip_ws_and_comments(Lexer *lx) {
    int c;
    for (;;) {
        c = lx_peek(lx, 0);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lx_advance(lx);
            continue;
        }
        if (c == '-' && lx_peek(lx, 1) == '-') {
            while (lx_peek(lx, 0) && lx_peek(lx, 0) != '\n') {
                lx_advance(lx);
            }
            continue;
        }
        if (c == '-' && lx_peek(lx, 1) == '[') {
            lx_advance(lx);
            lx_advance(lx);
            while (lx_peek(lx, 0)) {
                if (lx_peek(lx, 0) == ']' && lx_peek(lx, 1) == '-') {
                    lx_advance(lx);
                    lx_advance(lx);
                    break;
                }
                lx_advance(lx);
            }
            continue;
        }
        break;
    }
}

void lexer_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->len = strlen(src);
    lx->pos = 0;
    lx->line = 1;
    lx->column = 1;
}

Token lexer_next(Lexer *lx) {
    const char *start;
    int line, col;
    int c;
    size_t i;

    skip_ws_and_comments(lx);
    c = lx_peek(lx, 0);
    line = lx->line;
    col = lx->column;
    if (c == '\0') {
        return make_tok(TOK_EOF, "", 0, line, col);
    }

    if (isalpha(c) || c == '_') {
        size_t begin = lx->pos;
        while (isalnum(lx_peek(lx, 0)) || lx_peek(lx, 0) == '_') {
            lx_advance(lx);
        }
        start = lx->src + begin;
        for (i = 0; i < sizeof(g_keywords) / sizeof(g_keywords[0]); i++) {
            size_t n = strlen(g_keywords[i].kw);
            if ((size_t)(lx->src + lx->pos - start) == n &&
                strncmp(start, g_keywords[i].kw, n) == 0) {
                return make_tok(g_keywords[i].type, start, n, line, col);
            }
        }
        return make_tok(TOK_IDENTIFIER, start, (size_t)(lx->src + lx->pos - start), line, col);
    }

    if (isdigit(c)) {
        size_t begin = lx->pos;
        int is_float = 0;
        while (isdigit(lx_peek(lx, 0))) {
            lx_advance(lx);
        }
        if (lx_peek(lx, 0) == '.' && isdigit(lx_peek(lx, 1))) {
            is_float = 1;
            lx_advance(lx);
            while (isdigit(lx_peek(lx, 0))) {
                lx_advance(lx);
            }
        }
        start = lx->src + begin;
        return make_tok(is_float ? TOK_FLOAT : TOK_INT, start,
                        (size_t)(lx->src + lx->pos - start), line, col);
    }

    if (c == '"') {
        char *buf;
        size_t cap, n = 0;
        Token tok;
        lx_advance(lx);
        cap = 16;
        buf = (char *)malloc(cap);
        if (!buf) {
            ds_fatal("Out of memory lexing string literal");
        }
        while (lx_peek(lx, 0) && lx_peek(lx, 0) != '"') {
            int ch = lx_advance(lx);
            if (ch == '\\') {
                int esc = lx_advance(lx);
                switch (esc) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'r': ch = '\r'; break;
                    case '0': ch = '\0'; break;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    case '\'': ch = '\''; break;
                    case '\0':
                        ds_fatal("Lexer error at %d:%d: unterminated string literal", line, col);
                        break;
                    default: ch = esc; break;
                }
            }
            if (n + 1 >= cap) {
                cap *= 2;
                buf = (char *)realloc(buf, cap);
                if (!buf) {
                    ds_fatal("Out of memory lexing string literal");
                }
            }
            buf[n++] = (char)ch;
        }
        if (lx_peek(lx, 0) != '"') {
            free(buf);
            ds_fatal("Lexer error at %d:%d: unterminated string literal", line, col);
        }
        lx_advance(lx);
        buf[n] = '\0';
        tok = make_tok(TOK_STRING, buf, n, line, col);
        free(buf);
        return tok;
    }

    if (c == '=' && lx_peek(lx, 1) == '>') {
        lx_advance(lx); lx_advance(lx);
        return make_tok(TOK_ARROW, "=>", 2, line, col);
    }
    if (c == '=' && lx_peek(lx, 1) == '=') {
        lx_advance(lx); lx_advance(lx);
        return make_tok(TOK_EQ, "==", 2, line, col);
    }
    if (c == '!' && lx_peek(lx, 1) == '=') {
        lx_advance(lx); lx_advance(lx);
        return make_tok(TOK_NEQ, "!=", 2, line, col);
    }
    if (c == '<' && lx_peek(lx, 1) == '=') {
        lx_advance(lx); lx_advance(lx);
        return make_tok(TOK_LTE, "<=", 2, line, col);
    }
    if (c == '>' && lx_peek(lx, 1) == '=') {
        lx_advance(lx); lx_advance(lx);
        return make_tok(TOK_GTE, ">=", 2, line, col);
    }

    lx_advance(lx);
    switch (c) {
        case '(': return make_tok(TOK_LPAREN, "(", 1, line, col);
        case ')': return make_tok(TOK_RPAREN, ")", 1, line, col);
        case '{': return make_tok(TOK_LBRACE, "{", 1, line, col);
        case '}': return make_tok(TOK_RBRACE, "}", 1, line, col);
        case '[': return make_tok(TOK_LBRACKET, "[", 1, line, col);
        case ']': return make_tok(TOK_RBRACKET, "]", 1, line, col);
        case ',': return make_tok(TOK_COMMA, ",", 1, line, col);
        case '.': return make_tok(TOK_DOT, ".", 1, line, col);
        case ';': return make_tok(TOK_SEMICOLON, ";", 1, line, col);
        case '?': return make_tok(TOK_QUESTION, "?", 1, line, col);
        case ':': return make_tok(TOK_COLON, ":", 1, line, col);
        case '+': return make_tok(TOK_PLUS, "+", 1, line, col);
        case '-': return make_tok(TOK_MINUS, "-", 1, line, col);
        case '*': return make_tok(TOK_STAR, "*", 1, line, col);
        case '/': return make_tok(TOK_SLASH, "/", 1, line, col);
        case '=': return make_tok(TOK_ASSIGN, "=", 1, line, col);
        case '<': return make_tok(TOK_LT, "<", 1, line, col);
        case '>': return make_tok(TOK_GT, ">", 1, line, col);
        default:
            ds_fatal("Lexer error at %d:%d: unexpected character '%c'", line, col, c);
    }
    return make_tok(TOK_EOF, "", 0, line, col);
}

void token_free(Token *tok) {
    free(tok->lexeme);
    tok->lexeme = NULL;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_EOF: return "EOF";
        case TOK_IDENTIFIER: return "identifier";
        case TOK_INT: return "integer";
        case TOK_FLOAT: return "float";
        case TOK_STRING: return "string";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_COMMA: return ",";
        case TOK_DOT: return ".";
        case TOK_SEMICOLON: return ";";
        case TOK_QUESTION: return "?";
        case TOK_COLON: return ":";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_ASSIGN: return "=";
        case TOK_EQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LTE: return "<=";
        case TOK_GTE: return ">=";
        case TOK_ARROW: return "=>";
        case TOK_FUNC: return "func";
        case TOK_CREATE: return "create";
        case TOK_EXEC: return "exec";
        case TOK_VAR: return "var";
        case TOK_CLASS: return "class";
        case TOK_EXTENDS: return "extends";
        case TOK_IF: return "if";
        case TOK_ELIF: return "elif";
        case TOK_ELSE: return "else";
        case TOK_THEN: return "then";
        case TOK_WHILE: return "while";
        case TOK_FOR: return "for";
        case TOK_IN: return "in";
        case TOK_DO: return "do";
        case TOK_RETURN: return "return";
        case TOK_USE: return "use";
        case TOK_NEW: return "new";
        case TOK_TRY: return "try";
        case TOK_CATCH: return "catch";
        case TOK_FINALLY: return "finally";
        case TOK_LAMBDA: return "lambda";
        case TOK_CAST: return "cast";
        case TOK_CONST: return "const";
        case TOK_NULL: return "null";
        case TOK_TRUE: return "true";
        case TOK_FALSE: return "false";
    }
    return "unknown";
}
