#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif // DEBUG_PRINT_CODE

Chunk *compilingChunk;

static Chunk *currentChunk(void) { return compilingChunk; }

static void errorAt(Parser *parser, Token *token, const char *message) {
    if (parser->panicMode) {
        return;
    }

    parser->panicMode = true;
    fprintf(stderr, "[line %zu] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

static void error(Parser *parser, const char *message) {
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser *parser, const char *message) {
    errorAt(parser, &parser->current, message);
}

static void advance(Parser *parser, Scanner *scanner) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);

        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Parser *parser, Scanner *scanner, TokenType type,
                    const char *message) {
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    errorAtCurrent(parser, message);
}

static bool check(Parser *parser, TokenType type) { return parser->current.type == type; }

static bool match(Parser *parser, Scanner *scanner, TokenType type) {
    if (!check(parser, type)) {
        return false;
    }

    advance(parser, scanner);
    return true;
}

static void emitByte(Parser *parser, uint8_t byte) {
    writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void emitReturn(Parser *parser) { emitByte(parser, OP_RETURN); }

static uint8_t makeConstant(Parser *parser, Value value) {
    uint8_t constant = addConstant(currentChunk(), value);

    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return constant;
}

static void emitConstant(Parser *parser, Value value) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void endCompiler(Parser *parser) {
    emitReturn(parser);

#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif // DEBUG_PRINT_CODE
}

// Forward declarations
static void expression(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static void declaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static void statement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, Precedence precedence);

static uint8_t identifierConstant(Parser *parser, Token *name, VM *vm) {
    return makeConstant(parser, OBJ_VAL(copyString(name->length, name->start, vm)));
}

static void binary(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence(parser, scanner, vm, compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitBytes(parser, OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(parser, OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(parser, OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(parser, OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(parser, OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(parser, OP_GREATER, OP_NOT);
            break;
        case TOKEN_PLUS:
            emitByte(parser, OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(parser, OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(parser, OP_DIVIDE);
            break;
        default:
            return; // Unreachable
    }
}

static void literal(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emitByte(parser, OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(parser, OP_NIL);
            break;
        case TOKEN_TRUE:
            emitByte(parser, OP_TRUE);
            break;
        default:
            return; // Unreachable
    }
}

static void grouping(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value));
}

static void string(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    emitConstant(parser, OBJ_VAL(copyString(parser->previous.length - 2,
                                            parser->previous.start + 1, vm)));
}

static void namedVariable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign, Token name) {
    uint8_t arg = identifierConstant(parser, &name, vm);

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler);
        emitBytes(parser, OP_SET_GLOBAL, arg);
    } else {
        emitBytes(parser, OP_GET_GLOBAL, arg);
    }
}

static void variable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    namedVariable(parser, scanner, vm, compiler, canAssign, parser->previous);
}

static void unary(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    // Compile operand.
    parsePrecedence(parser, scanner, vm, compiler, PREC_UNARY);

    // Emit operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(parser, OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_NEGATE);
            break;
        default:
            return; // Unreachable
    }
}

// clang-format off

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

// clang-format on

static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler, Precedence precedence) {
    advance(parser, scanner);

    ParseFn prefixRule = getRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;

    prefixRule(parser, scanner, vm, compiler, canAssign);

    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser, scanner);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, scanner, vm, compiler, canAssign);
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void expression(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    parsePrecedence(parser, scanner, vm, compiler, PREC_ASSIGNMENT);
}

static void block(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    while (!check(parser, TOKEN_LEFT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(parser, scanner, vm, compiler);
    }

    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static uint8_t parseVariable(Parser *parser, Scanner *scanner, VM *vm,
                             const char *errorMsg) {
    consume(parser, scanner, TOKEN_IDENTIFIER, errorMsg);
    return identifierConstant(parser, &parser->previous, vm);
}

static void defineVariable(Parser *parser, uint8_t global) {
    emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

static void varDeclaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    uint8_t global = parseVariable(parser, scanner, vm, "Expect variable name.");

    if (match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler);
    } else {
        emitByte(parser, OP_NIL);
    }

    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(parser, global);
}

static void expressionStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(parser, OP_POP);
}

static void printStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(parser, OP_PRINT);
}

static void synchronize(Parser *parser, Scanner *scanner) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) {
            return;
        }

        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:; // Do nothing
        }
    }

    advance(parser, scanner);
}

static void declaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(parser, scanner, vm, compiler);
    } else {
        statement(parser, scanner, vm, compiler);
    }

    if (parser->panicMode) {
        synchronize(parser, scanner);
    }
}

static void statement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    if (match(parser, scanner, TOKEN_PRINT)) {
        printStatement(parser, scanner, vm, compiler);
    } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement(parser, scanner, vm, compiler);
    }
}

void initCompiler(Compiler *compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
}

bool compile(Scanner *scanner, const char *source, Chunk *chunk, VM *vm) {
    initScanner(scanner, source);

    Compiler compiler;
    initCompiler(&compiler);

    compilingChunk = chunk;

    Parser parser;
    parser.hadError = false;
    parser.panicMode = false;

    advance(&parser, scanner);

    while (!match(&parser, scanner, TOKEN_EOF)) {
        declaration(&parser, scanner, vm, &compiler);
    }

    endCompiler(&parser);
    return !parser.hadError;
}
