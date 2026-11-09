#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "../include/shell.h"

/*
 * Running commands: fork, exec, redirection, pipes and waiting.
 *
 * Each TODO [x-y] belongs to requirement x-y in README.md.
 * Lines marked (provided) are part of the template: keep them.
 */
//////////////////////////////////////////

/* TODO [1-1] Write exit_code(status): turn a waitpid() status into a $? value.
   WIFEXITED -> WEXITSTATUS(status), otherwise 128 + WTERMSIG(status). */
static int exit_code(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);
}

/*
 * Runs in the child right after fork() and never returns.
 * infile / outfile are pipe ends, or STDIN_FILENO / STDOUT_FILENO.
 */
void launch_process(process *p, int infile, int outfile, int background)
{
    /* TODO [2-2] If infile / outfile is a pipe end, dup2() it onto fd 0 / fd 1
       and close the original. */
    if (infile != STDIN_FILENO) {
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if (outfile != STDOUT_FILENO) {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    /* TODO [3-1] A background process whose stdin is still the terminal
       (no pipe, no "<") reads from /dev/null instead. */
    if (background && infile == STDIN_FILENO && !p->infile) {
        int fd = open("/dev/null", O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    /* TODO [2-1] Redirect p->infile onto fd 0 and p->outfile onto fd 1.
       If open() fails, perror(filename) and _exit(1). */
    if (p->infile) {
        int fd = open(p->infile, O_RDONLY);
        if (fd < 0) {
            perror(p->infile);
            _exit(1);
        }
        dup2(fd, 0);
        close(fd);
    }
    if (p->outfile) {
        int fd = open(p->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(p->outfile);
            _exit(1);
        }
        dup2(fd, 1);
        close(fd);
    }
    /* TODO [1-1] Run the program with execvp(). If it returns, it failed:
       print "<name>: command not found" to stderr and _exit(127). */
    execvp(p->argv[0], p->argv);
    fprintf(stderr, "%s: command not found\n", p->argv[0]);
    _exit(127);
}

/* Fork every process of the job, connecting neighbours with pipes. */
void launch_job(job *j)
{
    int infile = STDIN_FILENO;   /* (provided) */
    int outfile = STDOUT_FILENO; /* (provided) */

    for (process *p = j->first_process; p; p = p->next) { /* (provided) */
        /* TODO [2-2] If another process follows, create a pipe (int mypipe[2])
           and use its write end as outfile; otherwise outfile is stdout. */
        int mypipe[2];
        if (p->next) {
            pipe(mypipe);
            outfile = mypipe[1];
        } else {
            outfile = 1;
        }
        pid_t pid = -1; /* (provided) */
        /* TODO [1-1] fork() a child and store its pid in p->pid. */
        pid = fork();
        p->pid = pid;
        if (pid == 0) { /* (provided) the child */
            /* TODO [2-2] The pipe's read end belongs to the NEXT process:
               close it here. */
            if (p->next)
                close(mypipe[0]);
            launch_process(p, infile, outfile, j->bg); /* (provided) */
            _exit(1); /* (provided) launch_process() must not return */
        }

        /* TODO [2-2] In the shell, close the pipe ends it no longer needs, and
           make the pipe's read end the next process's input: store it in
           the local variable infile (not p->infile, the "<" file name). */
        if (infile != STDIN_FILENO)
            close(infile);
        if (outfile != STDOUT_FILENO)
            close(outfile);
        if (p->next)
            infile = mypipe[0];
    }

    /* TODO [3-1] Do not wait for a background job:
       call report_background_job(j), set last_status to 0 and return. */
    if (j->bg) {
        report_background_job(j);
        last_status = 0;
        return;
    }
    wait_for_job(j); /* (provided) */
    }

/* Wait until every process of a foreground job has finished. */
void wait_for_job(job *j)
{
    /* TODO [1-1] waitpid() every process of this job by its own pid and pass
       the status to mark_process_status(). Set last_status ($?) with
       exit_code() from the first process (a single command has only one;
       2-3 extends this to pipelines). */
    int status;
    for (process *p = j->first_process; p; p = p->next) { 
        waitpid(p->pid, &status,0);
        mark_process_status(p->pid, status);
        last_status = exit_code(status);
    }
    /* TODO [2-3] For a pipeline, $? is the status of the LAST process:
       overwrite the value set by [1-1] above. */
}

/* Reap every background process that has finished, without blocking. */
void update_bg_jobs(void)
{
    /* TODO [3-2] Loop waitpid(-1, &status, WNOHANG) and pass each pid to
       mark_process_status(); stop when it returns 0 or -1. */
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        mark_process_status(pid, status);
}
