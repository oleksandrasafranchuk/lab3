import math
import re
import sys


def printf(fmt, *args):
    fmt = re.sub(r'%([-+ #0]*\d*(?:\.\d+)?)(?:hh|h|ll|l|L|z|j|t)', r'%\1', fmt)
    s = fmt % args
    sys.stdout.write(s)
    return len(s)


def puts(s):
    sys.stdout.write(s + '\n')
    return 0


def putchar(c):
    sys.stdout.write(chr(c))
    return c


def _idiv(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def _imod(a, b):
    return a - b * _idiv(a, b)


def _set(a, i, v):
    a[i] = v
    return v


def _preinc(a, i, d):
    a[i] += d
    return a[i]


def _postinc(a, i, d):
    a[i] += d
    return a[i] - d


sqrt, pow, fabs = math.sqrt, math.pow, math.fabs


def fact(n):
    if n <= 1:
        return 1
    return n * fact(n - 1)


def main():
    sum_ = 0
    i = 1
    while i <= 5:
        sum_ += fact(i)
        i += 1
    avg = sum_ / 5.0
    printf("sum = %d, avg = %.2f\n", sum_, avg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
