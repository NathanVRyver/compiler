# Simple C Compiler

This is a simple compiler for a subset of the C language. It demonstrates the basic components of a compiler:

1. **Lexical Analysis (Tokenizer)** - Breaking the source code into tokens
2. **Syntax Analysis (Parser)** - Building an Abstract Syntax Tree (AST)
3. **Semantic Analysis** - Checking types and scopes
4. **Code Generation** - Generating LLVM IR code

## Project Structure

```
.
├── include/                 # Header files
│   ├── tokenizer/           # Tokenizer module headers
│   ├── parser/              # Parser module headers
│   ├── semantic/            # Semantic analyzer module headers
│   └── codegen/             # Code generator module headers
├── src/                     # Source files
│   ├── tokenizer/           # Tokenizer implementation
│   ├── parser/              # Parser implementation
│   ├── semantic/            # Semantic analyzer implementation
│   ├── codegen/             # Code generator implementation
│   └── main.c               # Main compiler driver program
└── examples/                # Example C files for testing
    ├── test.c               # Simple test program
    └── factorial.c          # Factorial calculation example
```

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
./ccompiler examples/test.c [output.ll]
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
