#include <stdio.h>
// Демонстрація складних випадків трансляції: перекриття змінних,
// присвоєння у виразах, ++/--, continue у for, глобальні змінні,
// неявні перетворення типів, імена-ключові слова Python (lambda, def).
int total = 0;
double avg = 0;
int lambda(int def) { return def * 2; }
void add(int v) { total += v; }
int main() {
    int a = 2, b = 3, c;
    int x = 1;
    {
        int x = 10;
        x++;
        printf("inner x=%d\n", x);
    }
    printf("outer x=%d\n", x);
    c = a = b = 7;
    printf("a=%d b=%d c=%d\n", a, b, c);
    int i = 0;
    while ((i += 3) < 10) printf("i=%d ", i);
    printf("\n");
    int k = 5;
    k++;
    ++k;
    printf("k=%d\n", k);
    printf("logic: %d %d %d %d\n", 2 && 3, 0 || 5, !7, !0);
    printf("cmp: %d %d\n", (1 < 2) == 1, 3 > 2 > 1);
    int s = 0;
    for (int j = 0; j < 10; j++) {
        if (j % 2 == 0) continue;
        if (j > 7) break;
        for (int t = 0; t < 3; t++) { if (t == 1) continue; s += t; }
        s += j;
    }
    printf("s=%d\n", s);
    for (int j = 0; j < 3; j++) add(j * 10);
    printf("total=%d lambda=%d\n", total, lambda(21));
    int q = 17;
    q /= 2.5;
    double r = 7;
    r /= 2;
    q %= 4;
    printf("q=%d r=%.2f %d %d\n", q, r, -17 / 5, -17 % 5);
    avg = total / 4;
    printf("avg=%.2f %.3f\n", avg, total / 4.0);
    int neg = -(-3);
    printf("neg=%d %d\n", neg, - -neg);
    for (;;) { if (++neg > 5) break; }
    printf("neg=%d\n", neg);
    if (a > 100) printf("big\n"); else if (a > 5) printf("mid\n"); else printf("small\n");
    double big = 1e10, small = .5, tenth = 0.1;
    printf("%g %g %.17g %d\n", big, small, tenth, 0 + 2);
    return 0;
}
