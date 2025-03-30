#include "../../include/codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static int generate_node(CodeGenerator *generator, ASTNode *node);
static void set_error(CodeGenerator *generator, const char *message);

CodeGenerator* init_code_generator(const char *output_filename) {
    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Could not open output file %s\n", output_filename);
        return NULL;
    }

    CodeGenerator *generator = (CodeGenerator*)malloc(sizeof(CodeGenerator));
    if (!generator) {
        fclose(output_file);
        return NULL;
    }

    generator->output_file = output_file;
    generator->temp_counter = 0;
    generator->label_counter = 0;
    generator->has_error = 0;
    memset(generator->error_message, 0, sizeof(generator->error_message));

    // Write LLVM IR module header
    fprintf(output_file, "; LLVM IR Generated Code\n");
    fprintf(output_file, "target triple = \"x86_64-unknown-linux-gnu\"\n\n");

    // Write declarations for C standard library functions
    fprintf(output_file, "declare i32 @printf(i8* nocapture readonly, ...)\n");
    fprintf(output_file, "declare i32 @scanf(i8* nocapture readonly, ...)\n\n");

    return generator;
}

void free_code_generator(CodeGenerator *generator) {
    if (generator) {
        if (generator->output_file) {
            fclose(generator->output_file);
        }
        free(generator);
    }
}

int generate_code(CodeGenerator *generator, ASTNode *ast) {
    if (!generator || !ast) return 0;

    generator->has_error = 0;
    return generate_node(generator, ast);
}

static int generate_node(CodeGenerator *generator, ASTNode *node) {
    if (!node) return 1; // Nothing to generate

    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode *program = (ProgramNode*)node;
            for (int i = 0; i < program->declaration_count; i++) {
                if (!generate_node(generator, program->declarations[i])) {
                    return 0; // Error in a child node
                }
            }
            return 1;
        }

        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            FILE *output = generator->output_file;

            // Generate function declaration
            fprintf(output, "define %s @%s(", 
                    strcmp(func->return_type, "void") == 0 ? "void" : "i32", 
                    func->name);

            // Generate parameters
            for (int i = 0; i < func->parameter_count; i++) {
                if (i > 0) fprintf(output, ", ");
                fprintf(output, "i32 %%%s", func->parameters[i].name);
            }
            fprintf(output, ") {\n");

            // Generate entry block
            fprintf(output, "entry:\n");

            // Generate function body
            if (func->body && !generate_node(generator, func->body)) {
                return 0; // Error in the function body
            }

            // Add a default return if the function returns a value
            if (strcmp(func->return_type, "void") != 0) {
                fprintf(output, "  ret i32 0\n");
            } else {
                fprintf(output, "  ret void\n");
            }

            fprintf(output, "}\n\n");
            return 1;
        }

        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            FILE *output = generator->output_file;

            // Allocate space for the variable on the stack
            fprintf(output, "  %%%s = alloca i32\n", var->name);

            // Generate code for initializer if it exists
            if (var->initializer) {
                if (!generate_node(generator, var->initializer)) {
                    return 0; // Error in initializer
                }

                // Store the initializer value in the variable
                char temp[32];
                generate_temp(generator, temp, sizeof(temp));
                fprintf(output, "  store i32 %%%s, i32* %%%s\n", temp, var->name);
            } else {
                // Default initialize to 0
                fprintf(output, "  store i32 0, i32* %%%s\n", var->name);
            }

            return 1;
        }

        case NODE_COMPOUND_STMT: {
            CompoundStmtNode *compound = (CompoundStmtNode*)node;
            
            // Generate code for all statements in the compound statement
            for (int i = 0; i < compound->statement_count; i++) {
                if (!generate_node(generator, compound->statements[i])) {
                    return 0; // Error in a child statement
                }
            }
            
            return 1;
        }

        case NODE_EXPRESSION_STMT: {
            ExprStmtNode *expr_stmt = (ExprStmtNode*)node;
            
            // Generate code for the expression if it exists
            if (expr_stmt->expression) {
                return generate_node(generator, expr_stmt->expression);
            }
            
            return 1; // Empty expression statement
        }

        case NODE_IF_STMT: {
            IfStmtNode *if_stmt = (IfStmtNode*)node;
            FILE *output = generator->output_file;
            
            // Generate code for the condition
            if (if_stmt->condition && !generate_node(generator, if_stmt->condition)) {
                return 0; // Error in condition
            }
            
            // Get the result of the condition
            char condition_reg[32];
            generate_temp(generator, condition_reg, sizeof(condition_reg));
            
            // Generate labels for the then, else, and end blocks
            char then_label[32], else_label[32], end_label[32];
            generate_label(generator, then_label, sizeof(then_label));
            generate_label(generator, else_label, sizeof(else_label));
            generate_label(generator, end_label, sizeof(end_label));
            
            // Generate the branch instruction
            fprintf(output, "  br i1 %%%s, label %%%s, label %%%s\n", 
                    condition_reg, then_label, 
                    if_stmt->else_branch ? else_label : end_label);
            
            // Generate the then block
            fprintf(output, "%s:\n", then_label);
            if (if_stmt->then_branch && !generate_node(generator, if_stmt->then_branch)) {
                return 0; // Error in then branch
            }
            fprintf(output, "  br label %%%s\n", end_label);
            
            // Generate the else block if it exists
            if (if_stmt->else_branch) {
                fprintf(output, "%s:\n", else_label);
                if (!generate_node(generator, if_stmt->else_branch)) {
                    return 0; // Error in else branch
                }
                fprintf(output, "  br label %%%s\n", end_label);
            }
            
            // Generate the end block
            fprintf(output, "%s:\n", end_label);
            
            return 1;
        }

        case NODE_WHILE_STMT: {
            WhileStmtNode *while_stmt = (WhileStmtNode*)node;
            FILE *output = generator->output_file;
            
            // Generate labels for the condition, body, and end blocks
            char cond_label[32], body_label[32], end_label[32];
            generate_label(generator, cond_label, sizeof(cond_label));
            generate_label(generator, body_label, sizeof(body_label));
            generate_label(generator, end_label, sizeof(end_label));
            
            // Generate branch to condition
            fprintf(output, "  br label %%%s\n", cond_label);
            
            // Generate the condition block
            fprintf(output, "%s:\n", cond_label);
            if (while_stmt->condition && !generate_node(generator, while_stmt->condition)) {
                return 0; // Error in condition
            }
            
            // Get the result of the condition
            char condition_reg[32];
            generate_temp(generator, condition_reg, sizeof(condition_reg));
            
            // Generate the branch instruction
            fprintf(output, "  br i1 %%%s, label %%%s, label %%%s\n", 
                    condition_reg, body_label, end_label);
            
            // Generate the body block
            fprintf(output, "%s:\n", body_label);
            if (while_stmt->body && !generate_node(generator, while_stmt->body)) {
                return 0; // Error in body
            }
            fprintf(output, "  br label %%%s\n", cond_label);
            
            // Generate the end block
            fprintf(output, "%s:\n", end_label);
            
            return 1;
        }

        case NODE_RETURN_STMT: {
            ReturnStmtNode *return_stmt = (ReturnStmtNode*)node;
            FILE *output = generator->output_file;
            
            if (return_stmt->value) {
                // Generate code for the return value
                if (!generate_node(generator, return_stmt->value)) {
                    return 0; // Error in return value
                }
                
                // Get the result
                char value_reg[32];
                generate_temp(generator, value_reg, sizeof(value_reg));
                
                // Generate the return instruction
                fprintf(output, "  ret i32 %%%s\n", value_reg);
            } else {
                // Generate void return
                fprintf(output, "  ret void\n");
            }
            
            return 1;
        }

        case NODE_IDENTIFIER: {
            IdentifierNode *identifier = (IdentifierNode*)node;
            FILE *output = generator->output_file;
            
            // Load the value of the identifier
            char temp[32];
            generate_temp(generator, temp, sizeof(temp));
            fprintf(output, "  %%%s = load i32, i32* %%%s\n", temp, identifier->name);
            
            return 1;
        }

        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode *number = (NumberLiteralNode*)node;
            FILE *output = generator->output_file;
            
            // Load the number into a temporary register
            char temp[32];
            generate_temp(generator, temp, sizeof(temp));
            fprintf(output, "  %%%s = add i32 %s, 0\n", temp, number->value);
            
            return 1;
        }

        case NODE_BINARY_EXPR: {
            BinaryExprNode *binary = (BinaryExprNode*)node;
            FILE *output = generator->output_file;
            
            // Generate code for the left operand
            if (!generate_node(generator, binary->left)) {
                return 0; // Error in left operand
            }
            char left_reg[32];
            generate_temp(generator, left_reg, sizeof(left_reg));
            
            // Generate code for the right operand
            if (!generate_node(generator, binary->right)) {
                return 0; // Error in right operand
            }
            char right_reg[32];
            generate_temp(generator, right_reg, sizeof(right_reg));
            
            // Generate code for the binary operation
            char result_reg[32];
            generate_temp(generator, result_reg, sizeof(result_reg));
            
            if (strcmp(binary->operator, "+") == 0) {
                fprintf(output, "  %%%s = add i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "-") == 0) {
                fprintf(output, "  %%%s = sub i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "*") == 0) {
                fprintf(output, "  %%%s = mul i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "/") == 0) {
                fprintf(output, "  %%%s = sdiv i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "==") == 0) {
                fprintf(output, "  %%%s = icmp eq i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "!=") == 0) {
                fprintf(output, "  %%%s = icmp ne i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "<") == 0) {
                fprintf(output, "  %%%s = icmp slt i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, "<=") == 0) {
                fprintf(output, "  %%%s = icmp sle i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, ">") == 0) {
                fprintf(output, "  %%%s = icmp sgt i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else if (strcmp(binary->operator, ">=") == 0) {
                fprintf(output, "  %%%s = icmp sge i32 %%%s, %%%s\n", result_reg, left_reg, right_reg);
            } else {
                set_error(generator, "Unsupported binary operator");
                return 0;
            }
            
            return 1;
        }

        case NODE_CALL_EXPR: {
            CallExprNode *call = (CallExprNode*)node;
            FILE *output = generator->output_file;
            
            // Generate code for the arguments
            char arg_regs[16][32]; // Assume max 16 arguments
            for (int i = 0; i < call->argument_count && i < 16; i++) {
                if (!generate_node(generator, call->arguments[i])) {
                    return 0; // Error in argument
                }
                generate_temp(generator, arg_regs[i], sizeof(arg_regs[i]));
            }
            
            // Generate the call instruction
            char result_reg[32];
            generate_temp(generator, result_reg, sizeof(result_reg));
            
            fprintf(output, "  %%%s = call i32 @%s(", result_reg, call->callee);
            for (int i = 0; i < call->argument_count && i < 16; i++) {
                if (i > 0) fprintf(output, ", ");
                fprintf(output, "i32 %%%s", arg_regs[i]);
            }
            fprintf(output, ")\n");
            
            return 1;
        }

        default:
            set_error(generator, "Unsupported node type for code generation");
            return 0;
    }
}

void generate_temp(CodeGenerator *generator, char *buf, size_t size) {
    snprintf(buf, size, "t%d", generator->temp_counter++);
}

void generate_label(CodeGenerator *generator, char *buf, size_t size) {
    snprintf(buf, size, "label%d", generator->label_counter++);
}

static void set_error(CodeGenerator *generator, const char *message) {
    generator->has_error = 1;
    snprintf(generator->error_message, sizeof(generator->error_message), 
             "Code generation error: %s", message);
    fprintf(stderr, "%s\n", generator->error_message);
} 