
#include <thread.h>
#include <kprintf.h>
#include <signal.h>

/*
* thread/signal.c - C Signals
*
* 
*/


struct signal_state {
    uint64_t masked_signals;
    uint64_t pending_signals;
    
    int mode[64];
    size_t handlers[64];
};

static int signal_get_default_action(int number) {
    switch (number) {
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGILL:
        case SIGQUIT:
        case SIGSEGV:
        case SIGSYS:
        case SIGTRAP:
        case SIGXCPU:
        case SIGXFSZ:
            return _SIG_ABT;
        case SIGALRM:
        case SIGHUP:
        case SIGINT:
        case SIGKILL:
        case SIGPIPE:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGPOLL:
        case SIGPROF:
        case SIGVTALRM:
            return _SIG_TRM;
        case SIGCHLD:
        case SIGURG:
            return _SIG_IGN;
        case SIGCONT:
            return _SIG_CNT;
        default:
            return SIG_ERR;
    } 
}

void signal_finish(int number) {
    struct signal_state* signals;

    signals->pending_signals &= ~(1 << number);
}

int signal_install_handler(int number, size_t handler) {
    if (number == SIGKILL || number == SIGSTOP || number < 0 || number >= _SIG_UPPER_BND) {
        return EINVAL;
    }

    struct signal_state* signals;

    if (handler == SIG_DFL) {
        signals->mode[number] = SIG_DFL;

    } else if (handler == SIG_IGN) {
        signals->mode[number] = SIG_IGN;

    } else {
        signals->mode[number] = _SIG_HND;
        signals->handler[number] = handler;
    }
}

/*
* If a signal is pending and unmasked, this returns the address of the signal handler of the
* highest priority. In this case, signal_check() should be called again to get any futher
* signals that need handling. Return 0 if there is no signal handler to run at this time.
*/
size_t signal_check() {
    uint64_t
    struct signal_state* signals;

    uint64_t pendingNotMasked = signals->pending_signals & (!signals->masked_signals);
    if (pendingNotMasked) {
        int number = 0;
        while (!(pendingNotMasked & 1)) {
            ++number;
            pendingNotMasked >>= 1;
        }

        /*
        * If the specified action for a kind of signal is to ignore it, then any such signal which
        * is generated is discarded immediately. This happens even if the signal is also blocked at
        * the time. A signal discarded in this way will never be delivered, not even if the program 
        * subsequently specifies a different action for that kind of signal and then unblocks it. 
        */
        int action = signals->mode[number];
        if (action == SIG_DFL) {
            action = signal_get_default_action(action);
        }

        /*
        * Clear the pending bit.
        */
        signals->pending_signals &= ~(1 << number);

        if (action == SIG_IGN) {
            /*
            * Check for further signals that may actually need to be processed.
            */
            return signal_check();

        } else if (action == _SIG_CNT) {
            /*
             * Check for further signals that may actually need to be processed.
             */

            // TODO: wake the thread...

            return signal_check();

        } else if (action == _SIG_HND) {
            /*
            * Run the user specified handler. There are two options for the OS here,
            * either block the signal until it returns, or set it back to SIG_DFL. 
            * Setting it back to SIG_DFL is *way* easier for us.
            */

            size_t handler = signals->handler[number];
            signal_install_handler(number, SIG_DFL);
            return handler;

        } else if (action == _SIG_ABT) {
            // terminate the process right here, right now
            // TODO:

        } else if (action == _SIG_TRM) {
            // force the user program to call exit() (i.e. have a user-accessible kernel function that calls the exit syscall)
            // TODO:

        } else {
            panic("invalid signal type!");
        }
    }

    return 
}