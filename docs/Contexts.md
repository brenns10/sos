Contexts
========


Initial
-------

At startup, the kernel begins executing assembly in `startup.s`. This entire
file is designed to execute without using any stack, and interrupts should not
yet be triggered at this point. So there's not much to say about the context
this code executes within.

Just before executing `main()`, the stack pointer is set to the linker symbol
`stack_end`. This initial stack is 4KB, statically allocated. The kernel
initialization occurs in `main()` using this initial stack, up until the first
process is started.


Processes
---------

Each process has an associated kernel-mode stack, which is allocated by the page
allocator in `create_process()` or `create_kthread()`. This pointer is stored
within the `kstack` field of the process. Userspace processes manage their own
stack pointers without any kernel intervention.

At the end of initialization, the kernel uses `start_process()` to enter
user mode. The kernel may be re-entered by a SWI or IRQ, at which point the
`kstack` field of `current` is stuck into the `sp` register, and used for kernel
stack frames.


Storing contexts
----------------

When a `SWI` occurs, the handler loads the current process's kernel stack into
`sp`, and dumps the current context into the top of the kernel stack. An SWI may
be triggered *only* by USR mode. A kernel thread in SYS triggering a SWI is a
bug.

When an `IRQ` occurs, the handler dumps the current context directly into
`current->context`, and then loads the interrupt-mode stack and calls into a
handler. An IRQ may interrupt *any* processor mode (except another interrupt):
USR, SVC, or SYS.

User-mode processes may only go to sleep by invoking a system call. The system
call, executing in SVC mode on behalf of the process, uses the `block()`
function to store its context into `current->context`. In particular, it
populates the link register with the address of a code snippet which will
restore any context and resume execution. Then, it continues onward to
`schedule`.

Kernel threads work similarly, except they are in SYS mode, and there is no
system call which blocks on their behalf -- they simply call `block()` directly.

There are two race conditions involved with the user/kernel processes above.
First, is that an interrupt could clobber any context stored into
`current->context` when they are in the process of blocking. Second, is that
even if this was addressed, if the timer interrupt decided to reschedule a
kernel thread (since they are preemptive), they could again still clobber the
context at `current->context`.

The first can be resolved by storing interrupt contexts on the interrupt-mode
stack (duh). Context switching would involve swapping the stored context with
another.

The second can be resolved with the following:

- The timer interrupt must be able to check whether pre-emption is allowed.
  Right now, the policy is that preempting SVC mode is against the rules, but
  any other mode is fine. We should be able to do the following:
  - Use a global (well, per-cpu) flag to disable preempt
  - Use a per-process flag to disable preempt? Needs a user first...
  - Place functions within a special linker section which cannot be preempted.
- Marking the following as non-premptable:
  - `block()` function
  - `return-from-syscall()`
- Finally, disabling preempt in the `block()` function and re-enabling it?


Resuming contexts
-----------------

Thankfully, this is a fair bit easier than storing.  A context could be resumed
through the following:

1. The timer interrupt selects a new task and runs it.
2. The `block()` function invokes the scheduler and resumes a different task.
