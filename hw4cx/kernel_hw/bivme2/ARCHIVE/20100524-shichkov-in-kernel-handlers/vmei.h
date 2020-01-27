#ifndef VMEI_H
#define VMEI_H

typedef int (*fInterruptHandler)(int, void*, struct pt_regs*, int, void*);

typedef struct vme_device_
{
    fInterruptHandler handler;
    void* device;
} vme_device_t;

int insert_interrupt_handler(int irq, int irq_vect, fInterruptHandler irq_handler, void* device);
int remove_interrupt_handler(int irq, int irq_vect, void *device);

#endif
