#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static int analyze_node(SemanticAnalyzer *analyzer, ASTNode *node);
static void set_error(SemanticAnalyzer *analyzer, const char *message);

SemanticAnalyzer* init_semantic_analyzer(void) {
    SemanticAnalyzer *analyzer = (SemanticAnalyzer*)malloc(sizeof(SemanticAnalyzer));
    if (!analyzer) return NULL;

    // Initialize the global scope
    analyzer->global_scope = (Scope*)malloc(sizeof(Scope));
    if (!analyzer->global_scope) {
        free(analyzer);
        return NULL;
    }

    analyzer->global_scope->symbols = NULL;
    analyzer->global_scope->parent = NULL;
    analyzer->global_scope->children = NULL;
    analyzer->global_scope->child_count = 0;

    // Start with the global scope
    analyzer->current_scope = analyzer->global_scope;
    analyzer->has_error = 0;
    memset(analyzer->error_message, 0, sizeof(analyzer->error_message));

    return analyzer;
}

void free_semantic_analyzer(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;

    // Free the global scope and all its children
    // (This would require a separate function to recursively free scopes)
    
    free(analyzer);
}

int analyze_ast(SemanticAnalyzer *analyzer, ASTNode *ast) {
    if (!analyzer || !ast) return 0;

    // Reset the analyzer to the global scope
    analyzer->current_scope = analyzer->global_scope;
    analyzer->has_error = 0;

    // Analyze the root node (which should be a program node)
    return analyze_node(analyzer, ast);
}

static int analyze_node(SemanticAnalyzer *analyzer, ASTNode *node) {
    if (!node) return 1; // Nothing to analyze

    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode *program = (ProgramNode*)node;
            for (int i = 0; i < program->declaration_count; i++) {
                if (!analyze_node(analyzer, program->declarations[i])) {
                    return 0; // Error in a child node
                }
            }
            return 1;
        }

        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            
            // Declare the function in the current scope
            if (!declare_symbol(analyzer, func->name, func->return_type, 
                              SYMBOL_FUNCTION, 1)) {
                return 0; // Error (probably redeclaration)
            }

            // Enter a new scope for the function body
            enter_scope(analyzer);

            // Declare parameters in the function scope
            for (int i = 0; i < func->parameter_count; i++) {
                if (!declare_symbol(analyzer, func->parameters[i].name, 
                                  func->parameters[i].type, SYMBOL_PARAMETER, 1)) {
                    return 0; // Error (probably redeclaration)
                }
            }

            // Analyze the function body
            int result = analyze_node(analyzer, func->body);

            // Exit the function scope
            exit_scope(analyzer);

            return result;
        }

        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            
            // Check if there's an initializer
            int is_initialized = (var->initializer != NULL);
            
            // Analyze the initializer if it exists
            if (is_initialized && !analyze_node(analyzer, var->initializer)) {
                return 0; // Error in initializer
            }
            
            // Declare the variable
            return declare_symbol(analyzer, var->name, var->type, 
                                SYMBOL_VARIABLE, is_initialized);
        }

        case NODE_COMPOUND_STMT: {
            CompoundStmtNode *compound = (CompoundStmtNode*)node;
            
            // Enter a new scope for the compound statement
            enter_scope(analyzer);
            
            // Analyze all statements in the compound statement
            for (int i = 0; i < compound->statement_count; i++) {
                if (!analyze_node(analyzer, compound->statements[i])) {
                    exit_scope(analyzer); // Clean up before returning
                    return 0; // Error in a child statement
                }
            }
            
            // Exit the compound statement scope
            exit_scope(analyzer);
            
            return 1;
        }

        case NODE_IDENTIFIER: {
            IdentifierNode *identifier = (IdentifierNode*)node;
            
            // Look up the identifier in the symbol table
            SymbolEntry *entry = lookup_symbol(analyzer, identifier->name);
            if (!entry) {
                set_error(analyzer, "Undeclared identifier");
                return 0;
            }
            
            // Check if the variable is initialized
            if (entry->symbol_type == SYMBOL_VARIABLE && !entry->is_initialized) {
                set_error(analyzer, "Using uninitialized variable");
                return 0;
            }
            
            return 1;
        }

        // Handle other node types here
        
        default:
            // Unhandled node type (this should be expanded)
            return 1;
    }
}

void enter_scope(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;

    // Create a new scope
    Scope *new_scope = (Scope*)malloc(sizeof(Scope));
    if (!new_scope) {
        set_error(analyzer, "Out of memory");
        return;
    }

    new_scope->symbols = NULL;
    new_scope->parent = analyzer->current_scope;
    new_scope->children = NULL;
    new_scope->child_count = 0;

    // Add the new scope as a child of the current scope
    analyzer->current_scope->child_count++;
    analyzer->current_scope->children = realloc(
        analyzer->current_scope->children,
        analyzer->current_scope->child_count * sizeof(Scope*)
    );
    analyzer->current_scope->children[analyzer->current_scope->child_count - 1] = new_scope;

    // Make the new scope the current scope
    analyzer->current_scope = new_scope;
}

void exit_scope(SemanticAnalyzer *analyzer) {
    if (!analyzer || !analyzer->current_scope->parent) {
        // Cannot exit from global scope
        return;
    }

    // Move up to the parent scope
    analyzer->current_scope = analyzer->current_scope->parent;
}

int declare_symbol(SemanticAnalyzer *analyzer, const char *name, const char *type, 
                   SymbolType symbol_type, int is_initialized) {
    if (!analyzer || !name || !type) return 0;

    // Check if the symbol already exists in the current scope
    SymbolEntry *current = analyzer->current_scope->symbols;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            set_error(analyzer, "Redeclaration of symbol");
            return 0;
        }
        current = current->next;
    }

    // Create a new symbol entry
    SymbolEntry *entry = (SymbolEntry*)malloc(sizeof(SymbolEntry));
    if (!entry) {
        set_error(analyzer, "Out of memory");
        return 0;
    }

    entry->name = strdup(name);
    entry->type = strdup(type);
    entry->symbol_type = symbol_type;
    entry->is_initialized = is_initialized;
    entry->parameter_count = 0; // Set for functions if needed
    entry->next = analyzer->current_scope->symbols;
    analyzer->current_scope->symbols = entry;

    return 1;
}

SymbolEntry* lookup_symbol(SemanticAnalyzer *analyzer, const char *name) {
    if (!analyzer || !name) return NULL;

    // Start from the current scope and work upward
    Scope *scope = analyzer->current_scope;
    while (scope) {
        SymbolEntry *entry = scope->symbols;
        while (entry) {
            if (strcmp(entry->name, name) == 0) {
                return entry;
            }
            entry = entry->next;
        }
        scope = scope->parent;
    }

    return NULL; // Symbol not found
}

void print_symbol_table(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;

    printf("Symbol Table:\n");
    
    // This would require a separate function to recursively print scopes
    // For now, just print the current scope
    printf("Current Scope:\n");
    SymbolEntry *entry = analyzer->current_scope->symbols;
    while (entry) {
        printf("  %s: %s (%s, %s)\n", 
               entry->name, 
               entry->type, 
               entry->symbol_type == SYMBOL_VARIABLE ? "variable" : 
               entry->symbol_type == SYMBOL_FUNCTION ? "function" : "parameter",
               entry->is_initialized ? "initialized" : "uninitialized");
        entry = entry->next;
    }
}

static void set_error(SemanticAnalyzer *analyzer, const char *message) {
    analyzer->has_error = 1;
    snprintf(analyzer->error_message, sizeof(analyzer->error_message), "Semantic error: %s", message);
    fprintf(stderr, "%s\n", analyzer->error_message);
} 