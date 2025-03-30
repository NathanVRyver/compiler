#ifndef PARSER_H
#define PARSER_H

#include "../tokenizer/tokenizer.h"
#include <stdlib.h>

/**
 * Types of AST nodes in our syntax tree
 */
typedef enum {
    NODE_PROGRAM,         // Root of the program
    NODE_FUNCTION_DECL,   // Function declaration
    NODE_VARIABLE_DECL,   // Variable declaration
    NODE_COMPOUND_STMT,   // Compound statement (block)
    NODE_EXPRESSION_STMT, // Expression statement
    NODE_IF_STMT,         // If statement
    NODE_WHILE_STMT,      // While statement
    NODE_FOR_STMT,        // For statement
    NODE_RETURN_STMT,     // Return statement
    NODE_BINARY_EXPR,     // Binary expression (a + b)
    NODE_UNARY_EXPR,      // Unary expression (!a, -b)
    NODE_CALL_EXPR,       // Function call expression
    NODE_IDENTIFIER,      // Identifier reference
    NODE_NUMBER_LITERAL,  // Number literal
    NODE_STRING_LITERAL,  // String literal
    NODE_ASSIGNMENT_EXPR  // Assignment expression
} ASTNodeType;

/**
 * Base structure for AST nodes
 */
typedef struct ASTNode {
    ASTNodeType type;  // Type of the node
    struct ASTNode *parent;  // Parent node
} ASTNode;

/**
 * Program node (root of the AST)
 */
typedef struct {
    ASTNode base;
    struct ASTNode **declarations;  // Array of top-level declarations
    int declaration_count;
} ProgramNode;

/**
 * Function declaration node
 */
typedef struct {
    ASTNode base;
    char *name;             // Function name
    char *return_type;      // Return type
    struct {
        char *type;
        char *name;
    } *parameters;          // Array of parameter type and name
    int parameter_count;
    struct ASTNode *body;   // Function body
} FunctionDeclNode;

/**
 * Variable declaration node
 */
typedef struct {
    ASTNode base;
    char *type;           // Variable type
    char *name;           // Variable name
    struct ASTNode *initializer;  // Initial value
} VariableDeclNode;

/**
 * Compound statement node (block)
 */
typedef struct {
    ASTNode base;
    struct ASTNode **statements;  // Array of statements
    int statement_count;
} CompoundStmtNode;

/**
 * Expression statement node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *expression;  // Expression
} ExprStmtNode;

/**
 * If statement node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *condition;    // Condition expression
    struct ASTNode *then_branch;  // Then branch
    struct ASTNode *else_branch;  // Else branch (can be NULL)
} IfStmtNode;

/**
 * While statement node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *condition;  // Condition expression
    struct ASTNode *body;       // Loop body
} WhileStmtNode;

/**
 * For statement node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *initializer;  // Initializer (can be NULL)
    struct ASTNode *condition;    // Condition (can be NULL)
    struct ASTNode *increment;    // Increment (can be NULL)
    struct ASTNode *body;         // Loop body
} ForStmtNode;

/**
 * Return statement node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *value;  // Return value (can be NULL)
} ReturnStmtNode;

/**
 * Binary expression node
 */
typedef struct {
    ASTNode base;
    char operator[MAX_TOKEN_LEN];  // Operator token value
    struct ASTNode *left;         // Left operand
    struct ASTNode *right;        // Right operand
} BinaryExprNode;

/**
 * Unary expression node
 */
typedef struct {
    ASTNode base;
    char operator[MAX_TOKEN_LEN];  // Operator token value
    struct ASTNode *operand;      // Operand
} UnaryExprNode;

/**
 * Function call expression node
 */
typedef struct {
    ASTNode base;
    char *callee;             // Function name
    struct ASTNode **arguments;  // Array of arguments
    int argument_count;
} CallExprNode;

/**
 * Identifier reference node
 */
typedef struct {
    ASTNode base;
    char name[MAX_TOKEN_LEN];  // Identifier name
} IdentifierNode;

/**
 * Number literal node
 */
typedef struct {
    ASTNode base;
    char value[MAX_TOKEN_LEN];  // Number as string
} NumberLiteralNode;

/**
 * String literal node
 */
typedef struct {
    ASTNode base;
    char value[MAX_TOKEN_LEN];  // String value
} StringLiteralNode;

/**
 * Assignment expression node
 */
typedef struct {
    ASTNode base;
    struct ASTNode *target;  // Target to assign to
    struct ASTNode *value;   // Value to assign
} AssignmentExprNode;

/**
 * Parser state structure
 */
typedef struct {
    FILE *file;             // Input file
    Token current_token;    // Current token
    Token previous_token;   // Previous token
    int has_error;          // Error flag
    char error_message[256];  // Error message
} Parser;

/**
 * Initialize the parser with an input file
 * 
 * @param filename The name of the file to parse
 * @return Parser state or NULL on error
 */
Parser* init_parser(const char *filename);

/**
 * Free all resources used by the parser
 * 
 * @param parser The parser to free
 */
void free_parser(Parser *parser);

/**
 * Parse a C program and build an AST
 * 
 * @param parser The parser state
 * @return Root node of the AST or NULL on error
 */
ASTNode* parse_program(Parser *parser);

/**
 * Free an AST node and all its children
 * 
 * @param node The node to free
 */
void free_ast_node(ASTNode *node);

/**
 * Print the AST for debugging purposes
 * 
 * @param node The root node of the AST
 * @param indent Indentation level
 */
void print_ast(ASTNode *node, int indent);

#endif /* PARSER_H */ 