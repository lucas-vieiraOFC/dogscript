#ifndef DOGSCRIPT_LEXER_H
#define DOGSCRIPT_LEXER_H

#include <stddef.h>

typedef enum {
    TOK_EOF = 0,
    TOK_IDENTIFIER,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,

    TOK_LPAREN,      /* ( */
    TOK_RPAREN,      /* ) */
    TOK_LBRACE,      /* { */
    TOK_RBRACE,      /* } */
    TOK_LBRACKET,    /* [ */
    TOK_RBRACKET,    /* ] */
    TOK_COMMA,       /* , */
    TOK_DOT,         /* . */
    TOK_SEMICOLON,   /* ; */
    TOK_QUESTION,    /* ? */
    TOK_COLON,       /* : */

    TOK_PLUS,        /* + */
    TOK_MINUS,       /* - */
    TOK_STAR,        /* * */
    TOK_SLASH,       /* / */
    TOK_ASSIGN,      /* = */
    TOK_EQ,          /* == */
    TOK_NEQ,         /* != */
    TOK_LT,          /* < */
    TOK_GT,          /* > */
    TOK_LTE,         /* <= */
    TOK_GTE,         /* >= */
    TOK_ARROW,       /* => */

    TOK_FUNC,
    TOK_CREATE,
    TOK_EXEC,
    TOK_VAR,
    TOK_CLASS,
    TOK_EXTENDS,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_THEN,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_DO,
    TOK_RETURN,
    TOK_USE,
    TOK_NEW,
    TOK_TRY,
    TOK_CATCH,
    TOK_FINALLY,
    TOK_LAMBDA,
    TOK_CAST,
    TOK_CONST,
    TOK_NULL,
    TOK_TRUE,
    TOK_FALSE
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int column;
} Lexer;

void lexer_init(Lexer *lx, const char *src);
Token lexer_next(Lexer *lx);
void token_free(Token *tok);
const char *token_type_name(TokenType type);

#endif
