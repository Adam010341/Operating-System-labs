# Operating System Labs

Laboratory assignments for the Operating Systems course at the Department of Computer Science and Information Engineering, National Cheng Kung University (Fall 2026). Each lab is a self-contained C project that explores an operating-system mechanism through the Linux system-call interface.

## Labs

| Lab | Topic | Key concepts |
|---|---|---|
| [Lab 1](lab1/) | Inter-process communication | System V message queues, shared memory, semaphores, producer–consumer synchronization, performance measurement |

## Environment

- Language: C (GCC 13, `-O3 -Wall`)
- Platform: Linux (tested on Ubuntu 24.04, kernel 7.0)

Each lab directory contains its own `Makefile` and a README describing the design, usage, and results.
