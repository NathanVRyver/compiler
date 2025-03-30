#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parser/parser.h"

/**
 * Code generator structure
 */
typedef struct {
    FILE *output_file;         // Output file for the generated code
    int temp_counter;          // Counter for generating temporary variables
    int label_counter;         // Counter for generating labels
    int has_error;             // Error flag
    char error_message[256];   // Error message
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

#endif /* CODEGEN_H */ 