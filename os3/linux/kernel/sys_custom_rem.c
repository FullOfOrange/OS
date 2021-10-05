#include <linux/kernel.h>
#include <linux/syscalls.h>

asmlinkage long sys_custom_rem(int a, int b) {
        printk("%d %% %d\n", a, b);
        return a % b;
}

SYSCALL_DEFINE2(custom_rem, int, a, int, b) {
        return sys_custom_rem(a, b);
}
