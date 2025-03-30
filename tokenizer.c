#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function implementations for the tokenizer

Token get_next_token(FILE *file) {
    Token token;
    char c;
    int i = 0;

    // Skip whitespace
    while ((c = fgetc(file)) != EOF && isspace(c));

    if (c == EOF) {
        token.type = TOKEN_EOF;
        token.value[0] = '\0';
        return token;
    }

    if (isalpha(c) || c == '_') {
        // Identifier or keyword
        token.value[i++] = c;
        while ((c = fgetc(file)) != EOF && (isalnum(c) || c == '_')) {
            token.value[i++] = c;
        }
        ungetc(c, file);
        token.value[i] = '\0';
        token.type = is_keyword(token.value) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
    } else if (isdigit(c)) {
        // Number
        token.value[i++] = c;
        while ((c = fgetc(file)) != EOF && isdigit(c)) {
            token.value[i++] = c;
        }
        ungetc(c, file);
        token.value[i] = '\0';
        token.type = TOKEN_NUMBER;
    } else if (c == '"') {
        // String
        token.value[i++] = c;
        while ((c = fgetc(file)) != EOF && c != '"') {
            token.value[i++] = c;
        }
        token.value[i++] = '"';
        token.value[i] = '\0';
        token.type = TOKEN_STRING;
    } else if (strchr("{}[]();,", c)) {
        // Punctuator
        token.value[i++] = c;
        token.value[i] = '\0';
        token.type = TOKEN_PUNCTUATOR;
    } else {
        // Operator
        token.value[i++] = c;
        // Handle multi-character operators like ==, !=, <=, >=, etc.
        if (c == '=' || c == '!' || c == '<' || c == '>' || c == '+' || c == '-' || c == '*' || c == '/' || c == '&' || c == '|') {
            char next_c = fgetc(file);
            if (next_c == '=' || 
                ((c == '+' || c == '-' || c == '&' || c == '|') && next_c == c)) {
                token.value[i++] = next_c;
            } else {
                ungetc(next_c, file);
            }
        }
        token.value[i] = '\0';
        token.type = TOKEN_OPERATOR;
    }

    return token;
}

int is_keyword(const char *str) {
    const char *keywords[] = {"int", "char", "void", "if", "else", "while", "for", "return"};
    int num_keywords = sizeof(keywords) / sizeof(keywords[0]);
    
    for (int i = 0; i < num_keywords; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

FILE* init_tokenizer(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
    }
    return file;
}

void close_tokenizer(FILE *file) {
    if (file) {
        fclose(file);
    }
}

void print_token(Token token) {
    printf("Token: Type=%s, Value=%s\n", 
           get_token_type_string(token.type), 
           token.value);
}

const char* get_token_type_string(enum TokenType type) {
    switch (type) {
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_KEYWORD: return "KEYWORD";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_OPERATOR: return "OPERATOR";
        case TOKEN_PUNCTUATOR: return "PUNCTUATOR";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}
