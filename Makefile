CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic

LIB_SOURCES = \
	src/lexer/lexer.c \
	src/diag/diag.c \
	src/parser/parser.c \
	src/ast/ast.c \
	src/utils/file.c \
	src/semantic/semantic.c \
	src/runtime/interp.c \
	src/codegen/codegen.c

SOURCES = src/main.c $(LIB_SOURCES)
TEST_SOURCES = tests/test_unit.c $(LIB_SOURCES)

TARGET = kratosc
TEST_TARGET = tests/test_unit

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -Isrc $(SOURCES) -o $(TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	$(CC) $(CFLAGS) -Isrc $(TEST_SOURCES) -o $(TEST_TARGET)

run: $(TARGET)
	./$(TARGET) examples/hello.kratos

test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET)
	./$(TARGET) examples/hello.kratos | grep -q 'Hello, Kratos'
	./$(TARGET) examples/loops.kratos | grep -q '6'
	./$(TARGET) examples/wield_main.kratos | grep -q 'imported'
	./$(TARGET) examples/short_circuit.kratos | grep -q 'ok'
	! ./$(TARGET) examples/short_circuit.kratos | grep -q 'boom'
	./$(TARGET) --ast examples/hello.kratos | grep -q 'FuncDecl main'
	./$(TARGET) --version | grep -q '^kratosc 0.1.0$$'
	./$(TARGET) --check examples/hello.kratos | grep -q 'Kratos-Lang: no errors found'
	! printf 'k_void craft main( {\n' | ./$(TARGET) --check >/dev/null 2>&1
	./$(TARGET) -o /tmp/kratosc_test examples/hello.kratos
	/tmp/kratosc_test | grep -q 'Hello, Kratos'
	./$(TARGET) --emit-c examples/hello.kratos | $(CC) -std=c11 -x c - -o /tmp/kratos_hello_c
	/tmp/kratos_hello_c | grep -q 'Hello, Kratos'

clean:
	rm -f $(TARGET) $(TEST_TARGET) /tmp/kratos_hello_c /tmp/kratosc_test
