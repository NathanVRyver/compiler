#include "../../include/parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function declarations
static void advance(Parser *parser);
static int check(Parser *parser, enum TokenType type);
static int match(Parser *parser, enum TokenType type);
static void consume(Parser *parser, enum TokenType type, const char *error_message);
static void set_error(Parser *parser, const char *message);
static ASTNode* create_ast_node(ASTNodeType type, size_t size);

// Forward declarations for recursive descent parsing
static ASTNode* parse_declaration(Parser *parser);
static ASTNode* parse_function_declaration(Parser *parser);
static ASTNode* parse_variable_declaration(Parser *parser);
static ASTNode* parse_statement(Parser *parser);
static ASTNode* parse_expression_statement(Parser *parser);
static ASTNode* parse_if_statement(Parser *parser);
static ASTNode* parse_while_statement(Parser *parser);
static ASTNode* parse_for_statement(Parser *parser);
static ASTNode* parse_return_statement(Parser *parser);
static ASTNode* parse_compound_statement(Parser *parser);
static ASTNode* parse_expression(Parser *parser);
static ASTNode* parse_assignment(Parser *parser);
static ASTNode* parse_equality(Parser *parser);
static ASTNode* parse_comparison(Parser *parser);
static ASTNode* parse_term(Parser *parser);
static ASTNode* parse_factor(Parser *parser);
static ASTNode* parse_unary(Parser *parser);
static ASTNode* parse_primary(Parser *parser);

Parser* init_parser(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return NULL;
    }

    Parser *parser = (Parser*)malloc(sizeof(Parser));
    if (parser == NULL) {
        fclose(file);
        return NULL;
    }

    parser->file = file;
    parser->has_error = 0;
    memset(parser->error_message, 0, sizeof(parser->error_message));
    
    // Get the first token
    advance(parser);
    
    return parser;
}

void free_parser(Parser *parser) {
    if (parser) {
        if (parser->file) {
            fclose(parser->file);
        }
        free(parser);
    }
}

ASTNode* parse_program(Parser *parser) {
    ProgramNode *program = (ProgramNode*)create_ast_node(NODE_PROGRAM, sizeof(ProgramNode));
    if (!program) return NULL;

    // Allocate initial space for declarations
    int capacity = 8;
    program->declarations = malloc(capacity * sizeof(ASTNode*));
    program->declaration_count = 0;

    // Parse declarations until the end of file
    while (!check(parser, TOKEN_EOF)) {
        ASTNode *declaration = parse_declaration(parser);
        if (declaration) {
            // Resize array if needed
            if (program->declaration_count >= capacity) {
                capacity *= 2;
                program->declarations = realloc(program->declarations, capacity * sizeof(ASTNode*));
            }
            
            // Add the declaration to the program
            program->declarations[program->declaration_count++] = declaration;
            declaration->parent = (ASTNode*)program;
        }
        
        if (parser->has_error) {
            // Skip to the next top-level declaration point
            while (!check(parser, TOKEN_EOF) && 
                  !(match(parser, TOKEN_KEYWORD) && 
                    (strcmp(parser->previous_token.value, "int") == 0 ||
                     strcmp(parser->previous_token.value, "void") == 0 ||
                     strcmp(parser->previous_token.value, "char") == 0))) {
                advance(parser);
            }
            parser->has_error = 0; // Reset error flag to continue parsing
        }
    }

    return (ASTNode*)program;
}

static ASTNode* parse_declaration(Parser *parser) {
    // Check for function or variable declaration
    if (match(parser, TOKEN_KEYWORD)) {
        // Save the type
        char type[MAX_TOKEN_LEN];
        strcpy(type, parser->previous_token.value);
        
        // Check for identifier
        if (match(parser, TOKEN_IDENTIFIER)) {
            // Save the identifier
            char identifier[MAX_TOKEN_LEN];
            strcpy(identifier, parser->previous_token.value);
            
            // Check if it's a function or variable
            if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "(") == 0) {
                // Function declaration
                // Restore token position to re-parse in parse_function_declaration
                parser->current_token = parser->previous_token;
                return parse_function_declaration(parser);
            } else {
                // Variable declaration
                // Restore token position to re-parse in parse_variable_declaration
                parser->current_token = parser->previous_token;
                return parse_variable_declaration(parser);
            }
        } else {
            set_error(parser, "Expected identifier after type.");
            return NULL;
        }
    }
    
    // Not a declaration, try parsing as a statement
    return parse_statement(parser);
}

static void advance(Parser *parser) {
    parser->previous_token = parser->current_token;
    parser->current_token = get_next_token(parser->file);
}

static int check(Parser *parser, enum TokenType type) {
    return parser->current_token.type == type;
}

static int match(Parser *parser, enum TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return 1;
    }
    return 0;
}

static void consume(Parser *parser, enum TokenType type, const char *error_message) {
    if (check(parser, type)) {
        advance(parser);
    } else {
        set_error(parser, error_message);
    }
}

static void set_error(Parser *parser, const char *message) {
    parser->has_error = 1;
    snprintf(parser->error_message, sizeof(parser->error_message), "Error at '%s': %s", 
             parser->current_token.value, message);
    fprintf(stderr, "%s\n", parser->error_message);
}

static ASTNode* create_ast_node(ASTNodeType type, size_t size) {
    ASTNode *node = (ASTNode*)calloc(1, size);
    if (node) {
        node->type = type;
        node->parent = NULL;
    }
    return node;
}

void free_ast_node(ASTNode *node) {
    if (!node) return;

    // Free based on the node type
    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode *program = (ProgramNode*)node;
            for (int i = 0; i < program->declaration_count; i++) {
                free_ast_node(program->declarations[i]);
            }
            free(program->declarations);
            break;
        }
        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            free(func->name);
            free(func->return_type);
            for (int i = 0; i < func->parameter_count; i++) {
                free(func->parameters[i].type);
                free(func->parameters[i].name);
            }
            free(func->parameters);
            free_ast_node(func->body);
            break;
        }
        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            free(var->type);
            free(var->name);
            free_ast_node(var->initializer);
            break;
        }
        case NODE_COMPOUND_STMT: {
            CompoundStmtNode *compound = (CompoundStmtNode*)node;
            for (int i = 0; i < compound->statement_count; i++) {
                free_ast_node(compound->statements[i]);
            }
            free(compound->statements);
            break;
        }
        case NODE_CALL_EXPR: {
            CallExprNode *call = (CallExprNode*)node;
            free(call->callee);
            for (int i = 0; i < call->argument_count; i++) {
                free_ast_node(call->arguments[i]);
            }
            free(call->arguments);
            break;
        }
        case NODE_BINARY_EXPR: {
            BinaryExprNode *binary = (BinaryExprNode*)node;
            free_ast_node(binary->left);
            free_ast_node(binary->right);
            break;
        }
        case NODE_UNARY_EXPR: {
            UnaryExprNode *unary = (UnaryExprNode*)node;
            free_ast_node(unary->operand);
            break;
        }
        case NODE_ASSIGNMENT_EXPR: {
            AssignmentExprNode *assign = (AssignmentExprNode*)node;
            free_ast_node(assign->target);
            free_ast_node(assign->value);
            break;
        }
        case NODE_EXPRESSION_STMT: {
            ExprStmtNode *expr = (ExprStmtNode*)node;
            free_ast_node(expr->expression);
            break;
        }
        case NODE_IF_STMT: {
            IfStmtNode *if_stmt = (IfStmtNode*)node;
            free_ast_node(if_stmt->condition);
            free_ast_node(if_stmt->then_branch);
            free_ast_node(if_stmt->else_branch);
            break;
        }
        case NODE_WHILE_STMT: {
            WhileStmtNode *while_stmt = (WhileStmtNode*)node;
            free_ast_node(while_stmt->condition);
            free_ast_node(while_stmt->body);
            break;
        }
        case NODE_FOR_STMT: {
            ForStmtNode *for_stmt = (ForStmtNode*)node;
            free_ast_node(for_stmt->initializer);
            free_ast_node(for_stmt->condition);
            free_ast_node(for_stmt->increment);
            free_ast_node(for_stmt->body);
            break;
        }
        case NODE_RETURN_STMT: {
            ReturnStmtNode *return_stmt = (ReturnStmtNode*)node;
            free_ast_node(return_stmt->value);
            break;
        }
        // Other node types don't need special handling
        default:
            break;
    }

    free(node);
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;

    // Print indentation
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    // Print based on node type
    switch (node->type) {
        case NODE_PROGRAM:
            printf("Program\n");
            for (int i = 0; i < ((ProgramNode*)node)->declaration_count; i++) {
                print_ast(((ProgramNode*)node)->declarations[i], indent + 1);
            }
            break;
        case NODE_FUNCTION_DECL: {
            FunctionDeclNode *func = (FunctionDeclNode*)node;
            printf("Function: %s %s(", func->return_type, func->name);
            for (int i = 0; i < func->parameter_count; i++) {
                if (i > 0) printf(", ");
                printf("%s %s", func->parameters[i].type, func->parameters[i].name);
            }
            printf(")\n");
            print_ast(func->body, indent + 1);
            break;
        }
        case NODE_VARIABLE_DECL: {
            VariableDeclNode *var = (VariableDeclNode*)node;
            printf("Variable: %s %s\n", var->type, var->name);
            if (var->initializer) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Initializer:\n");
                print_ast(var->initializer, indent + 2);
            }
            break;
        }
        case NODE_COMPOUND_STMT: {
            CompoundStmtNode *compound = (CompoundStmtNode*)node;
            printf("Block:\n");
            for (int i = 0; i < compound->statement_count; i++) {
                print_ast(compound->statements[i], indent + 1);
            }
            break;
        }
        case NODE_EXPRESSION_STMT: {
            ExprStmtNode *expr = (ExprStmtNode*)node;
            printf("Expression Statement:\n");
            if (expr->expression) {
                print_ast(expr->expression, indent + 1);
            } else {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("(empty)\n");
            }
            break;
        }
        case NODE_IF_STMT: {
            IfStmtNode *if_stmt = (IfStmtNode*)node;
            printf("If Statement:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Condition:\n");
            print_ast(if_stmt->condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Then:\n");
            print_ast(if_stmt->then_branch, indent + 2);
            if (if_stmt->else_branch) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Else:\n");
                print_ast(if_stmt->else_branch, indent + 2);
            }
            break;
        }
        case NODE_WHILE_STMT: {
            WhileStmtNode *while_stmt = (WhileStmtNode*)node;
            printf("While Statement:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Condition:\n");
            print_ast(while_stmt->condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Body:\n");
            print_ast(while_stmt->body, indent + 2);
            break;
        }
        case NODE_FOR_STMT: {
            ForStmtNode *for_stmt = (ForStmtNode*)node;
            printf("For Statement:\n");
            if (for_stmt->initializer) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Initializer:\n");
                print_ast(for_stmt->initializer, indent + 2);
            }
            if (for_stmt->condition) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Condition:\n");
                print_ast(for_stmt->condition, indent + 2);
            }
            if (for_stmt->increment) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Increment:\n");
                print_ast(for_stmt->increment, indent + 2);
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Body:\n");
            print_ast(for_stmt->body, indent + 2);
            break;
        }
        case NODE_RETURN_STMT: {
            ReturnStmtNode *return_stmt = (ReturnStmtNode*)node;
            printf("Return Statement:\n");
            if (return_stmt->value) {
                print_ast(return_stmt->value, indent + 1);
            } else {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("(void)\n");
            }
            break;
        }
        case NODE_BINARY_EXPR: {
            BinaryExprNode *binary = (BinaryExprNode*)node;
            printf("Binary Expression: %s\n", binary->operator);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Left:\n");
            print_ast(binary->left, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Right:\n");
            print_ast(binary->right, indent + 2);
            break;
        }
        case NODE_UNARY_EXPR: {
            UnaryExprNode *unary = (UnaryExprNode*)node;
            printf("Unary Expression: %s\n", unary->operator);
            print_ast(unary->operand, indent + 1);
            break;
        }
        case NODE_CALL_EXPR: {
            CallExprNode *call = (CallExprNode*)node;
            printf("Function Call: %s\n", call->callee);
            for (int i = 0; i < call->argument_count; i++) {
                for (int j = 0; j < indent + 1; j++) printf("  ");
                printf("Argument %d:\n", i + 1);
                print_ast(call->arguments[i], indent + 2);
            }
            break;
        }
        case NODE_ASSIGNMENT_EXPR: {
            AssignmentExprNode *assign = (AssignmentExprNode*)node;
            printf("Assignment:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Target:\n");
            print_ast(assign->target, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Value:\n");
            print_ast(assign->value, indent + 2);
            break;
        }
        case NODE_IDENTIFIER: {
            IdentifierNode *id = (IdentifierNode*)node;
            printf("Identifier: %s\n", id->name);
            break;
        }
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode *num = (NumberLiteralNode*)node;
            printf("Number: %s\n", num->value);
            break;
        }
        case NODE_STRING_LITERAL: {
            StringLiteralNode *str = (StringLiteralNode*)node;
            printf("String: %s\n", str->value);
            break;
        }
        default:
            printf("Unknown node type: %d\n", node->type);
            break;
    }
}

// Basic implementation for compound statement and primary expression
static ASTNode* parse_compound_statement(Parser *parser) {
    // Parse: { statement* }
    consume(parser, TOKEN_PUNCTUATOR, "Expected '{' at start of block.");
    if (parser->has_error || strcmp(parser->previous_token.value, "{") != 0) {
        set_error(parser, "Expected '{' at start of block.");
        return NULL;
    }
    
    CompoundStmtNode *compound = (CompoundStmtNode*)create_ast_node(NODE_COMPOUND_STMT, sizeof(CompoundStmtNode));
    if (!compound) return NULL;
    
    // Allocate initial space for statements
    int capacity = 8;
    compound->statements = malloc(capacity * sizeof(ASTNode*));
    compound->statement_count = 0;
    
    // Parse statements until we reach the closing brace
    while (!check(parser, TOKEN_PUNCTUATOR) || strcmp(parser->current_token.value, "}") != 0) {
        ASTNode *statement = parse_statement(parser);
        if (statement) {
            // Resize array if needed
            if (compound->statement_count >= capacity) {
                capacity *= 2;
                compound->statements = realloc(compound->statements, capacity * sizeof(ASTNode*));
            }
            
            // Add the statement to the compound statement
            compound->statements[compound->statement_count++] = statement;
            statement->parent = (ASTNode*)compound;
        }
        
        // If we've hit the end of the file, break out
        if (check(parser, TOKEN_EOF)) {
            set_error(parser, "Unterminated block, expected '}'.");
            free_ast_node((ASTNode*)compound);
            return NULL;
        }
    }
    
    // Match closing brace
    consume(parser, TOKEN_PUNCTUATOR, "Expected '}' at end of block.");
    if (parser->has_error || strcmp(parser->previous_token.value, "}") != 0) {
        set_error(parser, "Expected '}' at end of block.");
        free_ast_node((ASTNode*)compound);
        return NULL;
    }
    
    return (ASTNode*)compound;
}

static ASTNode* parse_primary(Parser *parser) {
    // Parse: number | string | identifier | ( expression )
    if (match(parser, TOKEN_NUMBER)) {
        NumberLiteralNode *number = (NumberLiteralNode*)create_ast_node(NODE_NUMBER_LITERAL, sizeof(NumberLiteralNode));
        if (!number) return NULL;
        
        strcpy(number->value, parser->previous_token.value);
        return (ASTNode*)number;
    } else if (match(parser, TOKEN_STRING)) {
        StringLiteralNode *string = (StringLiteralNode*)create_ast_node(NODE_STRING_LITERAL, sizeof(StringLiteralNode));
        if (!string) return NULL;
        
        strcpy(string->value, parser->previous_token.value);
        return (ASTNode*)string;
    } else if (match(parser, TOKEN_IDENTIFIER)) {
        IdentifierNode *identifier = (IdentifierNode*)create_ast_node(NODE_IDENTIFIER, sizeof(IdentifierNode));
        if (!identifier) return NULL;
        
        strcpy(identifier->name, parser->previous_token.value);
        return (ASTNode*)identifier;
    } else if (match(parser, TOKEN_PUNCTUATOR) && strcmp(parser->previous_token.value, "(") == 0) {
        // Parse: ( expression )
        ASTNode *expr = parse_expression(parser);
        if (!expr) return NULL;
        
        consume(parser, TOKEN_PUNCTUATOR, "Expected ')' after expression.");
        if (parser->has_error || strcmp(parser->previous_token.value, ")") != 0) {
            set_error(parser, "Expected ')' after expression.");
            free_ast_node(expr);
            return NULL;
        }
        
        return expr;
    }
    
    set_error(parser, "Expected expression.");
    return NULL;
}

// Stub implementations for the remaining functions - will be expanded later
static ASTNode* parse_function_declaration(Parser *parser) {
    // Return a dummy function declaration with minimal valid structure
    FunctionDeclNode *func = (FunctionDeclNode*)create_ast_node(NODE_FUNCTION_DECL, sizeof(FunctionDeclNode));
    if (!func) return NULL;
    
    // Parse: type identifier(params) { body }
    // Type was already matched in parse_declaration
    char return_type[MAX_TOKEN_LEN];
    strcpy(return_type, parser->previous_token.value);
    
    // Match identifier
    if (!match(parser, TOKEN_IDENTIFIER)) {
        set_error(parser, "Expected function name.");
        free(func);
        return NULL;
    }
    
    char name[MAX_TOKEN_LEN];
    strcpy(name, parser->previous_token.value);
    
    // Set minimal valid fields
    func->name = strdup(name);
    func->return_type = strdup(return_type);
    func->parameters = NULL;
    func->parameter_count = 0;
    
    // Skip to opening brace
    while (!check(parser, TOKEN_EOF) && 
          !(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "{") == 0)) {
        advance(parser);
    }
    
    // Parse body
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "{") == 0) {
        func->body = parse_compound_statement(parser);
    } else {
        // Create an empty body
        CompoundStmtNode *body = (CompoundStmtNode*)create_ast_node(NODE_COMPOUND_STMT, sizeof(CompoundStmtNode));
        if (body) {
            body->statements = NULL;
            body->statement_count = 0;
        }
        func->body = (ASTNode*)body;
    }
    
    return (ASTNode*)func;
}

static ASTNode* parse_variable_declaration(Parser *parser) {
    // Return a variable declaration with minimal valid structure
    VariableDeclNode *var = (VariableDeclNode*)create_ast_node(NODE_VARIABLE_DECL, sizeof(VariableDeclNode));
    if (!var) return NULL;
    
    // Parse: type identifier [= expression] ;
    // Type was already matched in parse_declaration
    char type[MAX_TOKEN_LEN];
    strcpy(type, parser->previous_token.value);
    
    // Match identifier
    if (!match(parser, TOKEN_IDENTIFIER)) {
        set_error(parser, "Expected variable name.");
        free(var);
        return NULL;
    }
    
    char name[MAX_TOKEN_LEN];
    strcpy(name, parser->previous_token.value);
    
    // Set minimal valid fields
    var->type = strdup(type);
    var->name = strdup(name);
    var->initializer = NULL;
    
    // Skip to semicolon
    while (!check(parser, TOKEN_EOF) && 
          !(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0)) {
        advance(parser);
    }
    
    // Skip the semicolon if found
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
        advance(parser);
    }
    
    return (ASTNode*)var;
}

static ASTNode* parse_statement(Parser *parser) {
    // Try to parse a compound statement if we see a '{'
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "{") == 0) {
        return parse_compound_statement(parser);
    } else if (check(parser, TOKEN_KEYWORD)) {
        // Check for specific keywords
        if (strcmp(parser->current_token.value, "if") == 0) {
            return parse_if_statement(parser);
        } else if (strcmp(parser->current_token.value, "while") == 0) {
            return parse_while_statement(parser);
        } else if (strcmp(parser->current_token.value, "for") == 0) {
            return parse_for_statement(parser);
        } else if (strcmp(parser->current_token.value, "return") == 0) {
            return parse_return_statement(parser);
        } else {
            // Probably a variable declaration - this should be handled by parse_declaration
            // but we'll just skip it for now
            advance(parser); // Skip the keyword
            
            // Skip to semicolon
            while (!check(parser, TOKEN_EOF) && 
                  !(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0)) {
                advance(parser);
            }
            
            if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
                advance(parser); // Skip the semicolon
            }
            
            // Return an empty compound statement for now
            CompoundStmtNode *empty = (CompoundStmtNode*)create_ast_node(NODE_COMPOUND_STMT, sizeof(CompoundStmtNode));
            if (empty) {
                empty->statements = NULL;
                empty->statement_count = 0;
            }
            return (ASTNode*)empty;
        }
    } else if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
        // Empty statement
        advance(parser); // Skip the semicolon
        
        // Return an empty compound statement
        CompoundStmtNode *empty = (CompoundStmtNode*)create_ast_node(NODE_COMPOUND_STMT, sizeof(CompoundStmtNode));
        if (empty) {
            empty->statements = NULL;
            empty->statement_count = 0;
        }
        return (ASTNode*)empty;
    } else {
        // Assume it's an expression statement
        ExprStmtNode *expr_stmt = (ExprStmtNode*)create_ast_node(NODE_EXPRESSION_STMT, sizeof(ExprStmtNode));
        if (!expr_stmt) return NULL;
        
        // Parse the expression (or just skip to semicolon for now)
        expr_stmt->expression = NULL;
        
        // Skip to semicolon
        while (!check(parser, TOKEN_EOF) && 
              !(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0)) {
            advance(parser);
        }
        
        if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
            advance(parser); // Skip the semicolon
        }
        
        return (ASTNode*)expr_stmt;
    }
}

static ASTNode* parse_expression_statement(Parser *parser) {
    // Return a dummy expression statement for now
    return create_ast_node(NODE_EXPRESSION_STMT, sizeof(ExprStmtNode));
}

static ASTNode* parse_if_statement(Parser *parser) {
    // Return a dummy if statement for now
    return create_ast_node(NODE_IF_STMT, sizeof(IfStmtNode));
}

static ASTNode* parse_while_statement(Parser *parser) {
    // Return a dummy while statement for now
    return create_ast_node(NODE_WHILE_STMT, sizeof(WhileStmtNode));
}

static ASTNode* parse_for_statement(Parser *parser) {
    // Return a dummy for statement for now
    return create_ast_node(NODE_FOR_STMT, sizeof(ForStmtNode));
}

static ASTNode* parse_return_statement(Parser *parser) {
    // Return a dummy return statement for now
    return create_ast_node(NODE_RETURN_STMT, sizeof(ReturnStmtNode));
}

static ASTNode* parse_expression(Parser *parser) {
    // For now, create a simple number literal
    NumberLiteralNode *number = (NumberLiteralNode*)create_ast_node(NODE_NUMBER_LITERAL, sizeof(NumberLiteralNode));
    if (number) {
        strcpy(number->value, "0"); // Default to 0
    }
    return (ASTNode*)number;
}

static ASTNode* parse_assignment(Parser *parser) {
    // Return a dummy assignment expression for now
    return create_ast_node(NODE_ASSIGNMENT_EXPR, sizeof(AssignmentExprNode));
}

static ASTNode* parse_equality(Parser *parser) {
    // Return a dummy binary expression for now
    return create_ast_node(NODE_BINARY_EXPR, sizeof(BinaryExprNode));
}

static ASTNode* parse_comparison(Parser *parser) {
    // Return a dummy binary expression for now
    return create_ast_node(NODE_BINARY_EXPR, sizeof(BinaryExprNode));
}

static ASTNode* parse_term(Parser *parser) {
    // Return a dummy binary expression for now
    return create_ast_node(NODE_BINARY_EXPR, sizeof(BinaryExprNode));
}

static ASTNode* parse_factor(Parser *parser) {
    // Return a dummy binary expression for now
    return create_ast_node(NODE_BINARY_EXPR, sizeof(BinaryExprNode));
}

static ASTNode* parse_unary(Parser *parser) {
    // Return a dummy unary expression for now
    return create_ast_node(NODE_UNARY_EXPR, sizeof(UnaryExprNode));
} 