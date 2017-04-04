#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ppf4td.h"
#include "ppf4td_pipe.h"


typedef struct
{
    FILE  *fp;
    pid_t  pid;
} pipe_privrec_t;

int ppf4td_pipe_open (ppf4td_ctx_t *ctx, char *const cmdline[])
{
  pipe_privrec_t *me;
  int             fds[2];
  int             r;
  int             saved_errno;

    if (pipe(fds) != 0) return -1;

    me = malloc(sizeof(me));
    if (me == NULL) return -1;

    r = fork();
    /* Failed? */
    if (r < 0) goto CLEANUP;
    /* Is it a child? */
    if (r == 0)
    {
        close(fds[0]);
        if (dup2(fds[1], 1) < 0) exit(1);
        if (fds[1] > 2) close(fds[1]);

        execv(cmdline[0], cmdline);

        /* Here only if failed to exec() */
        fprintf(stderr, "failed to exec(\"%s\")\n", cmdline[0]);
        exit(1);
    }
    /* Still in a parent */
    close(fds[1]);
    me->fp  = fdopen(fds[0], "r");
    me->pid = r;
    ctx->imp_privptr = me;

    return 0;

 CLEANUP:
    saved_errno = errno;
    close(fds[0]);
    close(fds[1]);
    free(me);
    errno = saved_errno;
    return -1;
}

int ppf4td_pipe_close(ppf4td_ctx_t *ctx)
{
  pipe_privrec_t *me = ctx->imp_privptr;
  int             status;

    fclose(me->fp);
    waitpid(me->pid, &status, 0);

    free(me);
    return 0;
}

static int result_of_xxxxc(ppf4td_ctx_t *ctx, int ch)
{
  pipe_privrec_t *me = ctx->imp_privptr;
  int             status;

    if (ferror(me->fp)) return -1;
    if (ch == EOF)
    {
#if 0
        int r;
        r = waitpid(me->pid, &status, 0);
        fprintf(stderr, "WAITPID: r=%d WIFEXITED=%d WEXITSTATUS=%d\n",
                r, WIFEXITED(status), WEXITSTATUS(status));
#else
        if (waitpid(me->pid, &status, 0) <= 0  ||
            !WIFEXITED(status)                 ||
            WEXITSTATUS(status) != 0)
        return                 -1;
#endif
        return                  0;
    }
    return                     +1;
}

int ppf4td_pipe_peekc(ppf4td_ctx_t *ctx, int *ch_p)
{
  pipe_privrec_t *me = ctx->imp_privptr;

    *ch_p = fgetc(me->fp);
    if (*ch_p != EOF) ungetc(*ch_p, me->fp);
    return result_of_xxxxc(ctx, *ch_p);
}

int ppf4td_pipe_nextc(ppf4td_ctx_t *ctx, int *ch_p)
{
  pipe_privrec_t *me = ctx->imp_privptr;

    *ch_p = fgetc(me->fp);
    return result_of_xxxxc(ctx, *ch_p);
}
