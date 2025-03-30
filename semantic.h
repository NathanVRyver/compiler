#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parser.h"

/**
 * Symbol type enumeration
 */
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER
} SymbolType;

/**
 * Symbol table entry structure
 */
typedef struct SymbolEntry {
    char *name;              // Symbol name
    char *type;              // Symbol type (int, char, etc.)
    SymbolType symbol_type;  // What kind of symbol this is
    int is_initialized;      // Whether the variable is initialized
    int parameter_count;     // For functions, the number of parameters
    struct SymbolEntry *next;  // For chaining in the symbol table
} SymbolEntry;

/**
 * Symbol table scope structure
 */
typedef struct Scope {
    SymbolEntry *symbols;     // Linked list of symbols in this scope
    struct Scope *parent;     // Parent scope (for nested scopes)
    struct Scope **children;  // Child scopes
    int child_count;          // Number of child scopes
} Scope;

/**
 * Semantic analyzer structure
 */
typedef struct {
    Scope *global_scope;      // Global scope
    Scope *current_scope;     // Current scope being analyzed
    int has_error;            // Error flag
    char error_message[256];  // Error message
} SemanticAnalyzer;

/**
 * Initialize a semantic analyzer
 * 
 * @return A new semantic analyzer or NULL on error
 */
SemanticAnalyzer* init_semantic_analyzer(void);

/**
 * Free a semantic analyzer and all its resources
 * 
 * @param analyzer The analyzer to free
 */
void free_semantic_analyzer(SemanticAnalyzer *analyzer);

/**
 * Perform semantic analysis on an AST
 * 
 * @param analyzer The semantic analyzer
 * @param ast The AST to analyze
 * @return 1 on success, 0 on error
 */
int analyze_ast(SemanticAnalyzer *analyzer, ASTNode *ast);

/**
 * Enter a new scope
 * 
 * @param analyzer The semantic analyzer
 */
void enter_scope(SemanticAnalyzer *analyzer);

/**
 * Exit the current scope
 * 
 * @param analyzer The semantic analyzer
 */
void exit_scope(SemanticAnalyzer *analyzer);

/**
 * Declare a symbol in the current scope
 * 
 * @param analyzer The semantic analyzer
 * @param name Symbol name
 * @param type Symbol type
 * @param symbol_type Kind of symbol
 * @param is_initialized Whether it's initialized
 * @return 1 on success, 0 on error (e.g., redeclaration)
 */
int declare_symbol(SemanticAnalyzer *analyzer, const char *name, const char *type, 
                   SymbolType symbol_type, int is_initialized);

/**
 * Look up a symbol in the current and parent scopes
 * 
 * @param analyzer The semantic analyzer
 * @param name Symbol name to look up
 * @return Symbol entry or NULL if not found
 */
SymbolEntry* lookup_symbol(SemanticAnalyzer *analyzer, const char *name);

/**
 * Print the symbol table for debugging
 * 
 * @param analyzer The semantic analyzer
 */
void print_symbol_table(SemanticAnalyzer *analyzer);

#endif /* SEMANTIC_H */ 