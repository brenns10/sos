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

At the end of initialization, the kernel uses `context_switch()` to enter the
first user-mode process. The kernel may be re-entered by a SWI or IRQ, at which
point the `kstack` field of `current` is stuck into the `sp` register, and used
for kernel stack frames.


Storing contexts
----------------

When a `SWI` occurs, the handler loads the current process's kernel stack into
`sp`, and dumps the current context into the top of the kernel stack. An SWI may
be triggered *only* by USR mode. A kernel thread in SYS triggering a SWI is a
bug. The user-space context stored by SWI can only be restored by returning from
the SWI.

When an `IRQ` occurs, the handler dumps the current context directly onto
the IRQ mode stack, and then calls into a handler. An IRQ may interrupt *any*
processor mode (except another interrupt): USR, SVC, or SYS. Interrupt handlers
MUST directly return (they cannot use setctx/resctx, see below). When interrupt
handlers return, they resume the context currently located on the IRQ mode
stack. In some cases (the timer interrupt being the only case as of now), the
context could have been switched during handling.

User-mode processes may only go to sleep by invoking a system call. The system
call, executing in SVC mode on behalf of the process, uses the `setctx()`
function to store its context into `current->context`. This function behaves
similar to `setjmp()`, in that it returns 0 at first, but could be resumed with
a different return value later.

Kernel threads work similarly, except they are in SYS mode, and there is no
system call which blocks on their behalf -- they simply call `setctx()`
directly.

Resuming contexts
-----------------

Thankfully, this is a fair bit easier than storing.  A context could be resumed
through the following:

1. The timer interrupt selects a new task and copies the `struct ctx` into its
   stack. When the interrupt handler returns, that context will be resumed.
2. The `resctx()` function is used to resume a context. Typically, this only
   happens in calls to `schedule()`, or sometimes `context_switch()` directly.

Since the setctx/resctx functions can only handle CPU contexts (not page tables
or anything else), they are not typically used directly for scheduling. However,
they can be used within kernel threads or system calls as a substitute for
setjmp/longjmp.

Race conditions
---------------

Interrupts may be triggered during any mode. If the interrupt switches the
currently active task, there could be some consequences.  If the currently
active task was in the process of storing contexts and switching to a new active
task, then this context could be clobbered. Preemption is normally not allowed
for SVC mode, but it is allowed for SYS, and SYS mode can use the context
resumption primitives. The solution here is to add more policy to prevent
preemption. This work is started (.nopreempt linker section) but not completed
yet.
