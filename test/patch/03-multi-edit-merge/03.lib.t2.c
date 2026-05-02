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
const char *greet_str = "hi";
const char *bye_str = "bye";
const char *placeholder_s = NULL;

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
