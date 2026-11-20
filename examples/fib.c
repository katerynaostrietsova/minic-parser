#include <stdio.h>

// Числа Фібоначчі: рекурсивно та ітеративно
int fib_rec(int n) {
    if (n < 2) return n;
    return fib_rec(n - 1) + fib_rec(n - 2);
}

int fib_iter(int n) {
    int a = 0, b = 1, t;
    while (n > 0) {
        t = a + b;
        a = b;
        b = t;
        n -= 1;
    }
    return a;
}

int main() {
    int i;
    for (i = 0; i <= 15; i++)
        printf("fib(%d) = %d / %d\n", i, fib_rec(i), fib_iter(i));
    return 0;
}
