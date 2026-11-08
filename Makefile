CC       = gcc
CFLAGS   = -Wall -O2 -Isrc -Ibuild
WASI_SDK ?= /opt/wasi-sdk

SRC = src/ast.c src/sema.c src/interp.c src/codegen.c src/main.c
GEN = build/parser.tab.c build/lex.yy.c

all: cparse

build/parser.tab.c: src/parser.y
	mkdir -p build
	bison -d -o build/parser.tab.c src/parser.y

build/lex.yy.c: src/lexer.l build/parser.tab.c
	flex -o build/lex.yy.c src/lexer.l

cparse: $(GEN) $(SRC) src/ast.h
	$(CC) $(CFLAGS) -o $@ $(GEN) $(SRC) -lm

web: $(GEN)
	$(WASI_SDK)/bin/clang --target=wasm32-wasi --sysroot=$(WASI_SDK)/share/wasi-sysroot -Os \
		-DMAX_DEPTH=1000 -DMAX_STEPS=3000000 -Isrc -Ibuild -Wl,-z,stack-size=8388608 -Wl,--strip-all \
		-o build/cparse.wasm $(GEN) $(SRC) -lm
	printf 'const WASM = "%s";\n' "$$(base64 < build/cparse.wasm | tr -d '\n')" > docs/wasm.js
	python3 -c "import json; \
	ex = [('demo.c', 'Факторіал і цикл for'), ('sort.c', 'Сортування масиву'), ('math.c', 'Вирази і типи'), \
	      ('syntax_errors.c', 'Синтаксичні помилки'), ('semantic_errors.c', 'Семантичні помилки')]; \
	print('const EXAMPLES = ' + json.dumps([{'title': t, 'code': open('examples/' + f).read()} for f, t in ex], ensure_ascii=False) + ';'); \
	print('const SOURCES = ' + json.dumps({'parser': open('src/parser.y').read(), 'lexer': open('src/lexer.l').read()}, ensure_ascii=False) + ';')" > docs/data.js

clean:
	rm -rf build cparse

.PHONY: all web clean
