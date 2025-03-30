#include "../../include/codegen/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static int generate_node(CodeGenerator *generator, ASTNode *node, char *result_reg, size_t result_size);
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
    generator->var_count = 0;
    generator->function_count = 0;
    generator->current_function = NULL;
    generator->optimize_level = 0;
    generator->enable_inlining = 0;
    generator->enable_constant_folding = 0;
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
    char result_reg[32] = {0};
    return generate_node(generator, ast, result_reg, sizeof(result_reg));
}

void set_optimization_level(CodeGenerator *generator, int level) {
    if (!generator) return;
    
    generator->optimize_level = level;
    
    switch (level) {
        case 0:
            generator->enable_inlining = 0;
            generator->enable_constant_folding = 0;
            break;
        case 1:
            generator->enable_inlining = 0;
            generator->enable_constant_folding = 1;
            break;
        case 2:
        case 3:
            generator->enable_inlining = 1;
            generator->enable_constant_folding = 1;
            break;
    }
}

void generate_temp(CodeGenerator *generator, char *buf, size_t size) {
    snprintf(buf, size, "t%d", generator->temp_counter++);
}

void generate_label(CodeGenerator *generator, char *buf, size_t size) {
    snprintf(buf, size, "label%d", generator->label_counter++);
}

void add_local_var(CodeGenerator *generator, const char *name, TypeInfo *type, int is_alloca) {
    if (generator->var_count >= MAX_VARS) {
        set_error(generator, "Too many local variables");
        return;
    }
    
    VarInfo *var = &generator->local_vars[generator->var_count++];
    strncpy(var->name, name, MAX_TOKEN_LEN - 1);
    var->name[MAX_TOKEN_LEN - 1] = '\0';
    var->type = type;
    var->is_alloca = is_alloca;
    
    if (is_alloca) {
        strcpy(var->reg, name);
    } else {
        generate_temp(generator, var->reg, sizeof(var->reg));
    }
}

VarInfo* find_var(CodeGenerator *generator, const char *name) {
    for (int i = 0; i < generator->var_count; i++) {
        if (strcmp(generator->local_vars[i].name, name) == 0) {
            return &generator->local_vars[i];
        }
    }
    return NULL;
}

void add_function(CodeGenerator *generator, const char *name, TypeInfo *return_type) {
    if (generator->function_count >= MAX_FUNCTIONS) {
        set_error(generator, "Too many functions");
        return;
    }
    
    FunctionInfo *func = &generator->functions[generator->function_count++];
    strncpy(func->name, name, MAX_TOKEN_LEN - 1);
    func->name[MAX_TOKEN_LEN - 1] = '\0';
    func->return_type = return_type;
    func->param_count = 0;
    
    generator->current_function = func;
}

FunctionInfo* find_function(CodeGenerator *generator, const char *name) {
    for (int i = 0; i < generator->function_count; i++) {
        if (strcmp(generator->functions[i].name, name) == 0) {
            return &generator->functions[i];
        }
    }
    return NULL;
}

static const char* type_to_llvm(TypeInfo *type) {
    if (!type) return "i32";
    
    switch (type->type) {
        case TYPE_VOID: return "void";
        case TYPE_INT: return "i32";
        case TYPE_CHAR: return "i8";
        case TYPE_POINTER: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "%s*", type_to_llvm(type->base_type));
            return buf;
        }
        case TYPE_ARRAY: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "[%d x %s]", type->array_size, type_to_llvm(type->base_type));
            return buf;
        }
        case TYPE_STRUCT: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "%%struct.%s", type->name + 7); // Skip "struct " prefix
            return buf;
        }
        default: return "i32";
    }
}

static int generate_node(CodeGenerator *generator, ASTNode *node, char *result_reg, size_t result_size) {
    if (!node) return 1;

    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode *program = (ProgramNode*)node;
            for (int i = 0; i < program->declaration_count; i++) {
                if (!generate_node(generator, program->declarations[i], NULL, 0)) {
                    return 0;
                }
            }
            return 1;
        }

        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            FILE *output = generator->output_file;
            
            TypeInfo *return_type = create_basic_type(TYPE_INT);
            if (strcmp(func->return_type, "void") == 0) {
                return_type = create_basic_type(TYPE_VOID);
            } else if (strcmp(func->return_type, "char") == 0) {
                return_type = create_basic_type(TYPE_CHAR);
            }
            
            add_function(generator, func->name, return_type);
            
            fprintf(output, "define %s @%s(", type_to_llvm(return_type), func->name);

            generator->var_count = 0;
            
            for (int i = 0; i < func->parameter_count; i++) {
                if (i > 0) fprintf(output, ", ");
                
                TypeInfo *param_type = create_basic_type(TYPE_INT);
                if (strcmp(func->parameters[i].type, "char") == 0) {
                    param_type = create_basic_type(TYPE_CHAR);
                }
                
                fprintf(output, "%s %%%s", type_to_llvm(param_type), func->parameters[i].name);
                
                generator->current_function->params[i].type = param_type;
                strncpy(generator->current_function->params[i].name, func->parameters[i].name, MAX_TOKEN_LEN - 1);
                generator->current_function->params[i].name[MAX_TOKEN_LEN - 1] = '\0';
                strcpy(generator->current_function->params[i].reg, func->parameters[i].name);
                generator->current_function->param_count++;
                
                add_local_var(generator, func->parameters[i].name, param_type, 0);
            }
            
            fprintf(output, ") {\n");
            fprintf(output, "entry:\n");

            if (func->body) {
                if (!generate_node(generator, func->body, NULL, 0)) {
                    return 0;
                }
            }

            if (return_type->type == TYPE_VOID) {
                fprintf(output, "  ret void\n");
            } else {
                fprintf(output, "  ret %s 0\n", type_to_llvm(return_type));
            }

            fprintf(output, "}\n\n");
            generator->current_function = NULL;
            return 1;
        }

        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            FILE *output = generator->output_file;
            
            TypeInfo *var_type = create_basic_type(TYPE_INT);
            if (strcmp(var->type, "char") == 0) {
                var_type = create_basic_type(TYPE_CHAR);
            }
            
            fprintf(output, "  %%%s = alloca %s\n", var->name, type_to_llvm(var_type));
            add_local_var(generator, var->name, var_type, 1);

            if (var->initializer) {
                char init_reg[32];
                if (!generate_node(generator, var->initializer, init_reg, sizeof(init_reg))) {
                    return 0;
                }
                
                VarInfo *var_info = find_var(generator, var->name);
                if (var_info) {
                    fprintf(output, "  store %s %%%s, %s* %%%s\n", 
                            type_to_llvm(var_type), init_reg, 
                            type_to_llvm(var_type), var->name);
                }
            } else {
                fprintf(output, "  store %s 0, %s* %%%s\n", 
                        type_to_llvm(var_type), type_to_llvm(var_type), var->name);
            }

            return 1;
        }

        case NODE_COMPOUND_STMT: {
            CompoundStmtNode *compound = (CompoundStmtNode*)node;
            
            for (int i = 0; i < compound->statement_count; i++) {
                if (!generate_node(generator, compound->statements[i], NULL, 0)) {
                    return 0;
                }
            }
            
            return 1;
        }

        case NODE_EXPRESSION_STMT: {
            ExprStmtNode *expr_stmt = (ExprStmtNode*)node;
            
            if (expr_stmt->expression) {
                char expr_reg[32];
                return generate_node(generator, expr_stmt->expression, expr_reg, sizeof(expr_reg));
            }
            
            return 1;
        }

        case NODE_IF_STMT: {
            IfStmtNode *if_stmt = (IfStmtNode*)node;
            FILE *output = generator->output_file;
            
            char cond_reg[32];
            if (!generate_node(generator, if_stmt->condition, cond_reg, sizeof(cond_reg))) {
                return 0;
            }
            
            char then_label[32], else_label[32], end_label[32];
            generate_label(generator, then_label, sizeof(then_label));
            generate_label(generator, else_label, sizeof(else_label));
            generate_label(generator, end_label, sizeof(end_label));
            
            fprintf(output, "  br i1 %%%s, label %%%s, label %%%s\n", 
                    cond_reg, then_label, 
                    if_stmt->else_branch ? else_label : end_label);
            
            fprintf(output, "\n%s:\n", then_label);
            if (!generate_node(generator, if_stmt->then_branch, NULL, 0)) {
                return 0;
            }
            fprintf(output, "  br label %%%s\n", end_label);
            
            if (if_stmt->else_branch) {
                fprintf(output, "\n%s:\n", else_label);
                if (!generate_node(generator, if_stmt->else_branch, NULL, 0)) {
                    return 0;
                }
                fprintf(output, "  br label %%%s\n", end_label);
            }
            
            fprintf(output, "\n%s:\n", end_label);
            
            return 1;
        }

        case NODE_WHILE_STMT: {
            WhileStmtNode *while_stmt = (WhileStmtNode*)node;
            FILE *output = generator->output_file;
            
            char cond_label[32], body_label[32], end_label[32];
            generate_label(generator, cond_label, sizeof(cond_label));
            generate_label(generator, body_label, sizeof(body_label));
            generate_label(generator, end_label, sizeof(end_label));
            
            fprintf(output, "  br label %%%s\n", cond_label);
            
            fprintf(output, "\n%s:\n", cond_label);
            char cond_reg[32];
            if (!generate_node(generator, while_stmt->condition, cond_reg, sizeof(cond_reg))) {
                return 0;
            }
            
            fprintf(output, "  br i1 %%%s, label %%%s, label %%%s\n", 
                    cond_reg, body_label, end_label);
            
            fprintf(output, "\n%s:\n", body_label);
            if (!generate_node(generator, while_stmt->body, NULL, 0)) {
                return 0;
            }
            fprintf(output, "  br label %%%s\n", cond_label);
            
            fprintf(output, "\n%s:\n", end_label);
            
            return 1;
        }

        case NODE_FOR_STMT: {
            ForStmtNode *for_stmt = (ForStmtNode*)node;
            FILE *output = generator->output_file;
            
            // Generate initializer
            if (for_stmt->initializer && !generate_node(generator, for_stmt->initializer, NULL, 0)) {
                return 0;
            }
            
            char cond_label[32], body_label[32], incr_label[32], end_label[32];
            generate_label(generator, cond_label, sizeof(cond_label));
            generate_label(generator, body_label, sizeof(body_label));
            generate_label(generator, incr_label, sizeof(incr_label));
            generate_label(generator, end_label, sizeof(end_label));
            
            fprintf(output, "  br label %%%s\n", cond_label);
            
            fprintf(output, "\n%s:\n", cond_label);
            if (for_stmt->condition) {
                char cond_reg[32];
                if (!generate_node(generator, for_stmt->condition, cond_reg, sizeof(cond_reg))) {
                    return 0;
                }
                fprintf(output, "  br i1 %%%s, label %%%s, label %%%s\n", 
                        cond_reg, body_label, end_label);
            } else {
                fprintf(output, "  br label %%%s\n", body_label);
            }
            
            fprintf(output, "\n%s:\n", body_label);
            if (!generate_node(generator, for_stmt->body, NULL, 0)) {
                return 0;
            }
            fprintf(output, "  br label %%%s\n", incr_label);
            
            fprintf(output, "\n%s:\n", incr_label);
            if (for_stmt->increment && !generate_node(generator, for_stmt->increment, NULL, 0)) {
                return 0;
            }
            fprintf(output, "  br label %%%s\n", cond_label);
            
            fprintf(output, "\n%s:\n", end_label);
            
            return 1;
        }

        case NODE_RETURN_STMT: {
            ReturnStmtNode *return_stmt = (ReturnStmtNode*)node;
            FILE *output = generator->output_file;
            
            if (return_stmt->value) {
                char value_reg[32];
                if (!generate_node(generator, return_stmt->value, value_reg, sizeof(value_reg))) {
                    return 0;
                }
                
                TypeInfo *return_type = generator->current_function->return_type;
                fprintf(output, "  ret %s %%%s\n", type_to_llvm(return_type), value_reg);
            } else {
                fprintf(output, "  ret void\n");
            }
            
            return 1;
        }

        case NODE_BINARY_EXPR: {
            BinaryExprNode *binary = (BinaryExprNode*)node;
            FILE *output = generator->output_file;
            
            if (!result_reg || result_size == 0) {
                set_error(generator, "No result register provided for binary expression");
                return 0;
            }
            
            char left_reg[32], right_reg[32];
            if (!generate_node(generator, binary->left, left_reg, sizeof(left_reg)) ||
                !generate_node(generator, binary->right, right_reg, sizeof(right_reg))) {
                return 0;
            }
            
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
            
            // Convert boolean results to i32
            if (strncmp(binary->operator, "=", 1) == 0 || 
                strncmp(binary->operator, "!", 1) == 0 || 
                strncmp(binary->operator, "<", 1) == 0 || 
                strncmp(binary->operator, ">", 1) == 0) {
                char bool_reg[32];
                strcpy(bool_reg, result_reg);
                generate_temp(generator, result_reg, result_size);
                fprintf(output, "  %%%s = zext i1 %%%s to i32\n", result_reg, bool_reg);
            }
            
            return 1;
        }

        case NODE_UNARY_EXPR: {
            UnaryExprNode *unary = (UnaryExprNode*)node;
            FILE *output = generator->output_file;
            
            if (!result_reg || result_size == 0) {
                set_error(generator, "No result register provided for unary expression");
                return 0;
            }
            
            char operand_reg[32];
            if (!generate_node(generator, unary->operand, operand_reg, sizeof(operand_reg))) {
                return 0;
            }
            
            if (strcmp(unary->operator, "-") == 0) {
                fprintf(output, "  %%%s = sub i32 0, %%%s\n", result_reg, operand_reg);
            } else if (strcmp(unary->operator, "!") == 0) {
                char cmp_reg[32];
                generate_temp(generator, cmp_reg, sizeof(cmp_reg));
                fprintf(output, "  %%%s = icmp eq i32 %%%s, 0\n", cmp_reg, operand_reg);
                fprintf(output, "  %%%s = zext i1 %%%s to i32\n", result_reg, cmp_reg);
            } else {
                set_error(generator, "Unsupported unary operator");
                return 0;
            }
            
            return 1;
        }

        case NODE_CALL_EXPR: {
            CallExprNode *call = (CallExprNode*)node;
            FILE *output = generator->output_file;
            
            char arg_regs[16][32];
            for (int i = 0; i < call->argument_count && i < 16; i++) {
                if (!generate_node(generator, call->arguments[i], arg_regs[i], sizeof(arg_regs[i]))) {
                    return 0;
                }
            }
            
            FunctionInfo *func = find_function(generator, call->callee);
            
            TypeInfo *return_type = create_basic_type(TYPE_INT); // Default
            if (func) {
                return_type = func->return_type;
            }
            
            if (result_reg && result_size > 0 && return_type->type != TYPE_VOID) {
                fprintf(output, "  %%%s = call %s @%s(", result_reg, type_to_llvm(return_type), call->callee);
            } else {
                fprintf(output, "  call %s @%s(", type_to_llvm(return_type), call->callee);
            }
            
            for (int i = 0; i < call->argument_count && i < 16; i++) {
                if (i > 0) fprintf(output, ", ");
                fprintf(output, "i32 %%%s", arg_regs[i]);
            }
            fprintf(output, ")\n");
            
            return 1;
        }
        
        case NODE_ASSIGNMENT_EXPR: {
            AssignmentExprNode *assign = (AssignmentExprNode*)node;
            FILE *output = generator->output_file;
            
            if (assign->target->type != NODE_IDENTIFIER) {
                set_error(generator, "Assignment target must be an identifier");
                return 0;
            }
            
            IdentifierNode *id = (IdentifierNode*)assign->target;
            VarInfo *var = find_var(generator, id->name);
            if (!var) {
                set_error(generator, "Undefined variable in assignment");
                return 0;
            }
            
            char value_reg[32];
            if (!generate_node(generator, assign->value, value_reg, sizeof(value_reg))) {
                return 0;
            }
            
            fprintf(output, "  store %s %%%s, %s* %%%s\n", 
                    type_to_llvm(var->type), value_reg, 
                    type_to_llvm(var->type), id->name);
            
            if (result_reg && result_size > 0) {
                strcpy(result_reg, value_reg);
            }
            
            return 1;
        }
        
        case NODE_IDENTIFIER: {
            IdentifierNode *identifier = (IdentifierNode*)node;
            FILE *output = generator->output_file;
            
            if (!result_reg || result_size == 0) {
                set_error(generator, "No result register provided for identifier");
                return 0;
            }
            
            VarInfo *var = find_var(generator, identifier->name);
            if (!var) {
                set_error(generator, "Undefined variable");
                return 0;
            }
            
            if (var->is_alloca) {
                fprintf(output, "  %%%s = load %s, %s* %%%s\n", 
                        result_reg, type_to_llvm(var->type), 
                        type_to_llvm(var->type), var->name);
            } else {
                strcpy(result_reg, var->reg);
            }
            
            return 1;
        }
        
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode *number = (NumberLiteralNode*)node;
            FILE *output = generator->output_file;
            
            if (!result_reg || result_size == 0) {
                set_error(generator, "No result register provided for number literal");
                return 0;
            }
            
            fprintf(output, "  %%%s = add i32 %s, 0\n", result_reg, number->value);
            
            return 1;
        }
        
        case NODE_STRING_LITERAL: {
            StringLiteralNode *string = (StringLiteralNode*)node;
            FILE *output = generator->output_file;
            
            if (!result_reg || result_size == 0) {
                set_error(generator, "No result register provided for string literal");
                return 0;
            }
            
            static int str_counter = 0;
            char str_name[32];
            snprintf(str_name, sizeof(str_name), "str.%d", str_counter++);
            
            // Remove surrounding quotes and handle escapes
            char processed[MAX_TOKEN_LEN];
            int len = 0;
            for (int i = 1; i < strlen(string->value) - 1 && len < MAX_TOKEN_LEN - 1; i++) {
                if (string->value[i] == '\\') {
                    i++;
                    switch (string->value[i]) {
                        case 'n': processed[len++] = '\n'; break;
                        case 't': processed[len++] = '\t'; break;
                        case 'r': processed[len++] = '\r'; break;
                        case '0': processed[len++] = '\0'; break;
                        default: processed[len++] = string->value[i];
                    }
                } else {
                    processed[len++] = string->value[i];
                }
            }
            processed[len] = '\0';
            
            // Add null terminator for strings
            processed[len++] = '\0';
            
            // Define string constant
            fprintf(output, "@%s = private constant [%d x i8] c\"", str_name, len);
            for (int i = 0; i < len; i++) {
                if (processed[i] == '\n') fprintf(output, "\\0A");
                else if (processed[i] == '\t') fprintf(output, "\\09");
                else if (processed[i] == '\r') fprintf(output, "\\0D");
                else if (processed[i] == '\0') fprintf(output, "\\00");
                else if (processed[i] == '\"') fprintf(output, "\\22");
                else if (processed[i] == '\\') fprintf(output, "\\5C");
                else fprintf(output, "%c", processed[i]);
            }
            fprintf(output, "\"\n");
            
            // Get pointer to string
            fprintf(output, "  %%%s = getelementptr [%d x i8], [%d x i8]* @%s, i32 0, i32 0\n", 
                    result_reg, len, len, str_name);
            
            return 1;
        }
        
        default:
            set_error(generator, "Unsupported node type for code generation");
            return 0;
    }
}

static void set_error(CodeGenerator *generator, const char *message) {
    generator->has_error = 1;
    snprintf(generator->error_message, sizeof(generator->error_message), 
             "Code generation error: %s", message);
    fprintf(stderr, "%s\n", generator->error_message);
} 