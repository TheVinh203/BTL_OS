/* ---------------  Simple “CPU” execution loop  ------------------- */
#include "cpu.h"
#include "mem.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdio.h>

#define MAX_GPR  NUM_REGS
static inline int gpr_ok(unsigned r) { return r < MAX_GPR; }

/* ---------- helpers when *no* paging is compiled in -------------- */
static int  calc       (struct pcb_t *p)                       { return ((unsigned)p & 0UL); }
static int  alloc_plain(struct pcb_t *p,uint32_t sz,uint32_t r){ addr_t a=alloc_mem(sz,p);if(!a)return 1;p->regs[r]=a;return 0;}
static int  free_plain (struct pcb_t *p,uint32_t r)            { return free_mem(p->regs[r],p); }
static int  read_plain (struct pcb_t *p,uint32_t s,uint32_t o,uint32_t d){BYTE v;if(!read_mem(p->regs[s]+o,p,&v))return 1;p->regs[d]=v;return 0;}
static int  write_plain(struct pcb_t *p,BYTE v,uint32_t d,uint32_t o)   { return write_mem(p->regs[d]+o,p,v); }

/* ================================================================= */
int run(struct pcb_t *proc)
{
    if (proc->pc >= proc->code->size) return 1;          /* finished */

    struct inst_t ins = proc->code->text[proc->pc++];

    /* DBG-BEGIN : one-liner that shows *every* instruction executed  */
    printf("[CPU] pid=%d pc=%04u  opcode=%d  mm=%p\n",
           proc->pid, proc->pc-1, ins.opcode, (void*)proc->mm);
    /* DBG-END   ---------------------------------------------------- */

    int rc = 1;                                         /* default = stop */

    switch (ins.opcode)
    {
/* ---- CALC ------------------------------------------------------- */
    case CALC:   rc = calc(proc);                        break;

/* ---- ALLOC ------------------------------------------------------ */
    case ALLOC:
#ifdef MM_PAGING
        rc = liballoc(proc, ins.arg_0, ins.arg_1);
#else
        if (gpr_ok(ins.arg_1)) rc = alloc_plain(proc, ins.arg_0, ins.arg_1);
#endif
        break;

/* ---- FREE ------------------------------------------------------- */
    case FREE:
#ifdef MM_PAGING
        rc = libfree(proc, ins.arg_0);
#else
        if (gpr_ok(ins.arg_0)) rc = free_plain(proc, ins.arg_0);
#endif
        break;

/* ---- READ ------------------------------------------------------- */
    case READ:
#ifdef MM_PAGING
        if (!gpr_ok(ins.arg_2)) { puts("[CPU] READ bad dst"); break; }

        printf("[CPU] READ  r%u + %u -> r%u ?\n",
               ins.arg_0, ins.arg_1, ins.arg_2);

        rc = libread(proc,
                     ins.arg_0,                /* region id   */
                     ins.arg_1,                /* offset      */
                     &proc->regs[ins.arg_2]);  /* dest reg    */

        if (rc==0)
            printf("[CPU]   → %u (stored in r%u)\n",
                   proc->regs[ins.arg_2], ins.arg_2);
#else
        if (gpr_ok(ins.arg_0)&&gpr_ok(ins.arg_2))
            rc = read_plain(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
        break;

/* ---- WRITE ------------------------------------------------------ */
    case WRITE:
#ifdef MM_PAGING
        rc = libwrite(proc,(BYTE)ins.arg_0,ins.arg_1,ins.arg_2);
#else
        if (gpr_ok(ins.arg_1))
            rc = write_plain(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
        break;

/* ---- SYSCALL ---------------------------------------------------- */
    case SYSCALL:
        rc = libsyscall(proc, ins.arg_0, ins.arg_1, ins.arg_2, ins.arg_3);
        break;

/* ---- unknown ---------------------------------------------------- */
    default:
        printf("[CPU] unknown opcode %d – halted\n", ins.opcode);
    }

    return rc;                                         /* 0 keep running */
}
