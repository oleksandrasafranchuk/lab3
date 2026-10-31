CC      = gcc
CFLAGS  = -Wall -O2 -Isrc -Ibuild
BUILD   = build
FILE   ?= examples/demo.c

SRC = src/ast.c src/sema.c src/interp.c src/codegen.c src/main.c
GEN = $(BUILD)/parser.tab.c $(BUILD)/lex.yy.c

.PHONY: all run tree py viz test clean

all: cparse

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h: src/parser.y | $(BUILD)
	bison -d -v -Wcounterexamples -o $(BUILD)/parser.tab.c src/parser.y

$(BUILD)/lex.yy.c: src/lexer.l $(BUILD)/parser.tab.h
	flex -o $(BUILD)/lex.yy.c src/lexer.l

cparse: $(GEN) $(SRC) src/ast.h
	$(CC) $(CFLAGS) -o $@ $(GEN) $(SRC) -lm

run: cparse
	./cparse -r $(FILE)

tree: cparse
	./cparse -t $(FILE)

py: cparse
	./cparse -p out.py $(FILE)
	python3 out.py

viz: cparse
	./cparse -v viz/ast.js $(FILE)
	@echo "Open viz/index.html in a browser"

test: cparse
	sh tests/run_tests.sh

clean:
	rm -rf $(BUILD) cparse out.py tests/out
