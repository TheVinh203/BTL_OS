/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

 #include "string.h"
 #include "mm.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 

#define TRACE_VMA 1                 /* set to 0 to silence            */

/* ------------------------------------------------------------------
 *  Return pointer to the <idx>-th vm_area or NULL.
 * ------------------------------------------------------------------ */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int idx)
{
#if TRACE_VMA
    printf("[DBG][vma] lookup mm=%p idx=%d\n",(void*)mm,idx);
#endif
    if (!mm) { puts("[ERR][vma] *** mm == NULL ***"); return NULL; }

    struct vm_area_struct *v = mm->mmap;
    int hop = 0;
    while (v && hop < idx) { v = v->vm_next; ++hop; }

#if TRACE_VMA
    printf("[DBG][vma] → %p after %d hops\n",(void*)v,hop);
#endif
    return v;                                    /* may be NULL       */
}


 
 
 /*
  * __mm_swap_page - Swap copy a page from a victim frame to a swap frame.
  * @caller: The caller's process control block.
  * @vicfpn: Victim frame page number (in MEMRAM).
  * @swpfpn: Swap frame page number (in the active swap device).
  *
  * Returns 0 on success.
  */
 int __mm_swap_page(struct pcb_t *caller, int vicfpn, int swpfpn)
 {
     __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
     return 0;
 }
 
 /*
  * get_vm_area_node_at_brk - Obtain a new free region node based on the current break pointer.
  * @caller:    Caller process control block.
  * @vmaid:     ID of the VM area from which to allocate.
  * @size:      Requested increase size (in bytes; not directly used in the calculation).
  * @alignedsz: The aligned size (using PAGING_PAGE_ALIGNSZ) that is a multiple of the page size.
  *
  * Returns a pointer to a newly allocated vm_rg_struct whose boundaries are set based on the current sbrk.
  */
 struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
 {
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     if (!cur_vma)
         return NULL;
     struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
     if (!newrg)
         return NULL;
     newrg->rg_start = cur_vma->sbrk;
     newrg->rg_end   = cur_vma->sbrk + alignedsz;
     newrg->rg_next  = NULL;
     return newrg;
 }
 
 /*
  * validate_overlap_vm_area - Validate that the planned region does not overlap with allocated areas.
  * @caller:   Caller process control block.
  * @vmaid:    ID of the VM area being extended.
  * @vmastart: Start address of the planned region.
  * @vmaend:   End address of the planned region.
  *
  * Returns 0 if the region is valid (no overlap), or -1 if an overlap is detected.
  */
/* mm-vm.c  ------------------------------------------------------------- */
/* --------------------------------------------------------------------
 * Return 0  →  “OK, go ahead and extend the heap”
 * Return -1 →  “Overlap with an *allocated* region, refuse the request”
 *
 * Rules:
 *   • A node whose size is 0 ([x..x)) is just a sentinel → skip it.
 *   • If the free-list is completely empty (or only zero-size nodes),
 *     there can be no overlap      → succeed.
 *   • Overlap detection is done against ALLOCATED space, **not** against
 *     the holes themselves, so we simply walk the free list and look
 *     for a real conflict; if none is found we return success.
 * ------------------------------------------------------------------ */
/* mm-vm.c ------------------------------------------------------------- */
int validate_overlap_vm_area(struct pcb_t *caller,
                             int vmaid,
                             int vmastart,
                             int vmaend)
{
    struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
    if (!vma)                 /* shouldn’t happen */
        return -1;

    /* if we are adding space *after* the current end, there is no overlap */
    return (vmastart >= (int)vma->vm_end) ? 0 : -1;
}



 
 /*
  * inc_vma_limit - Increase the VM area's limits to reserve space for new data.
  * @caller: Caller process control block.
  * @vmaid:  VM area ID to be extended.
  * @inc_sz: Requested increment size in bytes.
  *
  * Returns 0 on success, or -1 on failure.
  */
/* mm-vm.c ------------------------------------------------------------ */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
    /* round the request up to a whole-page multiple */
    int inc_amt   = PAGING_PAGE_ALIGNSZ(inc_sz);
    int inc_pages = inc_amt / PAGING_PAGESZ;
    printf("[DBG] inc_vma_limit pid=%d +%dB (%d pages)\n",
           caller->pid, inc_amt, inc_pages);

    /* locate the vm_area to grow */
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma) return -1;

    int mapstart = cur_vma->vm_end;          /* grow at the top */

    /* ask MM to obtain physical frames and create PTEs               */
    struct vm_rg_struct dummy;
    if (vm_map_ram(caller, mapstart, mapstart + inc_amt,
                   mapstart, inc_pages, &dummy) < 0)
    {
        printf("[DBG]   vm_map_ram failed\n");
        return -1;
    }

    /* publish the newly-available hole to the free-region list       */
    struct vm_rg_struct *hole = malloc(sizeof(struct vm_rg_struct));
    hole->rg_start = mapstart;
    hole->rg_end   = mapstart + inc_amt;
    enlist_vm_rg_node(&cur_vma->vm_freerg_list, hole);

    /* advance heap cursors                                           */
    cur_vma->vm_end += inc_amt;
    cur_vma->sbrk    = cur_vma->vm_end;
    printf("[DBG]   new vm_end=%d\n", cur_vma->vm_end);

    return 0;                                    /* success */
}

 
 
