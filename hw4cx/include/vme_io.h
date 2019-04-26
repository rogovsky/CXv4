#ifndef __VME_IO_H
#define __VME_IO_H


// Common VME interface
enum
{
    VME_OP_NONE      = 0,
    VME_OP_READ      = 'r',
    VME_OP_WRITE     = 'w',

    VME_COND_NONZERO = '!',  // (data & mask) != 0
    VME_COND_EQUAL   = '=',  // (data & mask  == value
};


//!!!
int  vme_io_open   (uint32 base_addr, uint32 space_size, int am);


int  vme_io_a16wr8 (int zzz, int am, uint32 ofs, uint8   value);
int  vme_io_a16rd8 (int zzz, int am, uint32 ofs, uint8  *val_p);
int  vme_io_a16wr16(int zzz, int am, uint32 ofs, uint16  value);
int  vme_io_a16rd16(int zzz, int am, uint32 ofs, uint16 *val_p);
int  vme_io_a16wr32(int zzz, int am, uint32 ofs, uint32  value);
int  vme_io_a16rd32(int zzz, int am, uint32 ofs, uint32 *val_p);

int  vme_io_a24wr8 (int zzz, int am, uint32 ofs, uint8   value);
int  vme_io_a24rd8 (int zzz, int am, uint32 ofs, uint8  *val_p);
int  vme_io_a24wr16(int zzz, int am, uint32 ofs, uint16  value);
int  vme_io_a24rd16(int zzz, int am, uint32 ofs, uint16 *val_p);
int  vme_io_a24wr32(int zzz, int am, uint32 ofs, uint32  value);
int  vme_io_a24rd32(int zzz, int am, uint32 ofs, uint32 *val_p);

int  vme_io_a32wr8 (int zzz, int am, uint32 ofs, uint8   value);
int  vme_io_a32rd8 (int zzz, int am, uint32 ofs, uint8  *val_p);
int  vme_io_a32wr16(int zzz, int am, uint32 ofs, uint16  value);
int  vme_io_a32rd16(int zzz, int am, uint32 ofs, uint16 *val_p);
int  vme_io_a32wr32(int zzz, int am, uint32 ofs, uint32  value);
int  vme_io_a32rd32(int zzz, int am, uint32 ofs, uint32 *val_p);



#endif /* __VME_IO_H */
