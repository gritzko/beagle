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
void log_msg(const char *m)
{
    printf("%s\n", m);
}

void placeholder_io(void)
{
}

int main(void)
{
    return 0;
}
