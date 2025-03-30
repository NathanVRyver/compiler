CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = 

# Source files
SRC = src/main.c \
      src/tokenizer/tokenizer.c \
      src/parser/parser.c \
      src/semantic/semantic.c \
      src/codegen/codegen.c

# Object files
OBJ = $(SRC:.c=.o)
TARGET = ccompiler

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) output.ll

test: $(TARGET)
	./$(TARGET) examples/test.c

# Dependencies
src/main.o: src/main.c include/tokenizer/tokenizer.h include/parser/parser.h include/semantic/semantic.h include/codegen/codegen.h
src/tokenizer/tokenizer.o: src/tokenizer/tokenizer.c include/tokenizer/tokenizer.h
src/parser/parser.o: src/parser/parser.c include/parser/parser.h include/tokenizer/tokenizer.h
src/semantic/semantic.o: src/semantic/semantic.c include/semantic/semantic.h include/parser/parser.h
src/codegen/codegen.o: src/codegen/codegen.c include/codegen/codegen.h include/parser/parser.h 