/usr/src/linux/linux-5.11.22/arch/x86/entry/syscalls

커널 빌드했던 이곳에서 작업 진행

syscall_64.tbl 파일 수정

내 파일에는 common 이후에 파일을 추가하라고 해서 이곳에 추가하기로 함


```
434     common  pidfd_open              sys_pidfd_open
435     common  clone3                  sys_clone3
436     common  close_range             sys_close_range
437     common  openat2                 sys_openat2
438     common  pidfd_getfd             sys_pidfd_getfd
439     common  faccessat2              sys_faccessat2
440     common  process_madvise         sys_process_madvise
441     common  epoll_pwait2            sys_epoll_pwait2

#
# Due to a historical design error, certain syscalls are numbered differently
# in x32 as compared to native x86_64.  These syscalls have numbers 512-547.
# Do not add new syscalls to this range.  Numbers 548 and above are available
# for non-x32 use.
#
512     x32     rt_sigaction            compat_sys_rt_sigaction
513     x32     rt_sigreturn            compat_sys_x32_rt_sigreturn
514     x32     ioctl                   compat_sys_ioctl
515     x32     readv                   sys_readv
516     x32     writev                  sys_writev
517     x32     recvfrom                compat_sys_recvfrom
518     x32     sendmsg                 compat_sys_sendmsg
519     x32     recvmsg                 compat_sys_recvmsg
```
