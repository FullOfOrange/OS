#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctype.h>

int isOperator(char c)
{
    if (c == '+' || c == '*' || c == '-' || c == '%')
    {
        return 1;
    }
    return 0;
}

int main()
{
    char a[100];
    char b[100];
    char buf[100];

    int operIdx = 0, oper = 0, len = 0;

    while (1)
    {
        printf(">> ");
        scanf("%s", buf);
        len = strlen(buf);

        for (int i = 0; i < 100; i++)
            if (isOperator(buf[i]))
            {
                oper = buf[i];
                operIdx = i;
                break;
            }

        for (int i = 0; i < operIdx; i++)
            a[i] = buf[i];

        a[operIdx] = 0;

        for (int i = operIdx + 1; i < len; i++)
            b[i - operIdx - 1] = buf[i];

        b[len - operIdx - 1] = 0;

        if (oper == '+')
            printf("%ld\n", syscall(443, atoi(a), atoi(b)));
        if (oper == '-')
            printf("%ld\n", syscall(444, atoi(a), atoi(b)));
        if (oper == '*')
            printf("%ld\n", syscall(445, atoi(a), atoi(b)));
        if (oper == '%')
            printf("%ld\n", syscall(446, atoi(a), atoi(b)));
    }

    return 0;
}