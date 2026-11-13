#ifndef HCR_FPU_H
#define HCR_FPU_H

static inline unsigned int fpu_get_fcr31(void)
{
    unsigned int v;
    __asm__ volatile("cfc1 %0, $31" : "=r"(v));
    return v;
}

static inline void fpu_no_exceptions(void)
{
    // Mask exception enable bits (7-11) and clear cause bits (12-17)
    unsigned int v = fpu_get_fcr31() & ~0x0003FF80u;
    __asm__ volatile("ctc1 %0, $31" : : "r"(v));
}

#endif

