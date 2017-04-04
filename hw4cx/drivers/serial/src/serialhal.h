#ifndef __SERIALHAL_H
#define __SERIALHAL_H


typedef void (*serialhal_errreport_t)(void *opaqueptr,
                                      const char *format, ...);

static int serialhal_opendev (int line, int baudrate,
                              serialhal_errreport_t errreport,
                              void *opaqueptr);


#endif /* __SERIALHAL_H */
