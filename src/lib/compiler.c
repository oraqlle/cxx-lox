#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include "vm.h"

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

static void emitByte(Parser *parser, uint8_t byte, Compiler *compiler, VM *vm) {
    writeChunk(vm, compiler, currentChunk(compiler), byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2, Compiler *compiler,
                      VM *vm) {
    emitByte(parser, byte1, compiler, vm);
    emitByte(parser, byte2, compiler, vm);
}

static size_t emitJump(Parser *parser, uint8_t instruction, Compiler *compiler, VM *vm) {
    emitByte(parser, instruction, compiler, vm);
    emitByte(parser, 0xff, compiler, vm);
    emitByte(parser, 0xff, compiler, vm);

    return currentChunk(compiler)->count - 2;
}

static void emitLoop(Parser *parser, size_t loopStart, Compiler *compiler, VM *vm) {
    emitByte(parser, OP_LOOP, compiler, vm);

    size_t offset = currentChunk(compiler)->count - loopStart + 2;

    if (offset > UINT16_MAX) {
        error(parser, "Loop body too large.");
    }

    emitByte(parser, (offset >> 8) & 0xff, compiler, vm);
    emitByte(parser, offset & 0xff, compiler, vm);
}

static void emitReturn(Parser *parser, Compiler *compiler, VM *vm) {
    if (compiler->ftype == TYPE_INITIALIZER) {
        emitBytes(parser, OP_GET_LOCAL, 0, compiler, vm);
    } else {
        emitByte(parser, OP_NIL, compiler, vm);
    }

    emitByte(parser, OP_RETURN, compiler, vm);
}

static uint8_t makeConstant(Parser *parser, Value value, Compiler *compiler, VM *vm) {
    uint8_t constant = addConstant(vm, compiler, currentChunk(compiler), value);

    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return constant;
}

static void emitConstant(Parser *parser, Value value, Compiler *compiler, VM *vm) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value, compiler, vm), compiler,
              vm);
}

static void patchJump(Parser *parser, size_t offset, Compiler *compiler) {
    size_t jump = currentChunk(compiler)->count - offset - 2;

    if (jump > UINT16_MAX) {
        error(parser, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static ObjFunction *endCompiler(Parser *parser, Compiler *compiler, VM *vm) {
    emitReturn(parser, compiler, vm);

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

static void endScope(Parser *parser, Compiler *compiler, VM *vm) {
    compiler->scopeDepth -= 1;

    while (compiler->localCount > 0 &&
           compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        if (compiler->locals[compiler->localCount - 1].isCaptured) {
            emitByte(parser, OP_CLOSE_UPVALUE, compiler, vm);
        } else {
            //! Could create OP_POPN opcode to pop multiple values off the stack at once.
            emitByte(parser, OP_POP, compiler, vm);
        }

        compiler->localCount -= 1;
    }
}

// Forward declarations
static void expression(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                       ClassCompiler *currentClass);
static void declaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                        ClassCompiler *currentClass);
static void statement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                      ClassCompiler *currentClass);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            ClassCompiler *currentClass, Precedence precedence);

static uint8_t identifierConstant(Parser *parser, Token *name, Compiler *compiler,
                                  VM *vm) {
    return makeConstant(parser,
                        OBJ_VAL(copyString(vm, compiler, name->length, name->start)),
                        compiler, vm);
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

static intmax_t addUpvalue(Compiler *compiler, Parser *parser, uint8_t index,
                           bool isLocal) {
    intmax_t upvalueCount = (intmax_t)compiler->func->upvalueCount;

    for (intmax_t idx = 0; idx < upvalueCount; idx++) {
        Upvalue *upvalue = &compiler->upvalues[idx];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return idx;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error(parser, "Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index = isLocal;
    return (intmax_t)compiler->func->upvalueCount++;
}

static intmax_t resolveUpvalue(Compiler *compiler, Parser *parser, Token *name) {
    if (compiler->enclosing == NULL) {
        return -1;
    }

    intmax_t local = resolveLocal(parser, compiler, name);

    if (local != -1) {
        ((Compiler *)compiler->enclosing)->locals[local].isCaptured = true;
        return addUpvalue(compiler, parser, (uint8_t)local, true);
    }

    intmax_t upvalue = resolveUpvalue((Compiler *)compiler->enclosing, parser, name);

    if (upvalue != -1) {
        return addUpvalue(compiler, parser, (uint8_t)upvalue, false);
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
    local->isCaptured = false;
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
                   ClassCompiler *currentClass, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence(parser, scanner, vm, compiler, currentClass,
                    (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitBytes(parser, OP_EQUAL, OP_NOT, compiler, vm);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(parser, OP_EQUAL, compiler, vm);
            break;
        case TOKEN_GREATER:
            emitByte(parser, OP_GREATER, compiler, vm);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(parser, OP_LESS, OP_NOT, compiler, vm);
            break;
        case TOKEN_LESS:
            emitByte(parser, OP_LESS, compiler, vm);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(parser, OP_GREATER, OP_NOT, compiler, vm);
            break;
        case TOKEN_PLUS:
            emitByte(parser, OP_ADD, compiler, vm);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_SUBTRACT, compiler, vm);
            break;
        case TOKEN_STAR:
            emitByte(parser, OP_MULTIPLY, compiler, vm);
            break;
        case TOKEN_SLASH:
            emitByte(parser, OP_DIVIDE, compiler, vm);
            break;
        default:
            return; // Unreachable
    }
}

static uint8_t argumentList(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            ClassCompiler *currentClass) {
    uint8_t argCount = 0;

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(parser, scanner, vm, compiler, currentClass);

            if (argCount == 255) {
                error(parser, "Can't have more than 254 arguments.");
            }

            argCount += 1;
        } while (match(parser, scanner, TOKEN_COMMA));
    }

    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                 ClassCompiler *currentClass, bool canAssign) {
    uint8_t argCount = argumentList(parser, scanner, vm, compiler, currentClass);
    emitBytes(parser, OP_CALL, argCount, compiler, vm);
}

static void dot(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                ClassCompiler *currentClass, bool canAssign) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(parser, &parser->previous, compiler, vm);

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler, currentClass);
        emitBytes(parser, OP_SET_PROPERTY, name, compiler, vm);
    } else if (match(parser, scanner, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(parser, scanner, vm, compiler, currentClass);
        emitBytes(parser, OP_INVOKE, name, compiler, vm);
        emitByte(parser, argCount, compiler, vm);
    } else {
        emitBytes(parser, OP_GET_PROPERTY, name, compiler, vm);
    }
}

static void literal(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                    ClassCompiler *currentClass, bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emitByte(parser, OP_FALSE, compiler, vm);
            break;
        case TOKEN_NIL:
            emitByte(parser, OP_NIL, compiler, vm);
            break;
        case TOKEN_TRUE:
            emitByte(parser, OP_TRUE, compiler, vm);
            break;
        default:
            return; // Unreachable
    }
}

static void and_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                 ClassCompiler *currentClass, bool canAssign) {
    size_t endJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler, vm);

    emitByte(parser, OP_POP, compiler, vm);
    parsePrecedence(parser, scanner, vm, compiler, currentClass, PREC_AND);

    patchJump(parser, endJmp, compiler);
}

static void or_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                ClassCompiler *currentClass, bool canAssign) {
    size_t elseJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler, vm);
    size_t endJmp = emitJump(parser, OP_JUMP, compiler, vm);

    patchJump(parser, elseJmp, compiler);
    emitByte(parser, OP_POP, compiler, vm);

    parsePrecedence(parser, scanner, vm, compiler, currentClass, PREC_OR);
    patchJump(parser, endJmp, compiler);
}

static void grouping(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                     ClassCompiler *currentClass, bool canAssign) {
    expression(parser, scanner, vm, compiler, currentClass);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   ClassCompiler *currentClass, bool canAssign) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value), compiler, vm);
}

static void string(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   ClassCompiler *currentClass, bool canAssign) {
    emitConstant(parser,
                 OBJ_VAL(copyString(vm, compiler, parser->previous.length - 2,
                                    parser->previous.start + 1)),
                 compiler, vm);
}

static void namedVariable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                          ClassCompiler *currentClass, bool canAssign, Token name) {
    uint8_t getOp;
    uint8_t setOp;
    intmax_t arg = resolveLocal(parser, compiler, &name);

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(compiler, parser, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(parser, &name, compiler, vm);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler, currentClass);
        emitBytes(parser, setOp, (uint8_t)arg, compiler, vm);
    } else {
        emitBytes(parser, getOp, (uint8_t)arg, compiler, vm);
    }
}

static void variable(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                     ClassCompiler *currentClass, bool canAssign) {
    namedVariable(parser, scanner, vm, compiler, currentClass, canAssign,
                  parser->previous);
}

static Token syntheticToken(const char *text) {
    Token token;
    token.start = text;
    token.length = (size_t)strlen(text);
    return token;
}

static void super_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   ClassCompiler *currentClass, bool canAssign) {
    if (currentClass == NULL) {
        error(parser, "Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error(parser, "Can't use 'super in a class with no superclass.");
    }

    consume(parser, scanner, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(parser, &parser->previous, compiler, vm);

    namedVariable(parser, scanner, vm, compiler, currentClass, false,
                  syntheticToken("this"));

    if (match(parser, scanner, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(parser, scanner, vm, compiler, currentClass);
        namedVariable(parser, scanner, vm, compiler, currentClass, false,
                      syntheticToken("super"));
        emitBytes(parser, OP_SUPER_INVOKE, name, compiler, vm);
        emitByte(parser, argCount, compiler, vm);
    } else {
        namedVariable(parser, scanner, vm, compiler, currentClass, false,
                      syntheticToken("super"));
        emitBytes(parser, OP_GET_SUPER, name, compiler, vm);
    }
}

static void this_(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                  ClassCompiler *currentClass, bool canAssign) {
    if (currentClass == NULL) {
        error(parser, "Can't use 'this' outside of a class.");
        return;
    }

    variable(parser, scanner, vm, compiler, currentClass, false);
}

static void unary(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                  ClassCompiler *currentClass, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    // Compile operand.
    parsePrecedence(parser, scanner, vm, compiler, currentClass, PREC_UNARY);

    // Emit operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(parser, OP_NOT, compiler, vm);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_NEGATE, compiler, vm);
            break;
        default:
            return; // Unreachable
    }
}

// clang-format off

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
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
    [TOKEN_SUPER]         = {NULL,     super_, PREC_NONE},
    [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

// clang-format on

static void parsePrecedence(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            ClassCompiler *currentClass, Precedence precedence) {
    advance(parser, scanner);

    ParseFn prefixRule = getRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;

    prefixRule(parser, scanner, vm, compiler, currentClass, canAssign);

    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser, scanner);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, scanner, vm, compiler, currentClass, canAssign);
    }

    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void expression(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                       ClassCompiler *currentClass) {
    parsePrecedence(parser, scanner, vm, compiler, currentClass, PREC_ASSIGNMENT);
}

static void block(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                  ClassCompiler *currentClass) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(parser, scanner, vm, compiler, currentClass);
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

    return identifierConstant(parser, &parser->previous, compiler, vm);
}

static void markInitialized(Compiler *compiler) {
    if (compiler->scopeDepth == 0) {
        return;
    }

    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Parser *parser, Compiler *compiler, VM *vm, uint8_t global) {
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler);
        return;
    }

    emitBytes(parser, OP_DEFINE_GLOBAL, global, compiler, vm);
}

static void function(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                     ClassCompiler *currentClass, FunctionType ftype) {
    Compiler localCompiler;
    initCompiler(&localCompiler, compiler, ftype, parser, vm);
    beginScope(&localCompiler);

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            localCompiler.func->arity += 1;

            if (!(localCompiler.func->arity < UINT8_MAX)) {
                errorAtCurrent(parser, "Can't have more than 254 parameters.");
            }

            uint8_t constant = parseVariable(parser, scanner, vm, &localCompiler,
                                             "Expect parameter name.");
            defineVariable(parser, &localCompiler, vm, constant);
        } while (match(parser, scanner, TOKEN_COMMA));
    }

    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, scanner, TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    block(parser, scanner, vm, &localCompiler, currentClass);

    ObjFunction *func = endCompiler(parser, &localCompiler, vm);
    emitBytes(parser, OP_CLOSURE, makeConstant(parser, OBJ_VAL(func), compiler, vm),
              compiler, vm);

    for (size_t idx = 0; idx < func->upvalueCount; idx++) {
        emitByte(parser, compiler->upvalues[idx].isLocal ? 1 : 0, compiler, vm);
        emitByte(parser, compiler->upvalues[idx].index, compiler, vm);
    }
}

static void method(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                   ClassCompiler *currentClass) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(parser, &parser->previous, compiler, vm);

    FunctionType ftype = TYPE_METHOD;

    if (parser->previous.length == 4 && memcmp(parser->previous.start, "init", 4) == 0) {
        ftype = TYPE_INITIALIZER;
    }

    function(parser, scanner, vm, compiler, currentClass, ftype);
    emitBytes(parser, OP_METHOD, constant, compiler, vm);
}

static void classDeclaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                             ClassCompiler *currentClass) {
    consume(parser, scanner, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser->previous;
    uint8_t nameConstant = identifierConstant(parser, &parser->previous, compiler, vm);

    declareVariable(parser, compiler);
    emitBytes(parser, OP_CLASS, nameConstant, compiler, vm);
    defineVariable(parser, compiler, vm, nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    classCompiler.hasSuperclass = false;
    currentClass = &classCompiler;

    if (match(parser, scanner, TOKEN_LESS)) {
        consume(parser, scanner, TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(parser, scanner, vm, compiler, currentClass, false);

        if (identifiersEqual(&className, &parser->previous)) {
            error(parser, "Class cannot inherit from itself.");
        }

        beginScope(compiler);
        addLocal(parser, compiler, syntheticToken("super"));
        defineVariable(parser, compiler, vm, 0);

        namedVariable(parser, scanner, vm, compiler, currentClass, false, className);
        emitByte(parser, OP_INHERIT, compiler, vm);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(parser, scanner, vm, compiler, currentClass, false, className);
    consume(parser, scanner, TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        method(parser, scanner, vm, compiler, currentClass);
    }

    consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    pop(vm);

    if (classCompiler.hasSuperclass) {
        endScope(parser, compiler, vm);
    }

    currentClass = currentClass->enclosing;
}

static void funDeclaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                           ClassCompiler *currentClass) {
    uint8_t global =
        parseVariable(parser, scanner, vm, compiler, "Expect function name.");
    markInitialized(compiler);
    function(parser, scanner, vm, compiler, currentClass, TYPE_FUNCTION);
    defineVariable(parser, compiler, vm, global);
}

static void varDeclaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                           ClassCompiler *currentClass) {
    uint8_t global =
        parseVariable(parser, scanner, vm, compiler, "Expect variable name.");

    if (match(parser, scanner, TOKEN_EQUAL)) {
        expression(parser, scanner, vm, compiler, currentClass);
    } else {
        emitByte(parser, OP_NIL, compiler, vm);
    }

    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(parser, compiler, vm, global);
}

static void expressionStatement(Parser *parser, Scanner *scanner, VM *vm,
                                Compiler *compiler, ClassCompiler *currentClass) {
    expression(parser, scanner, vm, compiler, currentClass);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(parser, OP_POP, compiler, vm);
}

static void printStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                           ClassCompiler *currentClass) {
    expression(parser, scanner, vm, compiler, currentClass);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(parser, OP_PRINT, compiler, vm);
}

static void returnStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                            ClassCompiler *currentClass) {
    if (compiler->ftype == TYPE_SCRIPT) {
        error(parser, "Can't return from top-level code.");
    }

    if (match(parser, scanner, TOKEN_SEMICOLON)) {
        emitReturn(parser, compiler, vm);
    } else {
        if (compiler->ftype == TYPE_INITIALIZER) {
            error(parser, "Can't return a value from an initializer.");
        }

        expression(parser, scanner, vm, compiler, currentClass);
        consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(parser, OP_RETURN, compiler, vm);
    }
}

static void ifStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                        ClassCompiler *currentClass) {
    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(parser, scanner, vm, compiler, currentClass);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after 'if'.");

    size_t thenJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler, vm);
    emitByte(parser, OP_POP, compiler, vm);
    statement(parser, scanner, vm, compiler, currentClass);

    size_t elseJmp = emitJump(parser, OP_JUMP, compiler, vm);

    patchJump(parser, thenJmp, compiler);
    emitByte(parser, OP_POP, compiler, vm);

    if (match(parser, scanner, TOKEN_ELSE)) {
        statement(parser, scanner, vm, compiler, currentClass);
    }

    patchJump(parser, elseJmp, compiler);
}

static void forStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                         ClassCompiler *currentClass) {
    beginScope(compiler);

    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '('  after 'for'.");

    if (match(parser, scanner, TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(parser, scanner, vm, compiler, currentClass);
    } else {
        expressionStatement(parser, scanner, vm, compiler, currentClass);
    }

    size_t loopStart = currentChunk(compiler)->count;
    intmax_t exitJmp = -1;

    if (!match(parser, scanner, TOKEN_SEMICOLON)) {
        expression(parser, scanner, vm, compiler, currentClass);
        consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJmp = (intmax_t)emitJump(parser, OP_JUMP_IF_FALSE, compiler, vm);
        emitByte(parser, OP_POP, compiler, vm);
    }

    if (!match(parser, scanner, TOKEN_RIGHT_PAREN)) {
        size_t bodyJmp = emitJump(parser, OP_JUMP, compiler, vm);
        size_t incStart = currentChunk(compiler)->count;
        expression(parser, scanner, vm, compiler, currentClass);
        emitByte(parser, OP_POP, compiler, vm);
        consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')'  after for clauses.");

        emitLoop(parser, loopStart, compiler, vm);
        loopStart = incStart;
        patchJump(parser, bodyJmp, compiler);
    }

    statement(parser, scanner, vm, compiler, currentClass);
    emitLoop(parser, loopStart, compiler, vm);

    if (exitJmp != -1) {
        patchJump(parser, (size_t)exitJmp, compiler);
    }

    endScope(parser, compiler, vm);
}

static void whileStatement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                           ClassCompiler *currentClass) {
    size_t loopStart = currentChunk(compiler)->count;
    consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(parser, scanner, vm, compiler, currentClass);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after 'while' condition.");

    size_t exitJmp = emitJump(parser, OP_JUMP_IF_FALSE, compiler, vm);
    emitByte(parser, OP_POP, compiler, vm);
    statement(parser, scanner, vm, compiler, currentClass);
    emitLoop(parser, loopStart, compiler, vm);

    patchJump(parser, exitJmp, compiler);
    emitByte(parser, OP_POP, compiler, vm);
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

static void declaration(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                        ClassCompiler *currentClass) {
    if (match(parser, scanner, TOKEN_CLASS)) {
        classDeclaration(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_FUN)) {
        funDeclaration(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_VAR)) {
        varDeclaration(parser, scanner, vm, compiler, currentClass);
    } else {
        statement(parser, scanner, vm, compiler, currentClass);
    }

    if (parser->panicMode) {
        synchronize(parser, scanner);
    }
}

static void statement(Parser *parser, Scanner *scanner, VM *vm, Compiler *compiler,
                      ClassCompiler *currentClass) {
    if (match(parser, scanner, TOKEN_PRINT)) {
        printStatement(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_FOR)) {
        forStatement(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_IF)) {
        ifStatement(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_RETURN)) {
        returnStatement(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_WHILE)) {
        whileStatement(parser, scanner, vm, compiler, currentClass);
    } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(parser, scanner, vm, compiler, currentClass);
        endScope(parser, compiler, vm);
    } else {
        expressionStatement(parser, scanner, vm, compiler, currentClass);
    }
}

void initCompiler(Compiler *compiler, Compiler *enclosing, FunctionType ftype,
                  Parser *parser, VM *vm) {
    compiler->enclosing = enclosing;

    compiler->func = NULL;
    compiler->ftype = ftype;
    compiler->func = newFunction(vm, compiler);
    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    if (ftype != TYPE_SCRIPT) {
        compiler->func->name =
            copyString(vm, compiler, parser->previous.length, parser->previous.start);
    }

    Local *local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;

    if (ftype != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

ObjFunction *compile(Scanner *scanner, const char *source, VM *vm) {
    initScanner(scanner, source);

    Parser parser;
    parser.hadError = false;
    parser.panicMode = false;

    Compiler compiler;
    initCompiler(&compiler, NULL, TYPE_SCRIPT, &parser, vm);

    advance(&parser, scanner);

    while (!match(&parser, scanner, TOKEN_EOF)) {
        declaration(&parser, scanner, vm, &compiler, NULL);
    }

    ObjFunction *func = endCompiler(&parser, &compiler, vm);
    return parser.hadError ? NULL : func;
}

void markCompilerRoots(VM *vm, Compiler *compiler) {
    while (compiler != NULL) {
        markObject(vm, (Obj *)compiler->func);
        compiler = compiler->enclosing;
    }
}
