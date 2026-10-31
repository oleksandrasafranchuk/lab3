#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
mkdir -p tests/out
pass=0
fail=0

for f in examples/demo.c examples/sort.c examples/primes.c examples/math.c; do
    name=$(basename "$f" .c)
    gcc -w -o "tests/out/$name.bin" "$f" -lm
    "./tests/out/$name.bin" > "tests/out/$name.gcc.txt"
    ./cparse -r "$f" > "tests/out/$name.run.txt"
    ./cparse -p "tests/out/py_$name.py" "$f"
    python3 "tests/out/py_$name.py" > "tests/out/py_$name.py.txt"

    if cmp -s "tests/out/$name.gcc.txt" "tests/out/$name.run.txt" &&
       cmp -s "tests/out/$name.gcc.txt" "tests/out/py_$name.py.txt"; then
        echo "PASS  $f  (gcc == interpreter == python)"
        pass=$((pass + 1))
    else
        echo "FAIL  $f"
        diff "tests/out/$name.gcc.txt" "tests/out/$name.run.txt"
        diff "tests/out/$name.gcc.txt" "tests/out/py_$name.py.txt"
        fail=$((fail + 1))
    fi
done

for f in examples/syntax_errors.c examples/semantic_errors.c; do
    if ./cparse "$f" 2>/dev/null; then
        echo "FAIL  $f  (errors not detected)"
        fail=$((fail + 1))
    else
        echo "PASS  $f  (errors detected)"
        pass=$((pass + 1))
    fi
done

echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]
