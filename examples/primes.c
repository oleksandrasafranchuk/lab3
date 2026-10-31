#include <stdio.h>

int sieve[101];
int count = 0;

int gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int main()
{
    for (int i = 2; i <= 100; i++) {
        if (sieve[i]) continue;
        count++;
        for (int j = i * i; j <= 100; j += i)
            sieve[j] = 1;
    }
    printf("primes up to 100: %d\n", count);

    int k = 0;
    for (int i = 2; ; i++) {
        if (sieve[i]) continue;
        printf("%d ", i);
        if (++k == 10) break;
    }
    printf("\n");

    int n = 0;
    do {
        n++;
        if (n % 2 == 0) continue;
        printf("odd %d\n", n);
    } while (n < 5);

    printf("gcd(84, 36) = %d\n", gcd(84, 36));
    printf("-7 / 2 = %d, -7 %% 2 = %d\n", -7 / 2, -7 % 2);
    return 0;
}
