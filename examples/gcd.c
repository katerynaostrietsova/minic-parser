#include <stdio.h>

/* Алгоритм Евкліда: НСД та НСК */
int gcd(int a, int b) {
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

int lcm(int a, int b) {
    return a / gcd(a, b) * b;
}

int main() {
    printf("gcd(48, 18) = %d\n", gcd(48, 18));
    printf("gcd(1071, 462) = %d\n", gcd(1071, 462));
    printf("lcm(4, 6) = %d\n", lcm(4, 6));
    printf("lcm(21, 6) = %d\n", lcm(21, 6));
    return 0;
}
