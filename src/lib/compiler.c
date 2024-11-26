#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif // DEBUG_PRINT_CODE

Chunk *compilingChunk;

static Chunk *currentChunk(Compiler *compiler) { return &compiler->func->chunk; }

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

static void emitByte(Parser *parser, uint8_t byte, Compiler *compiler) {
    writeChunk(currentChunk(compiler), byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2, Compiler *compiler) {
    emitByte(parser, byte1, compiler);
    emitByte(parser, byte2, compiler);
}

static size_t emitJump(Parser *parser, uint8_t instruction, Compiler *compiler) {
    emitByte(parser, instruction, compiler);
    emitByte(parser, 0xff, compiler);
    emitByte(parser, 0xff, compiler);

    return currentChunk(compiler)->count - 2;
}

static void emitLoop(Parser *parser, size_t loopStart, Compiler *compiler) {
    emitByte(parser, OP_LOOP, compiler);

    size_t offset = currentChunk(compiler)->count - loopStart + 2;

    if (offset > UINT16_MAX) {
        error(parser, "Loop body too large.");
    }

    emitByte(parser, (offset >> 8) & 0xff, compiler);
    emitByte(parser, offset & 0xff, compiler);
}

static void emitReturn(Parser *parser, Compiler *compiler) {
    emitByte(parser, OP_RETURN, compiler);
}

static uint8_t makeConstant(Parser *parser, Value value, Compiler *compiler) {
    uint8_t constant = addConstant(currentChunk(compiler), value);

    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return constant;
}

static void emitConstant(Parser *parser, Value value, Compiler *compiler) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value, compiler), compiler);
}

static void patchJump(Parser *parser, size_t offset, Compiler *compiler) {
    size_t jump = currentChunk(compiler)->count - offset - 2;

    if (jump > UINT16_MAX) {
        error(parser, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static ObjFunction *endCompiler(Parser *parser, Compiler *compiler) {
    emitReturn(parser, compiler);

    ObjFunction *func = compiler->func;

#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(currentChunk(compiler),
                         func->name != NULL ? func->name->chars : "<script>");
    }
#endif // DEBUG_PRINT_CODE

    return func;
}

static void beginScope(Compiler *compiler) { compiler->scopeDepth += 1; }

static void endScope(Parser *parser, Compiler *compiler) {
    compiler->scopeDepth -= 1;

    while (compiler->localCount > 0 &&
           compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        //! Could create OP_POPN opcode to pop multiple values off the stack at once.
        emitByte(parser, OP_POP, compiler);
        compiler->localCount -= 1;
    }
}

// Forward declarations
static void expression(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static void declaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static void statement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            Precedence precedence);

static uint8_t identifierConstant(Parser *parser, VM *vm, Token *name,
                                  Compiler *compiler) {
    return makeConstant(parser, OBJ_VAL(copyString(name->length, name->start, vm)),
                        compiler);
}

static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) {
        return false;
    }

    return memcmp(a->start, b->start, a->length) == 0;
}

static intmax_t resolveLocal(Parser *parser, Compiler *compiler, Token *name) {
    for (intmax_t i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error(parser, "Can't read local variable in its own initializer.");
            }

            return i;
        }
    }

    return -1;
}

static void addLocal(Parser *parser, Compiler *compiler, Token name) {
    if (compiler->localCount == UINT8_COUNT) {
        error(parser, "Too many local variables in function.");
        return;
    }

    Local *local = &compiler->locals[compiler->localCount++];
    local->name = name;
    local->depth = -1;
}

static void declareVariable(Parser *parser, Compiler *compiler) {
    if (compiler->scopeDepth == 0) {
        return;
    }

    Token *name = &parser->previous;

    for (intmax_t i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];

        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(parser, "Already a variable with this name in this scope.");
        }
    }

    addLocal(parser, compiler, *name);
}

static void binary(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   bool canAssign) {
    TokenType operatorType = parser->previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence(parser, scanner, vm, compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitBytes(parser, OP_EQUAL, OP_NOT, compiler);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(parser, OP_EQUAL, compiler);
            break;
        case TOKEN_GREATER:
            emitByte(parser, OP_GREATER, compiler);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(parser, OP_LESS, OP_NOT, compiler);
            break;
        case TOKEN_LESS:
            emitByte(parser, OP_LESS, compiler);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(parser, OP_GREATER, OP_NOT, compiler);
            break;
        case TOKEN_PLUS:
            emitByte(parser, OP_ADD, compiler);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_SUBTRACT, compiler);
            break;
        case TOKEN_STAR:
            emitByte(parser, OP_MULTIPLY, compiler);
            break;
        case TOKEN_SLASH:
            emitByte(parser, OP_DIVIDE, compiler);
            break;
        default:
            return; // Unreachable
    }
}

static void literal(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                    bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emitByte(parser, OP_FALSE, compiler);
            break;
        case TOKEN_NIL:
            emitByte(parser, OP_NIL, compiler);
            break;
        case TOKEN_TRUE:
            emitByte(parser, OP_TRUE, compiler);
            break;
        default:
            return; // Unreachable
    }
}

static void and_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                 bool canAssign) {
    size_t endJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler);

    emitByte(parser, OP_POP, compiler);
    parsePrecedence(parser, scanner, vm, compiler, PREC_AND);

    patchJump(parser, endJmp, compiler);
}

static void or_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                bool canAssign) {
    size_t elseJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler);
    size_t endJmp = emitJump(parser, OP_JUMP, compiler);

    patchJump(parser, elseJmp, compiler);
    emitByte(parser, OP_POP, compiler);

    parsePrecedence(parser, scanner, vm, compiler, PREC_OR);
    patchJump(parser, endJmp, compiler);
}

static void grouping(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                     bool canAssign) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   bool canAssign) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value), compiler);
}

static void string(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   bool canAssign) {
    emitConstant(
        parser,
        OBJ_VAL(copyString(parser->previous.length - 2, parser->previous.start + 1, vm)),
        compiler);
}

static void namedVariable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                          bool canAssign, Token name) {
    uint8_t getOp;
    uint8_t setOp;
    intmax_t arg = resolveLocal(parser, compiler, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(parser, vm, &name, compiler);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler);
        emitBytes(parser, setOp, (uint8_t)arg, compiler);
    } else {
        emitBytes(parser, getOp, (uint8_t)arg, compiler);
    }
}

static void variable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                     bool canAssign) {
    namedVariable(parser, scanner, vm, compiler, canAssign, parser->previous);
}

static void unary(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                  bool canAssign) {
    TokenType operatorType = parser->previous.type;

    // Compile operand.
    parsePrecedence(parser, scanner, vm, compiler, PREC_UNARY);

    // Emit operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(parser, OP_NOT, compiler);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_NEGATE, compiler);
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
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
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

static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            Precedence precedence) {
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
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(parser, scanner, vm, compiler);
    }

    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static uint8_t parseVariable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                             const char *errorMsg) {
    consume(parser, scanner, TOKEN_IDENTIFIER, errorMsg);

    declareVariable(parser, compiler);

    if (compiler->scopeDepth > 0) {
        return 0;
    }

    return identifierConstant(parser, vm, &parser->previous, compiler);
}

static void markInitialized(Compiler *compiler) {
    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Parser *parser, Compiler *compiler, uint8_t global) {
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler);
        return;
    }

    emitBytes(parser, OP_DEFINE_GLOBAL, global, compiler);
}

static void varDeclaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    uint8_t global =
        parseVariable(parser, scanner, vm, compiler, "Expect variable name.");

    if (match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler);
    } else {
        emitByte(parser, OP_NIL, compiler);
    }

    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(parser, compiler, global);
}

static void expressionStatement(Parser *parser, Scanner *scanner, VM *vm,
                                Compiler *compiler) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(parser, OP_POP, compiler);
}

static void printStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(parser, OP_PRINT, compiler);
}

static void ifStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after 'if'.");

    size_t thenJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler);
    emitByte(parser, OP_POP, compiler);
    statement(parser, scanner, vm, compiler);

    size_t elseJmp = emitJump(parser, OP_JUMP, compiler);

    patchJump(parser, thenJmp, compiler);
    emitByte(parser, OP_POP, compiler);

    if (match(parser, scanner, TOKEN_ELSE)) {
        statement(parser, scanner, vm, compiler);
    }

    patchJump(parser, elseJmp, compiler);
}

static void forStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    beginScope(compiler);

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '('  after 'for'.");

    if (match(parser, scanner, TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(parser, scanner, vm, compiler);
    } else {
        expressionStatement(parser, scanner, vm, compiler);
    }

    size_t loopStart = currentChunk(compiler)->count;
    intmax_t exitJmp = -1;

    if (!match(parser, scanner, TOKEN_SEMICOLON)) {
        expression(parser, scanner, vm, compiler);
        consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJmp = (intmax_t)emitJump(parser, OP_JUMP_IF_FALSE, compiler);
        emitByte(parser, OP_POP, compiler);
    }

    if (!match(parser, scanner, TOKEN_RIGHT_PAREN)) {
        size_t bodyJmp = emitJump(parser, OP_JUMP, compiler);
        size_t incStart = currentChunk(compiler)->count;
        expression(parser, scanner, vm, compiler);
        emitByte(parser, OP_POP, compiler);
        consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')'  after for clauses.");

        emitLoop(parser, loopStart, compiler);
        loopStart = incStart;
        patchJump(parser, bodyJmp, compiler);
    }

    statement(parser, scanner, vm, compiler);
    emitLoop(parser, loopStart, compiler);

    if (exitJmp != -1) {
        patchJump(parser, (size_t)exitJmp, compiler);
    }

    endScope(parser, compiler);
}

static void whileStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler) {
    size_t loopStart = currentChunk(compiler)->count;
    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(parser, scanner, vm, compiler);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after 'while' condition.");

    size_t exitJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler);
    emitByte(parser, OP_POP, compiler);
    statement(parser, scanner, vm, compiler);
    emitLoop(parser, loopStart, compiler);

    patchJump(parser, exitJmp, compiler);
    emitByte(parser, OP_POP, compiler);
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
    } else if (match(parser, scanner, TOKEN_FOR)) {
        forStatement(parser, scanner, vm, compiler);
    } else if (match(parser, scanner, TOKEN_IF)) {
        ifStatement(parser, scanner, vm, compiler);
    } else if (match(parser, scanner, TOKEN_WHILE)) {
        whileStatement(parser, scanner, vm, compiler);
    } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(parser, scanner, vm, compiler);
        endScope(parser, compiler);
    } else {
        expressionStatement(parser, scanner, vm, compiler);
    }
}

void initCompiler(Compiler *compiler, FunctionType ftype, VM *vm) {
    compiler->func = NULL;
    compiler->ftype = ftype;
    compiler->func = newFunction(vm);
    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    Local *local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

ObjFunction *compile(Scanner *scanner, const char *source, Chunk *chunk, VM *vm) {
    initScanner(scanner, source);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT, vm);

    compilingChunk = chunk;

    Parser parser;
    parser.hadError = false;
    parser.panicMode = false;

    advance(&parser, scanner);

    while (!match(&parser, scanner, TOKEN_EOF)) {
        declaration(&parser, scanner, vm, &compiler);
    }

    ObjFunction *func = endCompiler(&parser, &compiler);
    return parser.hadError ? NULL : func;
}
