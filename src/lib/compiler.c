#include "compiler.h"
#include "scanner.h"

void compile(VM *vm, Scanner *scanner, const char *source) {
    initScanner(scanner, source);
}
