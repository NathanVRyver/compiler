CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = 

SRC = main.c tokenizer.c parser.c semantic.c codegen.c
OBJ = $(SRC:.c=.o)
TARGET = ccompiler

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) output.ll

test: $(TARGET)
	./$(TARGET) test.c

# Dependencies
main.o: main.c tokenizer.h parser.h semantic.h codegen.h
tokenizer.o: tokenizer.c tokenizer.h
parser.o: parser.c parser.h tokenizer.h
semantic.o: semantic.c semantic.h parser.h
codegen.o: codegen.c codegen.h parser.h 