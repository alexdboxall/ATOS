#pragma once

/*
 * SIG_DFL must be 0, as we memset() the default signal structure to zero.
 */
#define SIG_DFL         0
#define SIG_IGN         1
#define SIG_ERR         -1

#define SIGABRT         0
#define SIGALRM         1
#define SIGBUS          2
#define SIGCHLD         3
#define SIGCONT         4
#define SIGFPE          5
#define SIGHUP          6
#define SIGILL          7
#define SIGINT          8
#define SIGKILL         9
#define SIGPIPE         10
#define SIGQUIT         11
#define SIGSEGV         12
#define SIGSTOP         13
#define SIGTERM         14
#define SIGTSTP         15
#define SIGTTIN         16
#define SIGTTOU         17
#define SIGUSR1         18
#define SIGUSR2         19
#define SIGPOLL         20
#define SIGPROF         21
#define SIGSYS          22
#define SIGTRAP         23
#define SIGURG          24
#define SIGVTALRM       25
#define SIGXCPU         26
#define SIGXFSZ         27
#define _SIG_UPPER_BND  28

#ifdef COMPILE_KERNEL

#define _SIG_HND        2
#define _SIG_CNT        3
#define _SIG_TRM        4
#define _SIG_ABT        5

#include <stddef.h>
struct thread;
struct signal_state;
size_t signal_check(struct thread* thread);
struct signal_state* signal_create_state();

#endif