#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"

#define MAX_STRUCT_FIELDS 64
#define MAX_TYPE_NAME 64

typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT
} DataType;

typedef struct TypeInfo {
    DataType type;
    char name[MAX_TYPE_NAME];
    struct TypeInfo* base_type;   // For pointers and arrays
    int array_size;               // For arrays
    struct StructField {
        char name[MAX_TOKEN_LEN]; // IDK IF THIS IS NEEDED
        struct TypeInfo* type;
    } fields[MAX_STRUCT_FIELDS];  // For structs
    int field_count;              // For structs
} TypeInfo;

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_STRUCT_TYPE
} SymbolType;

typedef struct SymbolEntry {
    char* name;
    TypeInfo* type_info;
    SymbolType symbol_type;
    int is_initialized;
    int parameter_count;      // For functions
    TypeInfo** param_types;   // For functions
    struct SymbolEntry* next;
} SymbolEntry;

typedef struct Scope {
    SymbolEntry* symbols;    
    struct Scope* parent;     
    struct Scope** children;  
    int child_count;          
} Scope;

typedef struct {
    Scope* global_scope;     
    Scope* current_scope;    
    int has_error;           
    char error_message[256]; 
    TypeInfo* struct_types;  // Registry of struct types
    int struct_type_count;
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
void free_semantic_analyzer(SemanticAnalyzer* analyzer);

/**
 * Perform semantic analysis on an AST
 * 
 * @param analyzer The semantic analyzer
 * @param ast The AST to analyze
 * @return 1 on success, 0 on error
 */
int analyze_ast(SemanticAnalyzer* analyzer, ASTNode* ast);

/**
 * Enter a new scope
 * 
 * @param analyzer The semantic analyzer
 */
void enter_scope(SemanticAnalyzer* analyzer);

/**
 * Exit the current scope
 * 
 * @param analyzer The semantic analyzer
 */
void exit_scope(SemanticAnalyzer* analyzer);

TypeInfo* create_basic_type(DataType type);
TypeInfo* create_pointer_type(TypeInfo* base);
TypeInfo* create_array_type(TypeInfo* base, int size);
TypeInfo* create_struct_type(const char* name);
TypeInfo* find_struct_type(SemanticAnalyzer* analyzer, const char* name);
int add_struct_field(TypeInfo* struct_type, const char* name, TypeInfo* type);

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
int declare_symbol(SemanticAnalyzer* analyzer, const char* name, TypeInfo* type, 
                   SymbolType symbol_type, int is_initialized);

/**
 * Declare a function in the current scope
 * 
 * @param analyzer The semantic analyzer
 * @param name Function name
 * @param return_type Function return type
 * @param param_types Function parameter types
 * @param param_count Number of parameters
 * @return 1 on success, 0 on error
 */
int declare_function(SemanticAnalyzer* analyzer, const char* name, TypeInfo* return_type,
                     TypeInfo** param_types, int param_count);

/**
 * Look up a symbol in the current and parent scopes
 * 
 * @param analyzer The semantic analyzer
 * @param name Symbol name to look up
 * @return Symbol entry or NULL if not found
 */
SymbolEntry* lookup_symbol(SemanticAnalyzer* analyzer, const char* name);

/**
 * Print the symbol table for debugging
 * 
 * @param analyzer The semantic analyzer
 */
void print_symbol_table(SemanticAnalyzer* analyzer);

int types_compatible(TypeInfo* left, TypeInfo* right);
TypeInfo* result_type(TypeInfo* left, TypeInfo* right, const char* op);

#endif /* SEMANTIC_H */ 