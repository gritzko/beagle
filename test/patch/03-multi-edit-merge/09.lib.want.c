#include <stdio.h>

/* math */
int add(int a, int b)
{
    int s = a + b;
    return s;
}

int sub(int a, int b)
{
    return a - b;
}

int mul(int a, int b)
{
    return a * b;
}

/* string */
const char *hello = "Hello!";
const char *goodbye = "Farewell.";
const char *welcome = "Welcome!";
const char *thanks = "Thanks!";

/* io */
void print_line(const char *m)
{
    printf("[log] %s\n", m);
}

void print_err(const char *m)
{
    fprintf(stderr, "%s\n", m);
}

int main(void)
{
    return 0;
}
