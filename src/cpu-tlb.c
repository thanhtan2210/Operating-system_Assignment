/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef CPU_TLB
/*
 * CPU TLB
 * TLB module cpu/cpu-tlb.c
 */
 
#include "mm.h"
#include "os-mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t vm_lock = PTHREAD_MUTEX_INITIALIZER;

int tlb_change_all_page_tables_of(struct pcb_t *proc,  struct memphy_struct * mp)
{
  /* TODO update all page table directory info 
   *      in flush or wipe TLB (if needed)
   */
  /* No need to use this or i just dont know how to implement this function ngl*/
  return 0;
}

int tlb_flush_tlb_of(struct pcb_t *proc, struct memphy_struct * mp)
{
  /* TODO flush tlb cached*/ // DONE
  struct vm_area_struct* vma = get_vma_by_num(proc->mm, 0);
  int pg_start = 0;
  int pg_end = (vma->vm_end -1) / PAGING_PAGESZ;

  for(int pgn = pg_start; pgn <= pg_end; pgn++) {
    tlb_cache_set_invalid(mp, proc, pgn);
  } 

  return 0;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr, val;

  /* By default using vmaid = 0 */
  val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1) return -1;
  /* TODO update TLB CACHED frame num of the new allocated page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  // done updated in vmap_page_range in mm.c
  int pgn = addr / PAGING_PAGESZ;
  while (pgn * 256 < addr + size)
  {
    tlb_cache_write(proc->tlb, proc, pgn);
    pgn++;
  }

#ifdef IODUMP

  printf("TLB after alloc: , PID: %d, size: %d, reg_index: %d \n", proc->pid, size, reg_index);
  TLBMEMPHY_bin_dump(proc->tlb);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
#endif
  return 0;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  if (__free(proc, 0, reg_index) == -1) return -1;

  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/ // DONE
  struct vm_rg_struct *rgnode = get_symrg_byid(proc->mm, reg_index);
  int pg_st = rgnode->rg_start / PAGING_PAGESZ;
  int pg_ed = (rgnode->rg_end-1) / PAGING_PAGESZ;
  
  if(__free(proc, 0, reg_index)==-1) return -1;

  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  for(int pgn = pg_st; pgn <= pg_ed; pgn++) {
    tlb_cache_set_invalid(proc->tlb, proc, pgn);
  }

  return 0;
}


/*tlbread - CPU TLB-based read a region memory
 *@proc: Process executing the instruction
 *@source: index of source register
 *@offset: source address = [source] + [offset]
 *@destination: destination storage
 */
int tlbread(struct pcb_t * proc, uint32_t source,
            uint32_t offset, 	uint32_t destination) 
{
  BYTE data;
  int frmnum = -1, val;
	
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/
  // DONE
  struct vm_rg_struct *currg = get_symrg_byid(proc->mm, source);
  int addr = currg->rg_start + offset;
  int pgn = PAGING_PGN(addr);

  val = __read(proc, 0, source, offset, &data);
  if(val == -1) return -1;
  frmnum = tlb_cache_read(proc->tlb, proc->pid, pgn, data);

#ifdef IODUMP
  if (frmnum >= 0) {
    printf("\tTLB hit at read region=%d offset=%d, Read value = %d\n", 
	         source, offset, data);
    pthread_mutex_lock(&vm_lock);
    proc->stat_hit_time++;
    pthread_mutex_unlock(&vm_lock);
  }
  else {
    printf("\tTLB miss at read region=%d offset=%d\n", 
	         source, offset);
    pthread_mutex_lock(&vm_lock);
    proc->stat_miss_time++;
    pthread_mutex_unlock(&vm_lock);
    // tlb_cache_write(proc->tlb, proc, pgn);
  }
  
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  destination = (uint32_t) data;

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  return val;
}

/*tlbwrite - CPU TLB-based write a region memory
 *@proc: Process executing the instruction
 *@data: data to be wrttien into memory
 *@destination: index of destination register
 *@offset: destination address = [destination] + [offset]
 */
int tlbwrite(struct pcb_t * proc, BYTE data,
             uint32_t destination, uint32_t offset)
{
  int val, frmnum = -1;

  /* TODO retrieve TLB CACHED frame num of accessing page(s))*/
  /* by using tlb_cache_read()/tlb_cache_write()
  frmnum is return value of tlb_cache_read/write value*/ // DONE

  struct vm_rg_struct *currg = get_symrg_byid(proc->mm, destination);
  int addr = currg->rg_start + offset;
  int pgn = PAGING_PGN(addr);

  frmnum = tlb_cache_read(proc->tlb, proc->pid, pgn, data);
  val = __write(proc, 0, proc->pid, offset, data);

  if (val == -1) return -1;

#ifdef IODUMP
  if (frmnum >= 0)
  {
    printf("TLB hit at write region=%d offset=%d value=%d\n",
	          destination, offset, data);
    pthread_mutex_lock(&vm_lock);
    proc->stat_hit_time++;
    pthread_mutex_unlock(&vm_lock);
  }
	else
  {
    printf("TLB miss at write region=%d offset=%d value=%d\n",
            destination, offset, data);
    pthread_mutex_lock(&vm_lock);
    proc->stat_miss_time++;
    pthread_mutex_unlock(&vm_lock);
    tlb_cache_write(proc->tlb, proc, pgn);
  }
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  val = __write(proc, 0, destination, offset, data);

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

  return val;
}

//#endif
