
#include <thread.h>
#include <kprintf.h>
#include <signal.h>
#include <spinlock.h>

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


/*
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
**
** START USERMODE EXPOSED SECTION
**
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/
void __attribute__((__section__("align_previous"))) DO_NOT_DELETE()
{

}

void __attribute__((__section__("userkernel"))) signal_default_terminate_handler(int sig)
{
	// TODO: call system call for 'exit'
}

void __attribute__((__section__("userkernel"))) signal_default_abort_handler(int sig)
{
	// TODO: call system call for 'abort'
}
/*
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
**
** END USERMODE EXPOSED SECTION
**
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

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
        case SIGCONT:
            return _SIG_IGN;
        default:
            return SIG_ERR;
    } 
}

struct signal_state signal_create_state() {
    struct signal_state* state = malloc(sizeof(struct signal_state));
    memset(state, 0, sizeof(struct signal_state));
    return state;
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

static size_t signal_get_handler(int number) {
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
        return (size_t) (void*) signal_default_abort_handler;

    } else if (action == _SIG_TRM) {
        return (size_t) (void*) signal_default_terminate_handler;

    } else {
        panic("invalid signal type!");
    }
    
    return 0;
}

/*
* If a signal is pending and unmasked, this returns the address of the signal handler of the
* highest priority. In this case, signal_check() should be called again to get any futher
* signals that need handling. Return 0 if there is no signal handler to run at this time.
*/
size_t signal_check() {
    struct thread* thread;
    struct signal_state* signals;

    /*
     * This should have been caught when the mask is being set, not here. Hence assert it.
     */
    assert(!(signals->masked_signals & ((1 << SIGSTOP) | (1 << SIGKILL))));

    /*
    * SIGCONT can still be handled by the user, so we must actually detect it specially.
    *
    * "This signal is special — it always makes the process continue if it is stopped, before the signal
    *  is delivered. The default behavior is to do nothing else. You cannot block this signal. You can
    *  set a handler, but SIGCONT always makes the process continue regardless. You can use a handler
    *  for SIGCONT to make a program do something special when it is stopped and continued...".
    */
    if (signals->pending_signals & (1 << SIGCONT)) {
        /*
        * "Sending a SIGCONT signal to a process causes any pending stop signals for that process to be discarded".
        * SIGCONT will only get cleared later on if it is not set to SIG_IGN.
        */
        signals->pending_signals &= ~((1 << SIGSTOP) | (1 << SIGCONT));

        if (thread->state == THREAD_STATE_STOPPED) {
            thread_unblock(thread);
        }

        /*
         * Do this now, so the handler runs on this turn. Otherwise, another signal might be chosen,
         * and as we already cleared the pending bit for SIGCONT, we wouldn't ever actually run the handler. 
         */
        return signal_get_handler(SIGCONT);
    }

    if (signals->pending_signals & (1 << SIGSTOP)) {
        /*
        * "Any pending SIGCONT signals for a process are discarded when it receives a stop signal."
        * SIGSTOP can never be masked, so no need to check for that or clear the bit (it can be handled
        * normally).
        */
        signals->pending_signals &= ~(1 << SIGCONT);
    }

    uint64_t pendingNotMasked = signals->pending_signals & (!signals->masked_signals);
    if (pendingNotMasked) {
        int number = 0;
        while (!(pendingNotMasked & 1)) {
            ++number;
            pendingNotMasked >>= 1;
        }

        return signal_get_handler(number);        
    }

    return 0;
}