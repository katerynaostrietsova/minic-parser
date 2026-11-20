#include <stdio.h>

// Квадратний корінь методом Ньютона
double my_sqrt(double x) {
    double g = x / 2.0;
    for (int i = 0; i < 20; i++)
        g = (g + x / g) / 2.0;
    return g;
}

double power(double base, int e) {
    double r = 1.0;
    while (e > 0) {
        r *= base;
        e--;
    }
    return r;
}

// "Висячий else": else належить найближчому if
void sign(int v) {
    if (v >= 0)
        if (v == 0)
            printf("%d is zero\n", v);
        else
            printf("%d is positive\n", v);
    else
        printf("%d is negative\n", v);
}

int main() {
    printf("sqrt(2) = %.6f\n", my_sqrt(2.0));
    printf("2^10 = %.1f\n", power(2.0, 10));
    printf("2 + 3 * 4 = %d\n", 2 + 3 * 4);
    printf("(2 + 3) * 4 = %d\n", (2 + 3) * 4);
    printf("10 - 4 - 3 = %d\n", 10 - 4 - 3);
    printf("7 / 2 = %d, 7.0 / 2 = %.2f\n", 7 / 2, 7.0 / 2);
    printf("-7 / 2 = %d, -7 %% 2 = %d\n", -7 / 2, -7 % 2);
    sign(5);
    sign(0);
    sign(-3);
    return 0;
}
