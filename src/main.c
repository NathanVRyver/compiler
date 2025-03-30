#include "../include/tokenizer/tokenizer.h"
#include "../include/parser/parser.h"
#include "../include/semantic/semantic.h"
#include "../include/codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *program_name) {
    printf("Usage: %s <input_file> [output_file] [-v]\n", program_name);
    printf("  input_file   - C source file to compile\n");
    printf("  output_file  - Optional output file for LLVM IR (default: output.ll)\n");
    printf("  -v           - Verbose output (displays tokens and debug info)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input_filename = NULL;
    const char *output_filename = "output.ll";
    int verbose = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (input_filename == NULL) {
            input_filename = argv[i];
        } else {
            output_filename = argv[i];
        }
    }
    
    if (input_filename == NULL) {
        fprintf(stderr, "Error: No input file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Initialize the parser
    Parser *parser = init_parser(input_filename);
    if (!parser) {
        fprintf(stderr, "Error: Failed to initialize parser\n");
        return 1;
    }
    
    // Show tokens if in verbose mode
    if (verbose) {
        FILE *file = fopen(input_filename, "r");
        if (file) {
            printf("\nTokens from lexical analysis:\n");
            Token token;
            do {
                token = get_next_token(file);
                print_token(token);
            } while (token.type != TOKEN_EOF);
            fclose(file);
        }
    }
    
    // Parse the input file
    printf("Parsing %s...\n", input_filename);
    ASTNode *ast = parse_program(parser);
    if (!ast || parser->has_error) {
        fprintf(stderr, "Error: Parsing failed\n");
        free_parser(parser);
        return 1;
    }
    
    printf("Parsing successful!\n");
    
    // Print the AST (for debugging)
    printf("\nAbstract Syntax Tree:\n");
    print_ast(ast, 0);
    
    // Initialize the semantic analyzer
    SemanticAnalyzer *analyzer = init_semantic_analyzer();
    if (!analyzer) {
        fprintf(stderr, "Error: Failed to initialize semantic analyzer\n");
        free_ast_node(ast);
        free_parser(parser);
        return 1;
    }
    
    // Perform semantic analysis
    printf("\nPerforming semantic analysis...\n");
    if (!analyze_ast(analyzer, ast)) {
        fprintf(stderr, "Error: Semantic analysis failed\n");
        free_semantic_analyzer(analyzer);
        free_ast_node(ast);
        free_parser(parser);
        return 1;
    }
    
    printf("Semantic analysis successful!\n");
    
    if (verbose) {
        printf("\nSymbol Table:\n");
        print_symbol_table(analyzer);
    }
    
    // Initialize the code generator
    CodeGenerator *generator = init_code_generator(output_filename);
    if (!generator) {
        fprintf(stderr, "Error: Failed to initialize code generator\n");
        free_semantic_analyzer(analyzer);
        free_ast_node(ast);
        free_parser(parser);
        return 1;
    }
    
    // Set optimization level (0-3)
    set_optimization_level(generator, 1);
    
    // Generate code
    printf("\nGenerating code to %s...\n", output_filename);
    if (!generate_code(generator, ast)) {
        fprintf(stderr, "Error: Code generation failed\n");
        free_code_generator(generator);
        free_semantic_analyzer(analyzer);
        free_ast_node(ast);
        free_parser(parser);
        return 1;
    }
    
    printf("Code generation successful!\n");
    
    if (verbose) {
        printf("\nTo run the generated LLVM IR, use:\n");
        printf("  llc %s -o output.s\n", output_filename);
        printf("  gcc output.s -o output\n");
        printf("  ./output\n");
    }
    
    // Clean up
    free_code_generator(generator);
    free_semantic_analyzer(analyzer);
    free_ast_node(ast);
    free_parser(parser);
    
    printf("\nCompilation completed successfully!\n");
    return 0;
}
