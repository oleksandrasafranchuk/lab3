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

EPS = 1e-9


def newton_sqrt(x):
    global EPS
    r = x / 2 if x > 1 else 1.0
    while fabs(r * r - x) > EPS:
        r = (r + x / r) / 2
    return r


def power(base, e):
    result = 1
    while e > 0:
        if e & 1:
            result *= base
        base *= base
        e >>= 1
    return result


def max3(a, b, c):
    return (a if a > c else c) if a > b else b if b > c else c


def main():
    printf("sqrt(2): newton = %.6f, libm = %.6f\n", newton_sqrt(2.0), sqrt(2.0))
    printf("2^10 = %d, 3^5 = %d\n", power(2, 10), power(3, 5))
    printf("max3(4, 9, 7) = %d\n", max3(4, 9, 7))
    x = 7
    d = _idiv(x, 2)
    e = float(x) / 2
    t = int(3.99)
    printf("x/2 = %.1f, (double)x/2 = %.1f, (int)3.99 = %d\n", d, e, t)
    a = 5
    b = 3
    printf("a&b=%d a|b=%d a^b=%d ~a=%d a<<2=%d\n", a & b, a | b, a ^ b, ~a, a << 2)
    printf("a&&b=%d !a=%d a<b=%d\n", int(bool(a) and bool(b)), int(not a), int(a < b))
    i = 5
    j = (i := i + 1) - 1
    k = (i := i + 1) * 2
    printf("i=%d j=%d k=%d\n", i, j, k)
    c = 10
    c += 5
    c -= 3
    c *= 2
    c = _idiv(c, 5)
    c = _imod(c, 3)
    printf("c = %d\n", c)
    ch = ord('A')
    ch = ch + 2
    printf("ch = %c (%d)\n", ch, ch)
    return 0


if __name__ == "__main__":
    sys.exit(main())
