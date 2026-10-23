# Lab 1: Inter-Process Communication

A sender process and a receiver process exchange the lines of a text file through two interchangeable IPC mechanisms, **message passing** and **shared memory**, both built on the System V IPC API. The program measures the time spent on communication alone and compares the two mechanisms.

The original assignment specification is in [`OS_Lab1.pdf`](OS_Lab1.pdf).

## Build and run

```bash
make
```

Start the receiver and the sender in two separate terminals. The second argument selects the mechanism: `1` for message passing, `2` for shared memory.

```bash
./lab1 receiver 1
```

```bash
./lab1 sender 1 message.txt
```

The two processes may be started in either order.

## Design

### Common interface

Both mechanisms are hidden behind the same two wrappers, so the calling code does not depend on the transport:

```c
void send(message_t message, mailbox_t *mailbox_ptr);
void receive(message_t *message_ptr, mailbox_t *mailbox_ptr);
```

`mailbox_t` stores either a message-queue identifier or a pointer to the attached shared-memory segment in a union, selected by the mechanism flag.

| Mechanism | System V calls | Data path |
|---|---|---|
| Message passing | `msgget`, `msgsnd`, `msgrcv`, `msgctl` | Copied from the sender into a kernel-managed queue, then from the queue into the receiver |
| Shared memory | `shmget`, `shmat`, `shmdt`, `shmctl` | Written and read directly in a memory segment mapped into both processes |

### Synchronization

The specification requires the two processes to alternate strictly: the sender transmits one message and blocks until the receiver has consumed it. This is implemented as a single-slot producer–consumer protocol with two System V semaphores:

| Semaphore | Initial value | Meaning |
|---|---|---|
| `empty` | 1 | The slot may be written; the sender waits on it |
| `full` | 0 | The slot holds a message; the receiver waits on it |

```
Sender:   wait(empty) → send message → signal(full)
Receiver: wait(full)  → read message → signal(empty)
```

The same protocol is applied to both mechanisms. Shared memory requires it for correctness, because nothing else prevents the sender from overwriting a message that has not yet been read. Applying it to message passing as well enforces the alternation required by the specification and makes the two measurements directly comparable.

Two details keep start-up and shutdown well defined:

- **Initialization.** Both processes call `semget`, but only the receiver sets the initial values. If the sender starts first, it finds `empty` at 0 and blocks until the receiver initializes it, so it can never write before the receiver is ready.
- **Termination and cleanup.** After the last line, the sender transmits an `EOF` sentinel through the same protocol. The receiver stops on the sentinel and removes the message queue or shared-memory segment and the semaphore set, so no IPC objects are left behind.

### Time measurement

Only communication is timed, using `clock_gettime(CLOCK_MONOTONIC)`:

- Message passing: the `msgsnd` and `msgrcv` system calls.
- Shared memory: the copy into and out of the shared segment.

Time spent blocked on a semaphore and time spent printing are excluded, as the specification requires. Each `wait` completes before the timer starts, so the measurement does not depend on when, or in which order, the two processes are launched.

## Results

Transferring the 100 lines of `message.txt`:

| Mechanism | Sending time | Receiving time |
|---|---|---|
| Message passing | 0.000158 s | 0.000179 s |
| Shared memory | 0.000022 s | 0.000025 s |

Measured on an Intel Core Ultra 5 125U, Ubuntu 24.04 (Linux 7.0), GCC 13.3 with `-O3`. Timings vary between runs; the relative difference is consistent.

Shared memory is roughly seven times faster. Each message-passing operation is a system call that switches into the kernel and copies the payload between user space and a kernel buffer, so a message is copied twice. With shared memory, once the segment is attached, sending and receiving are ordinary memory copies in user space with no system call on the data path. The synchronization cost, which both mechanisms share, is excluded from the measurement.

## Limitations

- The end of transmission is signalled by the string `EOF`, so an input line consisting of exactly `EOF` would end the transfer early. A dedicated message type would avoid this.
- The message buffer is 1024 bytes, including the string terminator, so a line longer than 1023 bytes is split into several messages.
- All IPC objects use a fixed key, so only one sender–receiver pair can run at a time. If a process is interrupted, leftover objects can be removed with `ipcrm -a`.
