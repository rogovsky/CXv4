#ifndef _LIBVMEDIRECT_H
#define _LIBVMEDIRECT_H

#define REGS_BASE 0xfff40000
#define REGS_SIZE 0x10000
#define A16_BASE 0xffff0000
#define A16_SIZE 0x10000
#define A24_BASE 0xf0000000
#define A24_SIZE 0x1000000
#define A32_BASE 0x80000000
#define A32_SIZE 0x4000000

#define VMEIF_REG0_AM_MASK 		0x1f
#define VMEIF_REG0_USERAM 		0x10
#define VMEIF_REG0_BUSREQ_MASK 		0x60
#define VMEIF_REG0_BUSREQ_SHIFTS 	5
#define VMEIF_REG0_SYSTEM 		0x80
#define VMEIF_REG0_RRSARB 		0x100
#define VMEIF_REG0_OVERLAY_MASK 	0x7e00
#define VMEIF_REG0_OVERLAY_SHIFTS 	9
#define VMEIF_REG0_SYSFAIL		0x8000

#define VMEIF_REG1_IRQ_MASK		0x7f
#define VMEIF_REG1_ACFAIL		0x80

#define VMEIF_REG2_SYSFAIL		0x08
#define VMEIF_REG2_ACFAIL		0x10
#define VMEIF_REG2_BERR 		0x20
#define VMEIF_REG2_IRQL_MASK		0x07


int libvme_init(void);
void libvme_close(void);
void libvme_debug_info(void);
int libvme_ctl(int mode, int *value);
void libvme_rtc_start(void);
void libvme_rtc_stop(void);
void libvme_rtc_gettime(int *seconds, int *minutes, int *hours);
void libvme_rtc_settime(int seconds, int minutes, int hours);
void libvme_rtc_getdate(int *date, int *mounth, int *years);
void libvme_rtc_setdate(int date, int mounth, int years);

extern volatile int *__libvme_shm_ptr;
extern volatile unsigned char *__libvme_a16;
extern volatile unsigned char *__libvme_a24;
extern volatile unsigned char *__libvme_a32;
extern volatile unsigned int *__libvme_regs;
extern volatile unsigned int *__libvme_reg2;

extern int __libvme_mutual_access;

static __inline__ void atomic_inc(volatile int *v)
{
        int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2		# atomic_inc\n\
	addic	%0,%0,1\n"
"	stwcx.	%0,0,%2\n\
	bne-	1b"
	: "=&r" (t), "=m" (*v)
	: "r" (v), "m" (*v)
	: "cc");
}

static __inline__ int atomic_dec_if_positive(volatile int *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1		# atomic_dec_if_positive\n\
	addic.	%0,%0,-1\n\
	blt-	2f\n"
"	stwcx.	%0,0,%1\n\
	bne-	1b"
	"\n\
2:"	: "=&r" (t)
	: "r" (v)
	: "cc", "memory");

	return t;
}

#include <sched.h>
static __inline__ int
libvme_read_a16_byte(unsigned int bus_addr, unsigned char *byte) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *byte = *(unsigned char *)(__libvme_a16 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a24_byte(unsigned int bus_addr, unsigned char *byte) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *byte = *(unsigned char *)(__libvme_a24 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a32_byte(unsigned int bus_addr, unsigned char *byte) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *byte = *(unsigned char *)(__libvme_a32 + offset);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a16_word(unsigned int bus_addr, unsigned short *word) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *word = *(unsigned short *)(__libvme_a16 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a24_word(unsigned int bus_addr, unsigned short *word) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *word = *(unsigned short *)(__libvme_a24 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a32_word(unsigned int bus_addr, unsigned short *word) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *word = *(unsigned short *)(__libvme_a32 + offset);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a16_dword(unsigned int bus_addr, unsigned int *dword) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *dword = *(unsigned int *)(__libvme_a16 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a24_dword(unsigned int bus_addr, unsigned int *dword) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *dword = *(unsigned int *)(__libvme_a24 + bus_addr);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_read_a32_dword(unsigned int bus_addr, unsigned int *dword) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *dword = *(unsigned int *)(__libvme_a32 + offset);
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}

static __inline__ int
libvme_write_a16_byte(unsigned int bus_addr, unsigned char byte) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned char *)(__libvme_a16 + bus_addr) = byte;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a24_byte(unsigned int bus_addr, unsigned char byte) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned char *)(__libvme_a24 + bus_addr) = byte;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a32_byte(unsigned int bus_addr, unsigned char byte) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *(unsigned char *)(__libvme_a32 + offset) = byte;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a16_word(unsigned int bus_addr, unsigned short word) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned short *)(__libvme_a16 + bus_addr) = word;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a24_word(unsigned int bus_addr, unsigned short word) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned short *)(__libvme_a24 + bus_addr) = word;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a32_word(unsigned int bus_addr, unsigned short word) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *(unsigned short *)(__libvme_a32 + offset) = word;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a16_dword(unsigned int bus_addr, unsigned int dword)
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned int *)(__libvme_a16 + bus_addr) = dword;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a24_dword(unsigned int bus_addr, unsigned int dword) 
{
    register int status;

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }
    *(unsigned int *)(__libvme_a24 + bus_addr) = dword;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}
static __inline__ int
libvme_write_a32_dword(unsigned int bus_addr, unsigned int dword) 
{
    register int status, reg0, overlay, offset;

    overlay = (bus_addr >> 26) & 0x3f;	/* 64 pages */
    offset = bus_addr & 0x3ffffff;	/* 64 M page */

    while (atomic_dec_if_positive(&__libvme_shm_ptr[0]) == -1) {
	__libvme_mutual_access++;
	sched_yield();
    }

    reg0 = __libvme_regs[0];
    reg0 &= ~VMEIF_REG0_OVERLAY_MASK;
    reg0 |= overlay << VMEIF_REG0_OVERLAY_SHIFTS;
    __libvme_regs[0] = reg0;
    
    *(unsigned int *)(__libvme_a32 + offset) = dword;
    status = *__libvme_reg2;
    atomic_inc(&__libvme_shm_ptr[0]);
    if (status & VMEIF_REG2_BERR) {*__libvme_reg2 = VMEIF_REG2_BERR; return -1; }
    return 0;
}

#define VME_AM_W	0x0001
#define VME_AM_R	0x0002
#define VME_BUSL_W	0x0004
#define VME_BUSL_R	0x0008
#define VME_SYSTEM_W	0x0010
#define VME_SYSTEM_R	0x0020
#define VME_ARB_W	0x0040
#define VME_ARB_R	0x0080
#define VME_SYSFAIL_W	0x0100
#define VME_SYSFAIL_R	0x0200
#define VME_ACFAIL_R	0x0400

#endif /* _LIBVMEDIRECT_H */
