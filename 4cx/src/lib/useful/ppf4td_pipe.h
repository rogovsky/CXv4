#ifndef __PPF4TD_PIPE_H
#define __PPF4TD_PIPE_H


int ppf4td_pipe_open (ppf4td_ctx_t *ctx, char *const cmdline[]);
int ppf4td_pipe_close(ppf4td_ctx_t *ctx);
int ppf4td_pipe_peekc(ppf4td_ctx_t *ctx, int *ch_p);
int ppf4td_pipe_nextc(ppf4td_ctx_t *ctx, int *ch_p);



#endif /* __PPF4TD_PIPE_H */
