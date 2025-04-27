//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

 #include "mm.h"
 #include <stdlib.h>
 #include <stdio.h>
 
 /* 
  * init_pte - Initialize PTE entry
  */
 int init_pte(uint32_t *pte,
              int pre,    // present
              int fpn,    // FPN
              int drt,    // dirty
              int swp,    // swap
              int swptyp, // swap type
              int swpoff) // swap offset
 {
   if (pre != 0) {
     if (swp == 0) { // Non swap ~ page online
       if (fpn == 0)
         return -1; // Invalid setting
 
       /* Valid setting with FPN */
       SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
       CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
       CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
 
       SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
     } else { // page swapped
       SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
       SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
       CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
 
       SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
       SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
     }
   }
 
   return 0;
 }
 
 /* 
  * pte_set_swap - Set PTE entry for swapped page
  * @pte    : target page table entry (PTE)
  * @swptyp : swap type
  * @swpoff : swap offset
  */
 int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
 {
   SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
   SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
 
   SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
   SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
 
   return 0;
 }
 
 /* 
  * pte_set_fpn - Set PTE entry for on-line page
  * @pte   : target page table entry (PTE)
  * @fpn   : frame page number (FPN)
  */
 int pte_set_fpn(uint32_t *pte, int fpn)
 {
   SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
   CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
 
   SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
 
   return 0;
 }
 
 /* 
  * vmap_page_range - map a range of page at aligned address
  */
/* mm.c ------------------------------------------------------- */
int vmap_page_range(struct pcb_t *caller,
                    int addr, int pgnum,
                    struct framephy_struct *frames,
                    struct vm_rg_struct *ret_rg)
{
    if (pgnum <= 0) return -3000;

    int pgn  = PAGING_PGN(addr);
    int pgit = 0;
    ret_rg->rg_start = addr;

    /* walk while there are still frames AND we haven’t mapped pgnum pages */
    while (frames && pgit < pgnum) {
        printf("[DBG]   map pgn=%d → fpn=%d\n", pgn+pgit, frames->fpn);
        caller->mm->pgd[pgn+pgit] = 0;
        pte_set_fpn(&caller->mm->pgd[pgn+pgit], frames->fpn);
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn+pgit);

        frames = frames->fp_next;
        ++pgit;
    }

    ret_rg->rg_end = addr + pgit * PAGING_PAGESZ;
    return 0;
}


 
 /* 
  * alloc_pages_range - allocate req_pgnum of frames in RAM
  */
int alloc_pages_range(struct pcb_t *caller, int req_pgnum,
                      struct framephy_struct **frm_lst)
{
  if (req_pgnum <= 0) return 0;
  printf("[DBG] alloc_pages_range pid=%d need=%d pages\n",
         caller->pid, req_pgnum);

  int pgit = 0, fpn;
  struct framephy_struct *head = NULL, *tail = NULL;

  while (pgit < req_pgnum) {
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
      printf("[DBG]   grant fpn=%d\n", fpn);
      struct framephy_struct *node = malloc(sizeof(*node));
      node->fpn = fpn; node->fp_next = NULL;
      if (!head) head = tail = node; else { tail->fp_next = node; tail = node; }
    } else {
      printf("[DBG]   out of frames!\n");
      /* rollback already grabbed frames */
      while (head) {
        MEMPHY_put_freefp(caller->mram, head->fpn);
        struct framephy_struct *tmp = head; head = head->fp_next; free(tmp);
      }
      return -3000;
    }
    ++pgit;
  }
  *frm_lst = head;
  return 0;
}

 /* 
  * vm_map_ram - map all VM areas to RAM storage
  */
int vm_map_ram(struct pcb_t *caller, int astart, int aend,
               int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  printf("[DBG] vm_map_ram pid=%d pages=%d start=%d\n",
         caller->pid, incpgnum, mapstart);

  struct framephy_struct *frm_lst = NULL;
  int ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc == -3000) {
    printf("[DBG]   OOM – no free frames\n");
    return -1;
  }
  if (ret_alloc < 0) return -1;

  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);
  return 0;
}

 
 /* Swap copy content page from source to destination frame */
 int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                    struct memphy_struct *mpdst, int dstfpn)
 {
   int cellidx;
   int addrsrc, addrdst;
 
   cellidx = 0;
   while (cellidx < PAGING_PAGESZ) {
     addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
     addrdst = dstfpn * PAGING_PAGESZ + cellidx;
 
     BYTE data;
     MEMPHY_read(mpsrc, addrsrc, &data);
     MEMPHY_write(mpdst, addrdst, data);
 
     cellidx += 1;
   }
 
   return 0;
 }
 
 /* Initialize empty Memory Management instance */
 int init_mm(struct mm_struct *mm, struct pcb_t *caller)
 {
   struct vm_area_struct *vma = malloc(sizeof(struct vm_area_struct));
   mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
 
   vma->vm_id = 1;
   vma->vm_start = 0;
   vma->vm_end = vma->vm_start;
   vma->sbrk = vma->vm_start;
   struct vm_rg_struct *first_rg = init_vm_rg(vma->vm_start, vma->vm_end);
   enlist_vm_rg_node(&vma->vm_freerg_list, first_rg);
 
   vma->vm_next = NULL;
   vma->vm_mm = mm;
   mm->mmap = vma;
 
   return 0;
 }
 
 struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end)
 {
   struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
   rgnode->rg_start = rg_start;
   rgnode->rg_end = rg_end;
   rgnode->rg_next = NULL;
   return rgnode;
 }
 
 int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
 {
   rgnode->rg_next = *rglist;
   *rglist = rgnode;
   return 0;
 }
 
 int enlist_pgn_node(struct pgn_t **plist, int pgn)
 {
   struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
   pnode->pgn = pgn;
   pnode->pg_next = *plist;
   *plist = pnode;
   return 0;
 }
 
 int print_list_fp(struct framephy_struct *ifp)
 {
   struct framephy_struct *fp = ifp;
   printf("print_list_fp: ");
   if (fp == NULL) { printf("NULL list\n"); return -1; }
   printf("\n");
   while (fp != NULL) {
     printf("fp[%d]\n", fp->fpn);
     fp = fp->fp_next;
   }
   printf("\n");
   return 0;
 }
 
 int print_list_rg(struct vm_rg_struct *irg)
 {
   struct vm_rg_struct *rg = irg;
   printf("print_list_rg: ");
   if (rg == NULL) { printf("NULL list\n"); return -1; }
   printf("\n");
   while (rg != NULL) {
     printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
     rg = rg->rg_next;
   }
   printf("\n");
   return 0;
 }
 
 int print_list_vma(struct vm_area_struct *ivma)
 {
   struct vm_area_struct *vma = ivma;
   printf("print_list_vma: ");
   if (vma == NULL) { printf("NULL list\n"); return -1; }
   printf("\n");
   while (vma != NULL) {
     printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
     vma = vma->vm_next;
   }
   printf("\n");
   return 0;
 }
 
 int print_list_pgn(struct pgn_t *ip)
 {
   printf("print_list_pgn: ");
   if (ip == NULL) { printf("NULL list\n"); return -1; }
   printf("\n");
   while (ip != NULL) {
     printf("va[%d]-\n", ip->pgn);
     ip = ip->pg_next;
   }
   printf("n");
   return 0;
 }
 
 int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
 {
   int pgn_start, pgn_end;
   int pgit;
 
   if (caller == NULL || caller->mm == NULL) {
     printf("NULL caller or mm\n");
     return -1;
   }
 
   if (end == -1) {
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
     if (cur_vma == NULL) {
       printf("NULL cur_vma in print_pgtbl\n");
       return -1;
     }
     end = cur_vma->vm_end;
   }
 
   pgn_start = PAGING_PGN(start);
   pgn_end   = PAGING_PGN(end);
 
   printf("print_pgtbl: %d - %d\n", start, end);
 
   pgit = pgn_start;
   while (pgit < pgn_end) {
     printf("%08ld: %08x\n", (long)(pgit * sizeof(uint32_t)), caller->mm->pgd[pgit]);
     pgit += 1;
   }
 
   return 0;
 }
 
 //#endif
 
