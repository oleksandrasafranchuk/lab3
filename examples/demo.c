#include <stdio.h>

int fact(int n)
{
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

int main(void)
{
    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum += fact(i);
    }
    double avg = sum / 5.0;
    printf("sum = %d, avg = %.2f\n", sum, avg);
    return 0;
}
