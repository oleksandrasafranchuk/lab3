#include <stdio.h>
#include <math.h>

const double EPS = 1e-9;

double newton_sqrt(double x)
{
    double r = x > 1 ? x / 2 : 1.0;
    while (fabs(r * r - x) > EPS)
        r = (r + x / r) / 2;
    return r;
}

int power(int base, int e)
{
    int result = 1;
    while (e > 0) {
        if (e & 1) result *= base;
        base *= base;
        e >>= 1;
    }
    return result;
}

int max3(int a, int b, int c)
{
    return a > b ? (a > c ? a : c) : (b > c ? b : c);
}

int main()
{
    printf("sqrt(2): newton = %.6f, libm = %.6f\n", newton_sqrt(2), sqrt(2.0));
    printf("2^10 = %d, 3^5 = %d\n", power(2, 10), power(3, 5));
    printf("max3(4, 9, 7) = %d\n", max3(4, 9, 7));

    int x = 7;
    double d = x / 2;
    double e = (double)x / 2;
    int t = 3.99;
    printf("x/2 = %.1f, (double)x/2 = %.1f, (int)3.99 = %d\n", d, e, t);

    int a = 5, b = 3;
    printf("a&b=%d a|b=%d a^b=%d ~a=%d a<<2=%d\n", a & b, a | b, a ^ b, ~a, a << 2);
    printf("a&&b=%d !a=%d a<b=%d\n", a && b, !a, a < b);

    int i = 5;
    int j = i++;
    int k = ++i * 2;
    printf("i=%d j=%d k=%d\n", i, j, k);

    int c = 10;
    c += 5; c -= 3; c *= 2; c /= 5; c %= 3;
    printf("c = %d\n", c);

    char ch = 'A';
    ch = ch + 2;
    printf("ch = %c (%d)\n", ch, ch);
    return 0;
}
