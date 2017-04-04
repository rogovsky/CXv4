#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "misc_macros.h"
#include "ppf4td.h"


static void ProcessFile(const char *argv0, const char *filename)
{
  ppf4td_ctx_t  ctx;
  int           r;
  int           ch;
  int           prev_ch = '\n';

    r = ppf4td_open(&ctx, "plaintext", filename);
    if (r != 0)
    {
        fprintf(stderr, "%s: failed to open \"%s\", %s\n",
                argv0, filename, strerror(errno));
        return;
    }
    while ((r = ppf4td_nextc(&ctx, &ch)) > 0)
    {
        if (prev_ch == '\r'  ||  prev_ch == '\n')
            fprintf(stdout, "%s:%d:",
                    ppf4td_cur_ref(&ctx), ppf4td_cur_line(&ctx));
        fputc(ch, stdout);
        prev_ch = ch;
    }
    if (r < 0) exit(1);
    ppf4td_close(&ctx);
}


int main(int argc, char *argv[])
{
  int  x;

    if (argc > 1)
    {
        for (x = 1;  x < argc;  x++)
            ProcessFile(argv[0], argv[x]);
    }
    else
    {
        ProcessFile(argv[0], NULL);
    }

    return 0;
}
