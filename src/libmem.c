/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
 static void dump_freerg(struct mm_struct *mm,const char *tag)
{
    struct vm_rg_struct *rg = mm->mmap->vm_freerg_list;
    printf("[DBG][freerg] %s ",tag);
    while (rg){ printf("[%ld..%ld)->",rg->rg_start,rg->rg_end); rg=rg->rg_next; }
    puts("NULL");
}
 /*enlist_vm_freerg_list - add new rg to freerg_list
  *@mm: memory region
  *@rg_elmt: new region
  *
  */
 int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
 {
   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
 
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
     return -1;
 
   if (rg_node != NULL)
     rg_elmt->rg_next = rg_node;
 
   /* Enlist the new region */
   mm->mmap->vm_freerg_list = rg_elmt;
 
   return 0;
 }
 
 /*get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  *
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
   if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
     return NULL;
 
   return &mm->symrgtbl[rgid];
 }
 
 /*__alloc - allocate a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *@alloc_addr: address of allocated memory region
  *
  */
/* libmem.c --------------------------------------------------- */
int __alloc(struct pcb_t *caller,int vmaid,int rgid,
            int size,int *alloc_addr)
{
    printf("[DBG] __alloc pid=%d vma=%d rgid=%d size=%d\n",
           caller->pid, vmaid, rgid, size);

    struct vm_rg_struct rgnode;

    /* 1. try to reuse a free hole */
    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
      caller->mm->symrgtbl[rgid] = rgnode;
      *alloc_addr = rgnode.rg_start;
      dump_freerg(caller->mm,"after alloc");          /* DBG */
      return 0;
  }

    /* 2. grow the heap once */
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    if (inc_vma_limit(caller, vmaid, inc_sz) == 0 &&
        get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
        caller->mm->symrgtbl[rgid] = rgnode;
        *alloc_addr = rgnode.rg_start;
        return 0;
    }

    /* 3. out of memory */
    printf("[DBG]   allocation failed\n");
    return -1;
}


 
 /*__free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid)
 {
     if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
         return -1;
 
     /* take a snapshot of the region we are about to free */
     struct vm_rg_struct *old = &caller->mm->symrgtbl[rgid];
     if (old->rg_start == old->rg_end)
         return 0;                           /* already freed */
 
     struct vm_rg_struct *clone = malloc(sizeof(struct vm_rg_struct));
     clone->rg_start = old->rg_start;
     clone->rg_end   = old->rg_end;
     clone->rg_next  = NULL;
 
     /* reset symbol-table entry */
     old->rg_start = old->rg_end = 0;
 
     /* put the clone on the free-list */
     enlist_vm_freerg_list(caller->mm, clone);
     printf("[DBG] __free pid=%d rgid=%d  [%ld..%ld)\n",
            caller->pid, rgid, clone->rg_start, clone->rg_end);
     dump_freerg(caller->mm,"after free");               /* DBG */
     return 0;
 }
 
 
 
 /*liballoc - PAGING-based allocate a region memory
  *@proc:  Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
   /* TODO Implement allocation on vm area 0 */
   int addr = 0;
 
   /* By default using vmaid = 0 */
   return __alloc(proc, 0, reg_index, size, &addr);
 }
 
 /*libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 
 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
   /* TODO Implement free region */
 
   /* By default using vmaid = 0 */
   return __free(proc, 0, reg_index);
 }
 
 /*pg_getpage - get the page in ram
  *@mm: memory region
  *@pagenum: PGN
  *@framenum: return FPN
  *@caller: caller
  *
  */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn,
               struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];
  printf("[DBG] pg_getpage pid=%d pgn=%d\n", caller->pid, pgn);

  if (!PAGING_PAGE_PRESENT(pte)) {
    printf("[DBG]   page fault!\n");

    int vicpgn, swpfpn;
    find_victim_page(caller->mm, &vicpgn);
    uint32_t vicpte = mm->pgd[vicpgn];
    int vicfpn = PAGING_FPN(vicpte);

    MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
    printf("[DBG]   swap victim pgn=%d (fpn=%d) ↔ swpfpn=%d\n",
           vicpgn, vicfpn, swpfpn);

    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP; regs.a2 = vicpgn; regs.a3 = swpfpn;
    syscall(caller, 17, &regs);

    int tgtfpn = PAGING_PTE_SWP(pte);
    regs.a1 = SYSMEM_SWP_OP; regs.a2 = pgn;
    regs.a3 = vicfpn;
    syscall(caller, 17, &regs);

    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);
    mm->pgd[pgn] = 0;
    pte_set_fpn(&mm->pgd[pgn], vicfpn);
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]);
  printf("[DBG]   hit fpn=%d\n", *fpn);
  return 0;
}

 
 /*pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
/* -------------------------------------------------------------------
 * pg_getval – read one byte from a virtual address
 * ------------------------------------------------------------------- */
int pg_getval(struct mm_struct *mm,
              int              vaddr,
              BYTE            *data,
              struct pcb_t    *caller)
{
    int pgn = PAGING_PGN(vaddr);          /* virtual-page number        */
    int off = PAGING_OFFST(vaddr);        /* byte offset in the page    */
    int fpn;                              /* (out) frame page number    */

    /* make sure the page is resident – swap in if necessary          */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;                        /* invalid access             */

    /* translate to real physical address                             */
    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    /* *** DIRECTLY access RAM – we already know where the byte is *** */
    MEMPHY_read(caller->mram, phyaddr, data);   /* prints [DBG] MEM[R] */

    return 0;
}

 
 /*pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
/* ---------------------------------------------------------------
 *  Write one BYTE to virtual address <vaddr>
 * --------------------------------------------------------------*/
int pg_setval(struct mm_struct *mm,
              int              vaddr,
              BYTE             value,
              struct pcb_t    *caller)
{
    if (vaddr < 0) return -1;                       /* paranoia */

    int pgn = PAGING_PGN(vaddr);
    int fpn;

    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;

    int off     = PAGING_OFFST(vaddr);
    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    printf("[DBG] pg_setval pid=%d vaddr=%d (pgn=%d,off=%d) "
           "→ fpn=%d phy=%d val=%d\n",
           caller->pid, vaddr, pgn, off, fpn, phyaddr, value);

    struct sc_regs regs = {
        .a1 = SYSMEM_IO_WRITE,
        .a2 = phyaddr,
        .a3 = (uint32_t)value
    };
    syscall(caller, 17, &regs);
    return 0;
}

 
 /*__read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 static inline int region_valid(struct vm_rg_struct *rg, int off)
 {
     return rg &&                      /* symbol id exists          */
            rg->rg_start < rg->rg_end  /* region was allocated      */
            && off < (rg->rg_end - rg->rg_start);
 }
 
 /* ----------------------------------------------------------------- */
/* ------------------------------------------------------------------ */
/*  libmem.c – excerpts with extra logging                             */
/* ------------------------------------------------------------------ */

/* ---------- helpers ------------------------------------------------*/
#define TRACE_RW 1
static void dump_rg(const char *tag,struct vm_rg_struct *rg,int off,int ok){
#if TRACE_RW
    printf("[DBG][%s] rg=%p [%ld..%ld) off=%d %s\n",
           tag,(void*)rg,
           rg?rg->rg_start:-1, rg?rg->rg_end:-1,
           off, ok?"OK":"BAD");
#endif
    }
/* ----------  __read  ---------------------------------------------- */
int __read(struct pcb_t *p, int vmaid, int rgid, int off, BYTE *dst)
{
    struct vm_rg_struct *rg = get_symrg_byid(p->mm, rgid);
    struct vm_area_struct *v = get_vma_by_num(p->mm, vmaid);

    int ok = rg && v && (rg->rg_start < rg->rg_end)
             && off < (rg->rg_end - rg->rg_start);

    dump_rg("READ", rg, off, ok);

    if (!ok) return -1;

    return pg_getval(p->mm, rg->rg_start + off, dst, p);
}

/* ----------  __write  --------------------------------------------- */


 
/* ---- libmem.c ---------------------------------------------- */


/* top-level API used by the CPU ------------------------------ */
int libread(struct pcb_t *proc,
            uint32_t   region_id,
            uint32_t   offset,
            uint32_t  *dst)
{
    BYTE val = 0;
    int  rc  = __read(proc, 0, region_id, offset, &val);

    if (rc == 0) {
        *dst = (uint32_t)val;
        printf("[DBG] libread pid=%d  rgn=%u off=%u  -> %u\n",
               proc->pid, region_id, offset, val);
    } else {
        printf("[DBG] libread ERROR  pid=%d  rgn=%u off=%u  rc=%d\n",
               proc->pid, region_id, offset, rc);
    }
    return rc;
}



 
 /*__write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __write(struct pcb_t *p, int vmaid, int rgid, int off, BYTE val)
 {
     struct vm_rg_struct *rg = get_symrg_byid(p->mm, rgid);
     struct vm_area_struct *v = get_vma_by_num(p->mm, vmaid);
 
     int ok = rg && v && (rg->rg_start < rg->rg_end)
              && off < (rg->rg_end - rg->rg_start);
 
     dump_rg("WRITE", rg, off, ok);
 
     if (!ok) return -1;
 
     return pg_setval(p->mm, rg->rg_start + off, val, p);
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   // Process executing the instruction
     BYTE data,            // Data to be wrttien into memory
     uint32_t destination, // Index of destination register
     uint32_t offset)
 {
 #ifdef IODUMP
   printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); //print max TBL
 #endif
   MEMPHY_dump(proc->mram);
 #endif
 
   return __write(proc, 0, destination, offset, data);
 }
 
 /*free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  */
 int free_pcb_memph(struct pcb_t *caller)
 {
   int pagenum, fpn;
   uint32_t pte;
 
 
   for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte= caller->mm->pgd[pagenum];
 
     if (!PAGING_PAGE_PRESENT(pte))
     {
       fpn = PAGING_PTE_FPN(pte);
       MEMPHY_put_freefp(caller->mram, fpn);
     } else {
       fpn = PAGING_PTE_SWP(pte);
       MEMPHY_put_freefp(caller->active_mswp, fpn);    
     }
   }
 
   return 0;
 }
 
 
 /*find_victim_page - find victim page
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
   struct pgn_t *pg = mm->fifo_pgn;
 
   /* TODO: Implement the theorical mechanism to find the victim page */
   if(pg == NULL){
     return -1;
   }
   *retpgn = pg->pgn;
   mm->fifo_pgn = pg->pg_next;
   free(pg);
 
   return 0;
 }
 
 /*get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  *
  */
int get_free_vmrg_area(struct pcb_t *caller,
                       int vmaid,
                       int size,
                       struct vm_rg_struct *newrg)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma || !newrg) return -1;

    struct vm_rg_struct *prev = NULL;
    struct vm_rg_struct *rg   = cur_vma->vm_freerg_list;

    newrg->rg_start = newrg->rg_end = -1;

    while (rg) {
        int hole_size = rg->rg_end - rg->rg_start;
        if (hole_size >= size) {
            /* carve the space */
            newrg->rg_start = rg->rg_start;
            newrg->rg_end   = rg->rg_start + size;
            rg->rg_start   += size;

            /* if the hole is now empty just unlink it (DON’T free) */
            if (rg->rg_start >= rg->rg_end) {
                if (prev) prev->rg_next = rg->rg_next;
                else      cur_vma->vm_freerg_list = rg->rg_next;
                /* keep the node for reuse or free here if you allocated it
                   with malloc in inc_vma_limit – never free symrgtbl nodes */
            }
            printf("[DBG]   take [%ld..%ld) remain=%d\n",
                   newrg->rg_start, newrg->rg_end,
                   rg->rg_end - rg->rg_start);
            return 0;
        }
        prev = rg;
        rg   = rg->rg_next;
    }
    return -1;          /* no hole fits */
}


 
 //#endif
 
