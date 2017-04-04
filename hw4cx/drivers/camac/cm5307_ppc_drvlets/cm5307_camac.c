#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <camac/camac.h>

#include "cm5307_camac.h"


#define CAMAC_DEVICE "/dev/camac"


int init_cm5307_camac(void)
{
    return open(CAMAC_DEVICE, O_RDWR | O_EXCL);
}

int do_naf(int fd, int N, int A, int F, int *data)
{
  cam_header_t  rec;

    rec.n = N - 1;
    rec.a = A;
    rec.f = F;
    rec.buf = data;
    if (F < 16)
        read (fd, (char *)&rec, 1);
    else
        write(fd, (char *)&rec, 1);

    return rec.status;
}

int do_nafb(int fd, int N, int A, int F, int *data, int count)
{
  cam_header_t  rec;
  
    rec.n = N - 1;
    rec.a = A;
    rec.f = F;
    rec.buf = data;
    rec.status = -1 &~ (CAMAC_X | CAMAC_Q);
    if (F < 16)
        read (fd, (char *)&rec, count);
    else
        write(fd, (char *)&rec, count);

    return rec.status;
}

int camac_setlam(int fd, int N, int onoff)
{
    --N;
    ioctl(fd, onoff? CAM_IOCTL_LAMENABLE : CAM_IOCTL_LAMDISABLE, &N);
    return 0;
}
