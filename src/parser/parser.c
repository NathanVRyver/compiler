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

// Helper function to compare tokens based on their raw bytes
static int token_equals(const char *token_val, const char *expected) {
    // Compare the token value against the expected string byte by byte
    int i;
    for (i = 0; token_val[i] != '\0' && expected[i] != '\0'; i++) {
        if ((unsigned char)token_val[i] != (unsigned char)expected[i]) {
            return 0;  // Mismatch found
        }
    }
    
    // Both strings should end at the same position
    return token_val[i] == '\0' && expected[i] == '\0';
}

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
    if (check(parser, TOKEN_KEYWORD)) {
        // Save current position
        Token current_token = parser->current_token;
        
        // Try to parse as a function declaration
        ASTNode* function = parse_function_declaration(parser);
        if (function) {
            return function;
        }
        
        // If not a function, restore position and try as a variable declaration
        parser->current_token = current_token;
        ASTNode* variable = parse_variable_declaration(parser);
        if (variable) {
            return variable;
        }
        
        // If neither worked, restore position and try as a statement
        parser->current_token = current_token;
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
    if (parser->has_error || !token_equals(parser->previous_token.value, "{")) {
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
    while (!check(parser, TOKEN_PUNCTUATOR) || !token_equals(parser->current_token.value, "}")) {
        ASTNode *statement = NULL;
        
        // First check if it's a declaration or a statement
        if (check(parser, TOKEN_KEYWORD) && 
            (token_equals(parser->current_token.value, "int") || 
             token_equals(parser->current_token.value, "char") || 
             token_equals(parser->current_token.value, "void"))) {
            statement = parse_declaration(parser);
        } else {
            statement = parse_statement(parser);
        }
        
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
    if (parser->has_error || !token_equals(parser->previous_token.value, "}")) {
        set_error(parser, "Expected '}' at end of block.");
        free_ast_node((ASTNode*)compound);
        return NULL;
    }
    
    return (ASTNode*)compound;
}

static ASTNode* parse_expression(Parser *parser) {
    return parse_assignment(parser);
}

static ASTNode* parse_assignment(Parser *parser) {
    ASTNode* expr = parse_equality(parser);
    
    if (expr && check(parser, TOKEN_OPERATOR) && 
        (token_equals(parser->current_token.value, "="))) {
        
        advance(parser);  // Consume '='
        
        ASTNode* value = parse_assignment(parser);
        
        if (value) {
            if (expr->type != NODE_IDENTIFIER) {
                set_error(parser, "Invalid assignment target");
                free_ast_node(value);
                return expr;
            }
            
            AssignmentExprNode* assign = (AssignmentExprNode*)create_ast_node(
                NODE_ASSIGNMENT_EXPR, sizeof(AssignmentExprNode));
            
            if (assign) {
                assign->target = expr;
                assign->value = value;
                expr = (ASTNode*)assign;
            } else {
                free_ast_node(value);
            }
        }
    }
    
    return expr;
}

static ASTNode* parse_equality(Parser *parser) {
    ASTNode* expr = parse_comparison(parser);
    
    while (expr && check(parser, TOKEN_OPERATOR) && 
           (token_equals(parser->current_token.value, "==") || 
            token_equals(parser->current_token.value, "!="))) {
        
        char op[MAX_TOKEN_LEN];
        strcpy(op, parser->current_token.value);
        advance(parser);
        
        ASTNode* right = parse_comparison(parser);
        if (right) {
            BinaryExprNode* binary = (BinaryExprNode*)create_ast_node(
                NODE_BINARY_EXPR, sizeof(BinaryExprNode));
            
            if (binary) {
                strcpy(binary->operator, op);
                binary->left = expr;
                binary->right = right;
                expr = (ASTNode*)binary;
            } else {
                free_ast_node(right);
            }
        }
    }
    
    return expr;
}

static ASTNode* parse_comparison(Parser *parser) {
    ASTNode* expr = parse_term(parser);
    
    while (expr && check(parser, TOKEN_OPERATOR) && 
           (token_equals(parser->current_token.value, "<") || 
            token_equals(parser->current_token.value, "<=") ||
            token_equals(parser->current_token.value, ">") ||
            token_equals(parser->current_token.value, ">="))) {
        
        char op[MAX_TOKEN_LEN];
        strcpy(op, parser->current_token.value);
        advance(parser);  // Consume the operator
        
        ASTNode* right = parse_term(parser);
        if (right) {
            BinaryExprNode* binary = (BinaryExprNode*)create_ast_node(
                NODE_BINARY_EXPR, sizeof(BinaryExprNode));
            
            if (binary) {
                strcpy(binary->operator, op);
                binary->left = expr;
                binary->right = right;
                expr = (ASTNode*)binary;
            } else {
                free_ast_node(right);
            }
        }
    }
    
    return expr;
}

static ASTNode* parse_term(Parser *parser) {
    ASTNode* expr = parse_factor(parser);
    
    while (expr && check(parser, TOKEN_OPERATOR) && 
           (strcmp(parser->current_token.value, "+") == 0 || 
            strcmp(parser->current_token.value, "-") == 0)) {
        
        char op[MAX_TOKEN_LEN];
        strcpy(op, parser->current_token.value);
        advance(parser);
        
        ASTNode* right = parse_factor(parser);
        if (right) {
            BinaryExprNode* binary = (BinaryExprNode*)create_ast_node(
                NODE_BINARY_EXPR, sizeof(BinaryExprNode));
            
            if (binary) {
                strcpy(binary->operator, op);
                binary->left = expr;
                binary->right = right;
                expr = (ASTNode*)binary;
            }
        }
    }
    
    return expr;
}

static ASTNode* parse_factor(Parser *parser) {
    ASTNode* expr = parse_unary(parser);
    
    while (expr && check(parser, TOKEN_OPERATOR) && 
           (strcmp(parser->current_token.value, "*") == 0 || 
            strcmp(parser->current_token.value, "/") == 0)) {
        
        char op[MAX_TOKEN_LEN];
        strcpy(op, parser->current_token.value);
        advance(parser);
        
        ASTNode* right = parse_unary(parser);
        if (right) {
            BinaryExprNode* binary = (BinaryExprNode*)create_ast_node(
                NODE_BINARY_EXPR, sizeof(BinaryExprNode));
            
            if (binary) {
                strcpy(binary->operator, op);
                binary->left = expr;
                binary->right = right;
                expr = (ASTNode*)binary;
            }
        }
    }
    
    return expr;
}

static ASTNode* parse_unary(Parser *parser) {
    if (check(parser, TOKEN_OPERATOR) &&
        (strcmp(parser->current_token.value, "!") == 0 || 
         strcmp(parser->current_token.value, "-") == 0 ||
         strcmp(parser->current_token.value, "&") == 0 ||
         strcmp(parser->current_token.value, "*") == 0)) {
        
        char op[MAX_TOKEN_LEN];
        strcpy(op, parser->current_token.value);
        advance(parser);
        
        ASTNode* operand = parse_unary(parser);
        if (operand) {
            UnaryExprNode* unary = (UnaryExprNode*)create_ast_node(
                NODE_UNARY_EXPR, sizeof(UnaryExprNode));
            
            if (unary) {
                strcpy(unary->operator, op);
                unary->operand = operand;
                return (ASTNode*)unary;
            }
        }
        
        return NULL;
    }
    
    return parse_primary(parser);
}

static ASTNode* parse_primary(Parser *parser) {
    if (match(parser, TOKEN_NUMBER)) {
        NumberLiteralNode* number = (NumberLiteralNode*)create_ast_node(
            NODE_NUMBER_LITERAL, sizeof(NumberLiteralNode));
        
        if (number) {
            strcpy(number->value, parser->previous_token.value);
            return (ASTNode*)number;
        }
    }
    
    if (match(parser, TOKEN_IDENTIFIER)) {
        char id_name[MAX_TOKEN_LEN];
        strcpy(id_name, parser->previous_token.value);
        
        if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "(") == 0) {
            advance(parser); // Consume '('
            
            CallExprNode* call = (CallExprNode*)create_ast_node(
                NODE_CALL_EXPR, sizeof(CallExprNode));
            
            if (!call) return NULL;
            
            call->callee = strdup(id_name);
            call->arguments = NULL;
            call->argument_count = 0;
            
            if (!check(parser, TOKEN_PUNCTUATOR) || strcmp(parser->current_token.value, ")") != 0) {
                int capacity = 4;
                call->arguments = malloc(capacity * sizeof(ASTNode*));
                
                do {
                    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ")") == 0) {
                        break;
                    }
                    
                    ASTNode* arg = parse_expression(parser);
                    if (!arg) {
                        for (int i = 0; i < call->argument_count; i++) {
                            free_ast_node(call->arguments[i]);
                        }
                        free(call->arguments);
                        free(call->callee);
                        free(call);
                        return NULL;
                    }
                    
                    if (call->argument_count >= capacity) {
                        capacity *= 2;
                        call->arguments = realloc(call->arguments, capacity * sizeof(ASTNode*));
                    }
                    
                    call->arguments[call->argument_count++] = arg;
                    
                    if (!check(parser, TOKEN_PUNCTUATOR) || strcmp(parser->current_token.value, ",") != 0) {
                        break;
                    }
                    
                    advance(parser); // Consume ','
                } while (1);
            }
            
            consume(parser, TOKEN_PUNCTUATOR, "Expected ')' after function arguments");
            if (parser->has_error || strcmp(parser->previous_token.value, ")") != 0) {
                set_error(parser, "Expected ')' after function arguments");
                for (int i = 0; i < call->argument_count; i++) {
                    free_ast_node(call->arguments[i]);
                }
                free(call->arguments);
                free(call->callee);
                free(call);
                return NULL;
            }
            
            return (ASTNode*)call;
        } else {
            IdentifierNode* identifier = (IdentifierNode*)create_ast_node(
                NODE_IDENTIFIER, sizeof(IdentifierNode));
            
            if (identifier) {
                strcpy(identifier->name, id_name);
                return (ASTNode*)identifier;
            }
        }
    }
    
    if (match(parser, TOKEN_PUNCTUATOR) && strcmp(parser->previous_token.value, "(") == 0) {
        ASTNode* expr = parse_expression(parser);
        
        consume(parser, TOKEN_PUNCTUATOR, "Expected ')' after expression");
        if (parser->has_error || strcmp(parser->previous_token.value, ")") != 0) {
            free_ast_node(expr);
            return NULL;
        }
        
        return expr;
    }
    
    set_error(parser, "Expected expression");
    return NULL;
}

// Stub implementations for the remaining functions - will be expanded later
static ASTNode* parse_function_declaration(Parser *parser) {
    // Parse return type
    if (!match(parser, TOKEN_KEYWORD)) {
        return NULL;  // Not a function declaration
    }
    
    char return_type[MAX_TOKEN_LEN];
    strcpy(return_type, parser->previous_token.value);
    
    // Parse function name
    if (!match(parser, TOKEN_IDENTIFIER)) {
        return NULL;  // Not a function declaration
    }
    
    char name[MAX_TOKEN_LEN];
    strcpy(name, parser->previous_token.value);
    
    // Match opening parenthesis
    if (!match(parser, TOKEN_PUNCTUATOR) || !token_equals(parser->previous_token.value, "(")) {
        return NULL;  // Not a function declaration
    }
    
    // Create function node
    FunctionDeclNode *func = (FunctionDeclNode*)create_ast_node(NODE_FUNCTION_DECL, sizeof(FunctionDeclNode));
    if (!func) return NULL;
    
    func->name = strdup(name);
    func->return_type = strdup(return_type);
    func->parameters = NULL;
    func->parameter_count = 0;
    
    // Parse parameters if any
    if (!check(parser, TOKEN_PUNCTUATOR) || !token_equals(parser->current_token.value, ")")) {
        // Allocate initial space for parameters
        int capacity = 4;
        func->parameters = malloc(capacity * sizeof(struct {char *type; char *name;}));
        if (!func->parameters) {
            goto cleanup;
        }
        
        do {
            // Reached the end of parameters
            if (check(parser, TOKEN_PUNCTUATOR) && token_equals(parser->current_token.value, ")")) {
                break;
            }
            
            // Parse parameter type
            if (!match(parser, TOKEN_KEYWORD)) {
                set_error(parser, "Expected parameter type");
                goto cleanup;
            }
            
            char param_type[MAX_TOKEN_LEN];
            strcpy(param_type, parser->previous_token.value);
            
            // Parse parameter name
            if (!match(parser, TOKEN_IDENTIFIER)) {
                set_error(parser, "Expected parameter name");
                goto cleanup;
            }
            
            char param_name[MAX_TOKEN_LEN];
            strcpy(param_name, parser->previous_token.value);
            
            // Resize parameter array if needed
            if (func->parameter_count >= capacity) {
                capacity *= 2;
                func->parameters = realloc(func->parameters, capacity * sizeof(struct {char *type; char *name;}));
                if (!func->parameters) {
                    set_error(parser, "Memory allocation failed");
                    goto cleanup;
                }
            }
            
            // Add parameter to function declaration
            func->parameters[func->parameter_count].type = strdup(param_type);
            func->parameters[func->parameter_count].name = strdup(param_name);
            func->parameter_count++;
            
            // Check for comma or end of parameters
            if (check(parser, TOKEN_PUNCTUATOR) && token_equals(parser->current_token.value, ",")) {
                advance(parser); // Consume comma
            } else if (check(parser, TOKEN_PUNCTUATOR) && token_equals(parser->current_token.value, ")")) {
                break;
            } else {
                set_error(parser, "Expected ',' or ')' after parameter");
                goto cleanup;
            }
        } while (1);
    }
    
    // Match closing parenthesis
    if (!match(parser, TOKEN_PUNCTUATOR) || !token_equals(parser->previous_token.value, ")")) {
        set_error(parser, "Expected ')' after parameters");
        goto cleanup;
    }
    
    // Check if it's a function declaration (ends with semicolon)
    if (check(parser, TOKEN_PUNCTUATOR) && token_equals(parser->current_token.value, ";")) {
        advance(parser); // Consume semicolon
        func->body = NULL;
        return (ASTNode*)func;
    }
    
    // Parse function body
    if (check(parser, TOKEN_PUNCTUATOR) && token_equals(parser->current_token.value, "{")) {
        func->body = parse_compound_statement(parser);
        if (!func->body) {
            goto cleanup;
        }
        return (ASTNode*)func;
    } else {
        set_error(parser, "Expected '{' for function body or ';' for declaration");
        goto cleanup;
    }
    
cleanup:
    free(func->name);
    free(func->return_type);
    for (int i = 0; i < func->parameter_count; i++) {
        free(func->parameters[i].type);
        free(func->parameters[i].name);
    }
    free(func->parameters);
    free(func);
    return NULL;
}

static ASTNode* parse_variable_declaration(Parser *parser) {
    // Match the variable type
    if (!match(parser, TOKEN_KEYWORD)) {
        set_error(parser, "Expected variable type");
        return NULL;
    }
    
    char type[MAX_TOKEN_LEN];
    strcpy(type, parser->previous_token.value);
    
    // Match the variable name
    if (!match(parser, TOKEN_IDENTIFIER)) {
        set_error(parser, "Expected variable name");
        return NULL;
    }
    
    char name[MAX_TOKEN_LEN];
    strcpy(name, parser->previous_token.value);
    
    // Create variable node
    VariableDeclNode *var = (VariableDeclNode*)create_ast_node(NODE_VARIABLE_DECL, sizeof(VariableDeclNode));
    if (!var) return NULL;
    
    var->type = strdup(type);
    var->name = strdup(name);
    var->initializer = NULL;
    
    // Check for initialization
    if (check(parser, TOKEN_OPERATOR) && strcmp(parser->current_token.value, "=") == 0) {
        advance(parser); // Consume '='
        
        // Parse initializer expression
        var->initializer = parse_expression(parser);
        if (!var->initializer) {
            free(var->type);
            free(var->name);
            free(var);
            return NULL;
        }
    }
    
    // Match semicolon
    if (!match(parser, TOKEN_PUNCTUATOR) || strcmp(parser->previous_token.value, ";") != 0) {
        set_error(parser, "Expected ';' after variable declaration");
        free(var->type);
        free(var->name);
        free_ast_node(var->initializer);
        free(var);
        return NULL;
    }
    
    return (ASTNode*)var;
}

static ASTNode* parse_statement(Parser *parser) {
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, "{") == 0) {
        return parse_compound_statement(parser);
    } else if (check(parser, TOKEN_KEYWORD)) {
        if (strcmp(parser->current_token.value, "if") == 0) {
            return parse_if_statement(parser);
        } else if (strcmp(parser->current_token.value, "while") == 0) {
            return parse_while_statement(parser);
        } else if (strcmp(parser->current_token.value, "for") == 0) {
            return parse_for_statement(parser);
        } else if (strcmp(parser->current_token.value, "return") == 0) {
            return parse_return_statement(parser);
        } else if (strcmp(parser->current_token.value, "int") == 0 ||
                   strcmp(parser->current_token.value, "char") == 0 ||
                   strcmp(parser->current_token.value, "void") == 0) {
            return parse_declaration(parser);
        }
    }
    
    return parse_expression_statement(parser);
}

static ASTNode* parse_expression_statement(Parser *parser) {
    // Create expression statement node
    ExprStmtNode *stmt = (ExprStmtNode*)create_ast_node(NODE_EXPRESSION_STMT, sizeof(ExprStmtNode));
    if (!stmt) return NULL;
    
    // Check for empty statement (just a semicolon)
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
        advance(parser); // Consume ';'
        stmt->expression = NULL;
        return (ASTNode*)stmt;
    }
    
    // Parse expression
    stmt->expression = parse_expression(parser);
    if (!stmt->expression) {
        free(stmt);
        return NULL;
    }
    
    // Match semicolon
    if (!match(parser, TOKEN_PUNCTUATOR) || strcmp(parser->previous_token.value, ";") != 0) {
        set_error(parser, "Expected ';' after expression");
        free_ast_node(stmt->expression);
        free(stmt);
        return NULL;
    }
    
    return (ASTNode*)stmt;
}

static ASTNode* parse_if_statement(Parser *parser) {
    // Match 'if' keyword
    if (!match(parser, TOKEN_KEYWORD) || strcmp(parser->previous_token.value, "if") != 0) {
        set_error(parser, "Expected 'if' keyword");
        return NULL;
    }
    
    // Match opening parenthesis
    if (!match(parser, TOKEN_PUNCTUATOR) || strcmp(parser->previous_token.value, "(") != 0) {
        set_error(parser, "Expected '(' after 'if'");
        return NULL;
    }
    
    // Create if statement node
    IfStmtNode *if_stmt = (IfStmtNode*)create_ast_node(NODE_IF_STMT, sizeof(IfStmtNode));
    if (!if_stmt) return NULL;
    
    // Parse condition
    if_stmt->condition = parse_expression(parser);
    if (!if_stmt->condition) {
        free(if_stmt);
        return NULL;
    }
    
    // Match closing parenthesis
    if (!match(parser, TOKEN_PUNCTUATOR) || strcmp(parser->previous_token.value, ")") != 0) {
        set_error(parser, "Expected ')' after if condition");
        free_ast_node(if_stmt->condition);
        free(if_stmt);
        return NULL;
    }
    
    // Parse 'then' branch
    if_stmt->then_branch = parse_statement(parser);
    if (!if_stmt->then_branch) {
        free_ast_node(if_stmt->condition);
        free(if_stmt);
        return NULL;
    }
    
    // Initialize else branch to NULL
    if_stmt->else_branch = NULL;
    
    // Check for 'else' branch
    if (match(parser, TOKEN_KEYWORD) && strcmp(parser->previous_token.value, "else") == 0) {
        if_stmt->else_branch = parse_statement(parser);
        if (!if_stmt->else_branch) {
            free_ast_node(if_stmt->condition);
            free_ast_node(if_stmt->then_branch);
            free(if_stmt);
            return NULL;
        }
    }
    
    return (ASTNode*)if_stmt;
}

static ASTNode* parse_while_statement(Parser *parser) {
    advance(parser); // Consume 'while'
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected '(' after 'while'");
    if (parser->has_error || strcmp(parser->previous_token.value, "(") != 0) {
        return NULL;
    }
    
    WhileStmtNode* while_stmt = (WhileStmtNode*)create_ast_node(NODE_WHILE_STMT, sizeof(WhileStmtNode));
    if (!while_stmt) return NULL;
    
    while_stmt->condition = parse_expression(parser);
    if (!while_stmt->condition) {
        free(while_stmt);
        return NULL;
    }
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected ')' after while condition");
    if (parser->has_error || strcmp(parser->previous_token.value, ")") != 0) {
        free_ast_node(while_stmt->condition);
        free(while_stmt);
        return NULL;
    }
    
    while_stmt->body = parse_statement(parser);
    if (!while_stmt->body) {
        free_ast_node(while_stmt->condition);
        free(while_stmt);
        return NULL;
    }
    
    return (ASTNode*)while_stmt;
}

static ASTNode* parse_for_statement(Parser *parser) {
    advance(parser); // Consume 'for'
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected '(' after 'for'");
    if (parser->has_error || strcmp(parser->previous_token.value, "(") != 0) {
        return NULL;
    }
    
    ForStmtNode* for_stmt = (ForStmtNode*)create_ast_node(NODE_FOR_STMT, sizeof(ForStmtNode));
    if (!for_stmt) return NULL;
    
    // Initialize all fields to NULL
    for_stmt->initializer = NULL;
    for_stmt->condition = NULL;
    for_stmt->increment = NULL;
    for_stmt->body = NULL;
    
    // Parse initializer
    if (!(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0)) {
        if (check(parser, TOKEN_KEYWORD) && 
            (strcmp(parser->current_token.value, "int") == 0 ||
             strcmp(parser->current_token.value, "char") == 0)) {
            for_stmt->initializer = parse_variable_declaration(parser);
        } else {
            ExprStmtNode* init_expr = (ExprStmtNode*)create_ast_node(NODE_EXPRESSION_STMT, sizeof(ExprStmtNode));
            if (init_expr) {
                init_expr->expression = parse_expression(parser);
                for_stmt->initializer = (ASTNode*)init_expr;
                
                consume(parser, TOKEN_PUNCTUATOR, "Expected ';' after for initializer");
                if (parser->has_error || strcmp(parser->previous_token.value, ";") != 0) {
                    free_ast_node(for_stmt->initializer);
                    free(for_stmt);
                    return NULL;
                }
            }
        }
        
        if (!for_stmt->initializer) {
            free(for_stmt);
            return NULL;
        }
    } else {
        advance(parser); // Consume ';'
    }
    
    // Parse condition
    if (!(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0)) {
        for_stmt->condition = parse_expression(parser);
        if (!for_stmt->condition) {
            free_ast_node(for_stmt->initializer);
            free(for_stmt);
            return NULL;
        }
    }
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected ';' after for condition");
    if (parser->has_error || strcmp(parser->previous_token.value, ";") != 0) {
        free_ast_node(for_stmt->initializer);
        free_ast_node(for_stmt->condition);
        free(for_stmt);
        return NULL;
    }
    
    // Parse increment
    if (!(check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ")") == 0)) {
        ExprStmtNode* incr_expr = (ExprStmtNode*)create_ast_node(NODE_EXPRESSION_STMT, sizeof(ExprStmtNode));
        if (incr_expr) {
            incr_expr->expression = parse_expression(parser);
            for_stmt->increment = (ASTNode*)incr_expr;
            
            if (!for_stmt->increment) {
                free_ast_node(for_stmt->initializer);
                free_ast_node(for_stmt->condition);
                free(for_stmt);
                return NULL;
            }
        }
    }
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected ')' after for clauses");
    if (parser->has_error || strcmp(parser->previous_token.value, ")") != 0) {
        free_ast_node(for_stmt->initializer);
        free_ast_node(for_stmt->condition);
        free_ast_node(for_stmt->increment);
        free(for_stmt);
        return NULL;
    }
    
    for_stmt->body = parse_statement(parser);
    if (!for_stmt->body) {
        free_ast_node(for_stmt->initializer);
        free_ast_node(for_stmt->condition);
        free_ast_node(for_stmt->increment);
        free(for_stmt);
        return NULL;
    }
    
    return (ASTNode*)for_stmt;
}

static ASTNode* parse_return_statement(Parser *parser) {
    advance(parser); // Consume 'return'
    
    ReturnStmtNode* return_stmt = (ReturnStmtNode*)create_ast_node(NODE_RETURN_STMT, sizeof(ReturnStmtNode));
    if (!return_stmt) return NULL;
    
    if (check(parser, TOKEN_PUNCTUATOR) && strcmp(parser->current_token.value, ";") == 0) {
        return_stmt->value = NULL;
    } else {
        return_stmt->value = parse_expression(parser);
        if (!return_stmt->value) {
            free(return_stmt);
            return NULL;
        }
    }
    
    consume(parser, TOKEN_PUNCTUATOR, "Expected ';' after return value");
    if (parser->has_error || strcmp(parser->previous_token.value, ";") != 0) {
        free_ast_node(return_stmt->value);
        free(return_stmt);
        return NULL;
    }
    
    return (ASTNode*)return_stmt;
}