#include "../../include/semantic/semantic.h"
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
    
    analyzer->struct_types = NULL;
    analyzer->struct_type_count = 0;

    return analyzer;
}

void free_semantic_analyzer(SemanticAnalyzer *analyzer) {
    if (!analyzer) return;

    // Free the global scope and all its children
    // (This would require a separate function to recursively free scopes)
    
    free(analyzer->struct_types);
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

TypeInfo* create_basic_type(DataType type) {
    TypeInfo *info = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!info) return NULL;
    
    info->type = type;
    switch (type) {
        case TYPE_VOID: strcpy(info->name, "void"); break;
        case TYPE_INT: strcpy(info->name, "int"); break;
        case TYPE_CHAR: strcpy(info->name, "char"); break;
        default: strcpy(info->name, "unknown");
    }
    
    return info;
}

TypeInfo* create_pointer_type(TypeInfo *base) {
    TypeInfo *info = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!info) return NULL;
    
    info->type = TYPE_POINTER;
    info->base_type = base;
    snprintf(info->name, MAX_TYPE_NAME, "%s*", base->name);
    
    return info;
}

TypeInfo* create_array_type(TypeInfo *base, int size) {
    TypeInfo *info = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!info) return NULL;
    
    info->type = TYPE_ARRAY;
    info->base_type = base;
    info->array_size = size;
    snprintf(info->name, MAX_TYPE_NAME, "%s[%d]", base->name, size);
    
    return info;
}

TypeInfo* create_struct_type(const char *name) {
    TypeInfo *info = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!info) return NULL;
    
    info->type = TYPE_STRUCT;
    snprintf(info->name, MAX_TYPE_NAME, "struct %s", name);
    info->field_count = 0;
    
    return info;
}

TypeInfo* find_struct_type(SemanticAnalyzer *analyzer, const char *name) {
    for (int i = 0; i < analyzer->struct_type_count; i++) {
        if (strcmp(analyzer->struct_types[i].name + 7, name) == 0) {
            return &analyzer->struct_types[i];
        }
    }
    return NULL;
}

int add_struct_field(TypeInfo *struct_type, const char *name, TypeInfo *type) {
    if (struct_type->type != TYPE_STRUCT || struct_type->field_count >= MAX_STRUCT_FIELDS) {
        return 0;
    }
    
    for (int i = 0; i < struct_type->field_count; i++) {
        if (strcmp(struct_type->fields[i].name, name) == 0) {
            return 0; // Field already exists
        }
    }
    
    strcpy(struct_type->fields[struct_type->field_count].name, name);
    struct_type->fields[struct_type->field_count].type = type;
    struct_type->field_count++;
    
    return 1;
}

static TypeInfo* get_type_from_string(SemanticAnalyzer *analyzer, const char *type_str) {
    if (strcmp(type_str, "int") == 0) {
        return create_basic_type(TYPE_INT);
    } else if (strcmp(type_str, "char") == 0) {
        return create_basic_type(TYPE_CHAR);
    } else if (strcmp(type_str, "void") == 0) {
        return create_basic_type(TYPE_VOID);
    } else if (strncmp(type_str, "struct ", 7) == 0) {
        return find_struct_type(analyzer, type_str + 7);
    }
    
    return NULL;
}

static int analyze_node(SemanticAnalyzer *analyzer, ASTNode *node) {
    if (!node) return 1;

    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode *program = (ProgramNode*)node;
            for (int i = 0; i < program->declaration_count; i++) {
                if (!analyze_node(analyzer, program->declarations[i])) {
                    return 0;
                }
            }
            return 1;
        }

        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            
            TypeInfo *return_type = get_type_from_string(analyzer, func->return_type);
            if (!return_type) {
                set_error(analyzer, "Unknown return type");
                return 0;
            }
            
            TypeInfo **param_types = NULL;
            if (func->parameter_count > 0) {
                param_types = (TypeInfo**)malloc(func->parameter_count * sizeof(TypeInfo*));
                for (int i = 0; i < func->parameter_count; i++) {
                    param_types[i] = get_type_from_string(analyzer, func->parameters[i].type);
                    if (!param_types[i]) {
                        set_error(analyzer, "Unknown parameter type");
                        free(param_types);
                        return 0;
                    }
                }
            }
            
            if (!declare_function(analyzer, func->name, return_type, param_types, func->parameter_count)) {
                set_error(analyzer, "Function redeclaration");
                free(param_types);
                return 0;
            }
            
            if (func->body) {
                enter_scope(analyzer);
                
                for (int i = 0; i < func->parameter_count; i++) {
                    if (!declare_symbol(analyzer, func->parameters[i].name, param_types[i], 
                                      SYMBOL_PARAMETER, 1)) {
                        free(param_types);
                        return 0;
                    }
                }
                
                int result = analyze_node(analyzer, func->body);
                exit_scope(analyzer);
                free(param_types);
                return result;
            }
            
            free(param_types);
            return 1;
        }

        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            
            TypeInfo *type = get_type_from_string(analyzer, var->type);
            if (!type) {
                set_error(analyzer, "Unknown variable type");
                return 0;
            }
            
            // Variables without initializers are considered initialized for our compiler
            // This allows declarations like "int x;" to work without errors
            int is_initialized = 1;
            
            if (var->initializer && !analyze_node(analyzer, var->initializer)) {
                return 0;
            }
            
            return declare_symbol(analyzer, var->name, type, SYMBOL_VARIABLE, is_initialized);
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
                // For debugging, print the name of the undeclared identifier
                char error_message[100];
                snprintf(error_message, sizeof(error_message), 
                        "Undeclared identifier: %s", identifier->name);
                set_error(analyzer, error_message);
                return 0;
            }
            
            // Check if the variable is initialized (commented out for now to simplify)
            // if (entry->symbol_type == SYMBOL_VARIABLE && !entry->is_initialized) {
            //     set_error(analyzer, "Using uninitialized variable");
            //     return 0;
            // }
            
            return 1;
        }

        case NODE_IF_STMT: {
            IfStmtNode *if_stmt = (IfStmtNode*)node;
            
            if (!analyze_node(analyzer, if_stmt->condition)) {
                return 0;
            }
            
            if (!analyze_node(analyzer, if_stmt->then_branch)) {
                return 0;
            }
            
            if (if_stmt->else_branch && !analyze_node(analyzer, if_stmt->else_branch)) {
                return 0;
            }
            
            return 1;
        }
        
        case NODE_WHILE_STMT: {
            WhileStmtNode *while_stmt = (WhileStmtNode*)node;
            
            if (!analyze_node(analyzer, while_stmt->condition)) {
                return 0;
            }
            
            return analyze_node(analyzer, while_stmt->body);
        }
        
        case NODE_FOR_STMT: {
            ForStmtNode *for_stmt = (ForStmtNode*)node;
            
            enter_scope(analyzer);
            
            if (for_stmt->initializer && !analyze_node(analyzer, for_stmt->initializer)) {
                exit_scope(analyzer);
                return 0;
            }
            
            if (for_stmt->condition && !analyze_node(analyzer, for_stmt->condition)) {
                exit_scope(analyzer);
                return 0;
            }
            
            if (for_stmt->increment && !analyze_node(analyzer, for_stmt->increment)) {
                exit_scope(analyzer);
                return 0;
            }
            
            int result = analyze_node(analyzer, for_stmt->body);
            exit_scope(analyzer);
            return result;
        }
        
        case NODE_RETURN_STMT: {
            ReturnStmtNode *return_stmt = (ReturnStmtNode*)node;
            
            if (return_stmt->value && !analyze_node(analyzer, return_stmt->value)) {
                return 0;
            }
            
            return 1;
        }

        case NODE_BINARY_EXPR: {
            BinaryExprNode *binary = (BinaryExprNode*)node;
            
            if (!analyze_node(analyzer, binary->left) || 
                !analyze_node(analyzer, binary->right)) {
                return 0;
            }
            
            return 1;
        }
        
        case NODE_UNARY_EXPR: {
            UnaryExprNode *unary = (UnaryExprNode*)node;
            
            return analyze_node(analyzer, unary->operand);
        }
        
        case NODE_CALL_EXPR: {
            CallExprNode *call = (CallExprNode*)node;
            
            SymbolEntry *entry = lookup_symbol(analyzer, call->callee);
            if (!entry) {
                set_error(analyzer, "Undeclared function");
                return 0;
            }
            
            if (entry->symbol_type != SYMBOL_FUNCTION) {
                set_error(analyzer, "Called object is not a function");
                return 0;
            }
            
            if (entry->parameter_count != call->argument_count) {
                set_error(analyzer, "Wrong number of arguments");
                return 0;
            }
            
            for (int i = 0; i < call->argument_count; i++) {
                if (!analyze_node(analyzer, call->arguments[i])) {
                    return 0;
                }
            }
            
            return 1;
        }
        
        case NODE_ASSIGNMENT_EXPR: {
            AssignmentExprNode *assign = (AssignmentExprNode*)node;
            
            if (!analyze_node(analyzer, assign->target) || 
                !analyze_node(analyzer, assign->value)) {
                return 0;
            }
            
            if (assign->target->type == NODE_IDENTIFIER) {
                IdentifierNode *id = (IdentifierNode*)assign->target;
                SymbolEntry *entry = lookup_symbol(analyzer, id->name);
                if (entry) {
                    entry->is_initialized = 1;
                }
            }
            
            return 1;
        }
        
        case NODE_EXPRESSION_STMT: {
            ExprStmtNode *expr = (ExprStmtNode*)node;
            
            if (expr->expression) {
                return analyze_node(analyzer, expr->expression);
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

int declare_symbol(SemanticAnalyzer *analyzer, const char *name, TypeInfo *type, 
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
    entry->type_info = type;
    entry->symbol_type = symbol_type;
    entry->is_initialized = is_initialized;
    entry->parameter_count = 0; // Set for functions if needed
    entry->param_types = NULL;
    entry->next = analyzer->current_scope->symbols;
    analyzer->current_scope->symbols = entry;

    return 1;
}

int declare_function(SemanticAnalyzer *analyzer, const char *name, TypeInfo *return_type,
                    TypeInfo **param_types, int param_count) {
    if (!declare_symbol(analyzer, name, return_type, SYMBOL_FUNCTION, 1)) {
        return 0;
    }
    
    SymbolEntry *entry = analyzer->current_scope->symbols;
    entry->parameter_count = param_count;
    
    if (param_count > 0) {
        entry->param_types = (TypeInfo**)malloc(param_count * sizeof(TypeInfo*));
        for (int i = 0; i < param_count; i++) {
            entry->param_types[i] = param_types[i];
        }
    }
    
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
               entry->type_info->name, 
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