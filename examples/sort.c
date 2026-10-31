#include <stdio.h>

void print_array(int a[], int n)
{
    for (int i = 0; i < n; i++)
        printf("%d%c", a[i], i == n - 1 ? '\n' : ' ');
}

void bubble_sort(int a[], int n)
{
    int swapped;
    do {
        swapped = 0;
        for (int i = 1; i < n; i++) {
            if (a[i - 1] > a[i]) {
                int t = a[i - 1];
                a[i - 1] = a[i];
                a[i] = t;
                swapped = 1;
            }
        }
        n--;
    } while (swapped);
}

int binary_search(int a[], int n, int key)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == key) return mid;
        else if (a[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int main()
{
    int a[10] = { 42, -7, 15, 0, 99, 3, 15, -20, 8, 61 };
    printf("before: ");
    print_array(a, 10);
    bubble_sort(a, 10);
    printf("after:  ");
    print_array(a, 10);
    printf("index of 42: %d\n", binary_search(a, 10, 42));
    printf("index of 5:  %d\n", binary_search(a, 10, 5));
    return 0;
}
