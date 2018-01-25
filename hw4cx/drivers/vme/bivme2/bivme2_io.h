#ifndef __BIVME2_IO_H
#define __BIVME2_IO_H


#include "cx.h"


typedef void (*bivme2_irq_proc)(int devid, void *devptr,
                                int irq_n, int vector);


int  bivme2_io_open   (int devid, void *devptr,
                       uint32 base_addr, uint32 space_size, int am,
                       int irq_n, bivme2_irq_proc irq_proc);
int  bivme2_io_close  (int handle);

int  bivme2_io_a16wr8 (int handle, uint32 ofs, uint8   value);
int  bivme2_io_a16rd8 (int handle, uint32 ofs, uint8  *val_p);
int  bivme2_io_a16wr16(int handle, uint32 ofs, uint16  value);
int  bivme2_io_a16rd16(int handle, uint32 ofs, uint16 *val_p);
int  bivme2_io_a16wr32(int handle, uint32 ofs, uint32  value);
int  bivme2_io_a16rd32(int handle, uint32 ofs, uint32 *val_p);

int  bivme2_io_a24wr8 (int handle, uint32 ofs, uint8   value);
int  bivme2_io_a24rd8 (int handle, uint32 ofs, uint8  *val_p);
int  bivme2_io_a24wr16(int handle, uint32 ofs, uint16  value);
int  bivme2_io_a24rd16(int handle, uint32 ofs, uint16 *val_p);
int  bivme2_io_a24wr32(int handle, uint32 ofs, uint32  value);
int  bivme2_io_a24rd32(int handle, uint32 ofs, uint32 *val_p);

int  bivme2_io_a32wr8 (int handle, uint32 ofs, uint8   value);
int  bivme2_io_a32rd8 (int handle, uint32 ofs, uint8  *val_p);
int  bivme2_io_a32wr16(int handle, uint32 ofs, uint16  value);
int  bivme2_io_a32rd16(int handle, uint32 ofs, uint16 *val_p);
int  bivme2_io_a32wr32(int handle, uint32 ofs, uint32  value);
int  bivme2_io_a32rd32(int handle, uint32 ofs, uint32 *val_p);


#endif /* __BIVME2_IO_H */
