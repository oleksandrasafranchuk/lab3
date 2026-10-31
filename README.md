# C Parser (Flex + Bison)

Синтаксичний аналізатор підмножини мови C: AST, семантичний аналіз, інтерпретатор, трансляція в Python, візуалізація AST (d3.js).

## Збірка

```bash
make
make test
```

## Використання

```bash
./cparse -t examples/demo.c              # AST у терміналі
./cparse -r examples/sort.c              # виконання
./cparse -p out.py examples/sort.c       # трансляція в Python
./cparse -y examples/demo.c              # трасування LALR-парсера
./cparse -v viz/ast.js examples/sort.c   # дані для візуалізації
```

Візуалізація: відкрити `viz/index.html` у браузері.

## Структура

| Файл             | Призначення               |
| ---------------- | ------------------------- |
| `src/lexer.l`    | лексичний аналізатор      |
| `src/parser.y`   | граматика C, побудова AST |
| `src/ast.c`      | AST, друк, JSON           |
| `src/sema.c`     | семантичний аналіз        |
| `src/interp.c`   | інтерпретатор             |
| `src/codegen.c`  | генерація Python          |
| `viz/index.html` | візуалізація AST          |
