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

sieve = [0] * 101
count = 0


def gcd(a, b):
    while b != 0:
        t = _imod(a, b)
        a = b
        b = t
    return a


def main():
    global count
    i = 2
    while i <= 100:
        if sieve[i]:
            i += 1
            continue
        count += 1
        j = i * i
        while j <= 100:
            sieve[j] = 1
            j += i
        i += 1
    printf("primes up to 100: %d\n", count)
    k = 0
    i = 2
    while True:
        if sieve[i]:
            i += 1
            continue
        printf("%d ", i)
        if (k := k + 1) == 10:
            break
        i += 1
    printf("\n")
    n = 0
    while True:
        n += 1
        if _imod(n, 2) == 0:
            if not n < 5:
                break
            continue
        printf("odd %d\n", n)
        if not n < 5:
            break
    printf("gcd(84, 36) = %d\n", gcd(84, 36))
    printf("-7 / 2 = %d, -7 %% 2 = %d\n", _idiv(-7, 2), _imod(-7, 2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
