# Simple C Compiler

This is a simple compiler for a subset of the C language. It demonstrates the basic components of a compiler:

1. **Lexical Analysis (Tokenizer)** - Breaking the source code into tokens
2. **Syntax Analysis (Parser)** - Building an Abstract Syntax Tree (AST)
3. **Semantic Analysis** - Checking types and scopes
4. **Code Generation** - Generating LLVM IR code

## Project Structure

- `tokenizer.h/c` - Lexical analyzer that converts C code into tokens
- `parser.h/c` - Parser that builds an AST from tokens
- `semantic.h/c` - Semantic analyzer for type checking and scope analysis
- `codegen.h/c` - Code generator that outputs LLVM IR
- `main.c` - Main compiler driver program
- `Makefile` - Build system configuration

## Building the Compiler

```bash
# Build the compiler
make

# Clean build files
make clean
```

## Usage

```bash
# Compile a C source file
./ccompiler input.c [output.ll]
```

If no output file is specified, the default `output.ll` will be used.

## Testing

```bash
# Run the compiler on the test file
make test
```

## Supported Features

This compiler supports a subset of C language features:

- Basic variable declarations and initialization
- Function declarations and calls
- Arithmetic expressions
- Control flow statements (if-else, while, for)
- Return statements

## Output Format

The compiler generates LLVM IR code which can be further compiled to machine code using the LLVM toolchain.

## Future Improvements

- Support for more C language features
- Better error handling and recovery
- Optimization passes
- Direct code generation to assembly or object code

## License

This project is open source and available under the MIT License.
