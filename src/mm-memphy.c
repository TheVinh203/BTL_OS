//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

 #include "mm.h"
 #include <stdio.h>
 #include <stdlib.h>
 
 /*
  *  MEMPHY_mv_csr - move MEMPHY cursor
  *  @mp: memphy struct
  *  @offset: offset
  */
 int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
 {
    int numstep = 0;
 
    mp->cursor = 0;
    while (numstep < offset && numstep < mp->maxsz) {
      /* Traverse sequentially */
      mp->cursor = (mp->cursor + 1) % mp->maxsz;
      numstep++;
    }
 
    return 0;
 }
 
 /*
  *  MEMPHY_seq_read - read MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @value: obtained value
  */
 int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
 {
    if (mp == NULL)
      return -1;
 
    if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */
 
    MEMPHY_mv_csr(mp, addr);
    *value = (BYTE) mp->storage[addr];
 
    return 0;
 }
 
 /*
  *  MEMPHY_read read MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @value: obtained value
  */
 int MEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value)
 {
   if (mp == NULL) return -1;
 
   int ret = 0;
   if (mp->rdmflg) *value = mp->storage[addr];
   else            ret = MEMPHY_seq_read(mp, addr, value);
 
   printf("[DBG] MEM[R] addr=%d -> %d\n", addr, *value);
   return ret;
 }
 
 /*
  *  MEMPHY_seq_write - write MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @data: written data
  */
 int MEMPHY_seq_write(struct memphy_struct * mp, int addr, BYTE value)
 {
    if (mp == NULL)
      return -1;
 
    if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential write */
 
    MEMPHY_mv_csr(mp, addr);
    mp->storage[addr] = value;
 
    return 0;
 }
 
 /*
  *  MEMPHY_write-write MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @data: written data
  */
 int MEMPHY_write(struct memphy_struct *mp, int addr, BYTE data)
 {
   printf("[DBG] MEM[W] addr=%d val=%d\n", addr, data);
 
   if (mp == NULL) return -1;
   if (mp->rdmflg) mp->storage[addr] = data;
   else            return MEMPHY_seq_write(mp, addr, data);
   return 0;
 }
 
 /*
  *  MEMPHY_format-format MEMPHY device
  *  @mp: memphy struct
  */
 int MEMPHY_format(struct memphy_struct *mp, int pagesz)
 {
     /* This setting comes with fixed constant PAGESZ */
     int numfp = mp->maxsz / pagesz;
     struct framephy_struct *newfst, *fst;
     int iter = 1;
 
     if (numfp <= 0)
       return -1;
 
     /* Init head of free framephy list */ 
     fst = malloc(sizeof(struct framephy_struct));
     fst->fpn = 0;
     mp->free_fp_list = fst;
 
     /* We have list with first element, fill in the rest numfp-1 elements */
     while (iter < numfp) {
        newfst = malloc(sizeof(struct framephy_struct));
        newfst->fpn = iter;
        newfst->fp_next = NULL;
        fst->fp_next = newfst;
        fst = newfst;
        iter++;
     }
 
     return 0;
 }
 
 int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
 {
    struct framephy_struct *fp = mp->free_fp_list;
 
    if (fp == NULL)
      return -1;
 
    *retfpn = fp->fpn;
    mp->free_fp_list = fp->fp_next;
 
    free(fp);
 
    return 0;
 }
 
 int MEMPHY_dump(struct memphy_struct * mp)
 {
     /*TODO dump memphy content mp->storage */
     return 0;
 }
 
 int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
 {
    struct framephy_struct *fp = mp->free_fp_list;
    struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));
 
    newnode->fpn = fpn;
    newnode->fp_next = fp;
    mp->free_fp_list = newnode;
 
    return 0;
 }
 
 /*
  *  Init MEMPHY struct
  */
 int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
 {
    mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
    mp->maxsz = max_size;
 
    MEMPHY_format(mp, PAGING_PAGESZ);
 
    mp->rdmflg = (randomflg != 0) ? 1 : 0;
 
    if (!mp->rdmflg)
       mp->cursor = 0;
 
    return 0;
 }
 
 //#endif
 
