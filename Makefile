CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic

SOURCES = src/main.c src/lexer/lexer.c
TARGET = kratosc

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) src/lexer/lexer.h
	$(CC) $(CFLAGS) -Isrc $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
