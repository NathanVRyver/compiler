#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parser/parser.h"
#include "../semantic/semantic.h"

#define MAX_VARS 1024
#define MAX_FUNCTIONS 128

typedef struct {
    char name[MAX_TOKEN_LEN];
    char reg[32];
    TypeInfo* type;
    int is_alloca;
} VarInfo;

typedef struct {
    char name[MAX_TOKEN_LEN];
    TypeInfo* return_type;
    int param_count;
    VarInfo params[16];
} FunctionInfo;

/**
 * Code generator structure
 */
typedef struct {
    FILE *output_file;         // Output file for the generated code
    int temp_counter;          // Counter for generating temporary variables
    int label_counter;         // Counter for generating labels
    int has_error;             // Error flag
    char error_message[256];   // Error message
    
    // Symbol tables for code generation
    VarInfo local_vars[MAX_VARS];
    int var_count;
    
    FunctionInfo functions[MAX_FUNCTIONS];
    int function_count;
    
    // Current function being generated
    FunctionInfo* current_function;
    
    // Optimization flags
    int optimize_level;
    int enable_inlining;
    int enable_constant_folding;
} CodeGenerator;

/**
 * Initialize a code generator
 * 
 * @param output_filename The file to write generated code to
 * @return A new code generator or NULL on error
 */
CodeGenerator* init_code_generator(const char *output_filename);

/**
 * Free a code generator and all its resources
 * 
 * @param generator The generator to free
 */
void free_code_generator(CodeGenerator *generator);

/**
 * Generate code from an AST
 * 
 * @param generator The code generator
 * @param ast The AST to generate code from
 * @return 1 on success, 0 on error
 */
int generate_code(CodeGenerator *generator, ASTNode *ast);

/**
 * Set optimization level
 * 
 * @param generator The code generator
 * @param level The optimization level to set
 */
void set_optimization_level(CodeGenerator* generator, int level);

/**
 * Generate a new temporary variable name
 * 
 * @param generator The code generator
 * @param buf Buffer to store the name in
 * @param size Size of the buffer
 */
void generate_temp(CodeGenerator *generator, char *buf, size_t size);

/**
 * Generate a new label name
 * 
 * @param generator The code generator
 * @param buf Buffer to store the name in
 * @param size Size of the buffer
 */
void generate_label(CodeGenerator *generator, char *buf, size_t size);

// Internal helper functions
void add_local_var(CodeGenerator* generator, const char* name, TypeInfo* type, int is_alloca);
VarInfo* find_var(CodeGenerator* generator, const char* name);
void add_function(CodeGenerator* generator, const char* name, TypeInfo* return_type);
FunctionInfo* find_function(CodeGenerator* generator, const char* name);

#endif /* CODEGEN_H */ 