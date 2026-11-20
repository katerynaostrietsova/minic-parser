#include <stdio.h>

int count = 0;   // глобальна змінна

// Перевірка числа на простоту
int is_prime(int n) {
    if (n < 2) return 0;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0) return 0;
    }
    return 1;
}

int main() {
    // перші 15 простих чисел: continue та break
    for (int n = 1; n <= 100; n++) {
        if (!is_prime(n)) continue;
        printf("%d ", n);
        count++;
        if (count >= 15) break;
    }
    printf("\nprinted %d primes\n", count);

    // логічні оператори: && має вищий пріоритет, ніж ||
    int x = 5, y = -10;
    if (x > 0 && y > 0 || x == 5)
        printf("condition is true\n");
    else
        printf("condition is false\n");
    return 0;
}
