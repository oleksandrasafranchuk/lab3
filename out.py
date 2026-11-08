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


def print_array(a, n):
    i = 0
    while i < n:
        printf("%d%c", a[i], 10 if i == n - 1 else ord(' '))
        i += 1


def bubble_sort(a, n):
    swapped = 0
    while True:
        swapped = 0
        i = 1
        while i < n:
            if a[i - 1] > a[i]:
                t = a[i - 1]
                a[i - 1] = a[i]
                a[i] = t
                swapped = 1
            i += 1
        n -= 1
        if not swapped:
            break


def binary_search(a, n, key):
    lo = 0
    hi = n - 1
    while lo <= hi:
        mid = _idiv(lo + hi, 2)
        if a[mid] == key:
            return mid
        elif a[mid] < key:
            lo = mid + 1
        else:
            hi = mid - 1
    return -1


def main():
    a = [42, -7, 15, 0, 99, 3, 15, -20, 8, 61]
    printf("before: ")
    print_array(a, 10)
    bubble_sort(a, 10)
    printf("after:  ")
    print_array(a, 10)
    printf("index of 42: %d\n", binary_search(a, 10, 42))
    printf("index of 5:  %d\n", binary_search(a, 10, 5))
    return 0


if __name__ == "__main__":
    sys.exit(main())
