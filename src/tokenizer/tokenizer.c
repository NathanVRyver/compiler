#include "../../include/tokenizer/tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function implementations for the tokenizer

Token get_next_token(FILE *file) {
    Token token;
    char c;
    int i = 0;

    // Skip whitespace and comments
    while (1) {
        c = fgetc(file);
        if (c == EOF) {
            token.type = TOKEN_EOF;
            token.value[0] = '\0';
            return token;
        }
        
        if (isspace(c)) {
            continue;
        }
        
        // Skip comments
        if (c == '/') {
            char next = fgetc(file);
            if (next == '/') {
                // Single-line comment
                while ((c = fgetc(file)) != EOF && c != '\n');
                continue;
            } else if (next == '*') {
                // Multi-line comment
                int done = 0;
                while (!done && (c = fgetc(file)) != EOF) {
                    if (c == '*') {
                        if ((c = fgetc(file)) == '/') {
                            done = 1;
                        } else {
                            ungetc(c, file);
                        }
                    }
                }
                continue;
            } else {
                ungetc(next, file);
                break;
            }
        } else {
            break;
        }
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
            // Handle escaped quotes
            if (c == '\\') {
                char next = fgetc(file);
                if (next == '"' || next == '\\' || next == 'n' || next == 't') {
                    token.value[i++] = c;
                    token.value[i++] = next;
                } else {
                    ungetc(next, file);
                    token.value[i++] = c;
                }
            } else {
                token.value[i++] = c;
            }
        }
        if (c == '"') {
            token.value[i++] = c;
        }
        token.value[i] = '\0';
        token.type = TOKEN_STRING;
    } else if (strchr("{}[]();,", c)) {
        // Punctuator
        token.value[i++] = c;
        token.value[i] = '\0';
        token.type = TOKEN_PUNCTUATOR;
    } else {
        // Operator - handle each case explicitly
        token.value[i++] = c;
        
        // Check for potentially multi-character operators
        if (c == '=') {
            // Could be '=' or '=='
            char next = fgetc(file);
            printf("DEBUG: '=' followed by '%c' (ASCII %d)\n", next, next);
            if (next == '=') {
                // This is '==' (equality)
                token.value[i++] = next;
                printf("DEBUG: Recognized as '=='\n");
            } else {
                // This is just '=' (assignment)
                ungetc(next, file);
                printf("DEBUG: Recognized as '='\n");
            }
        } else if (c == '!' || c == '<' || c == '>') {
            // Could be '!', '!=', '<', '<=', '>', or '>='
            char next = fgetc(file);
            if (next == '=') {
                token.value[i++] = next;
            } else {
                ungetc(next, file);
            }
        } else if (c == '+' || c == '-') {
            // Could be '+', '++', '-', or '--'
            char next = fgetc(file);
            if (next == c) {
                token.value[i++] = next;
            } else {
                ungetc(next, file);
            }
        } else if (c == '&' || c == '|') {
            // Could be '&', '&&', '|', or '||'
            char next = fgetc(file);
            if (next == c) {
                token.value[i++] = next;
            } else {
                ungetc(next, file);
            }
        }
        // All other operators (like '*', '/', etc.) are single character
        
        token.value[i] = '\0';
        token.type = TOKEN_OPERATOR;
    }

    return token;
}

int is_keyword(const char *str) {
    const char *keywords[] = {
        "int", "char", "void", "if", "else", "while", "for", "return",
        "struct", "typedef", "const", "unsigned", "signed", "break", "continue",
        "default", "switch", "case", "enum", "extern", "float", "double",
        "goto", "register", "short", "sizeof", "static", "union", "volatile"
    };
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
    // Print basic token info
    printf("Token: Type=%s, Value=", get_token_type_string(token.type));
    
    // Check for single equals sign (which sometimes gets displayed incorrectly)
    if (token.type == TOKEN_OPERATOR && 
        token.value[0] == '=' && 
        token.value[1] == '\0') {
        printf("= (assignment)");
    } else {
        // Print the token value normally
        printf("%s", token.value);
    }
    
    // Also print hex values for debugging
    printf(" [Hex: ");
    for (int i = 0; token.value[i] != '\0'; i++) {
        printf("%02X ", (unsigned char)token.value[i]);
    }
    printf("]");
    
    printf("\n");
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
