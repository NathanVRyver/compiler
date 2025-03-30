#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdio.h>

// Maximum token length
#define MAX_TOKEN_LEN 100

/**
 * Enum for different types of tokens in C language
 */
enum TokenType {
    TOKEN_IDENTIFIER,  // Variable names, function names
    TOKEN_KEYWORD,     // C keywords like if, while, for, etc.
    TOKEN_NUMBER,      // Numeric literals
    TOKEN_STRING,      // String literals
    TOKEN_OPERATOR,    // Operators like +, -, *, /, =, etc.
    TOKEN_PUNCTUATOR,  // Punctuation marks like (, ), {, }, ;, etc.
    TOKEN_EOF          // End of file
};

/**
 * Structure to represent a token
 */
typedef struct Token {
    enum TokenType type;             // Type of the token
    char value[MAX_TOKEN_LEN];       // String value of the token
} Token;

/**
 * Extracts the next token from the input file
 * 
 * @param file The input file to read from
 * @return The next token found in the input
 */
Token get_next_token(FILE *file);

/**
 * Checks if a string is a C keyword
 * 
 * @param str The string to check
 * @return 1 if the string is a keyword, 0 otherwise
 */
int is_keyword(const char *str);

/**
 * Initializes the tokenizer with a file
 * 
 * @param filename The name of the file to tokenize
 * @return File pointer if successful, NULL otherwise
 */
FILE* init_tokenizer(const char *filename);

/**
 * Closes the tokenizer and frees resources
 * 
 * @param file The file pointer to close
 */
void close_tokenizer(FILE *file);

/**
 * Prints token information for debugging
 * 
 * @param token The token to print
 */
void print_token(Token token);

/**
 * Gets a string representation of a token type
 * 
 * @param type The token type
 * @return String representation of the token type
 */
const char* get_token_type_string(enum TokenType type);

#endif /* TOKENIZER_H */
