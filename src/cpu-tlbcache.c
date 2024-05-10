/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef MM_TLB
/*
 * Memory physical based TLB Cache
 * TLB cache module tlb/tlbcache.c
 *
 * TLB cache is physically memory phy
 * supports random access 
 * and runs at high speed
 */


#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

void printBits(unsigned int v) {
   unsigned int cv = v;
   char bit[33];
   unsigned int mask = (1<<31);
   for(int i=0; i<32; i++) {
      if( (mask & v) != 0 ) bit[i] = '1';
      else bit[i] = '0' ;
      mask = (mask >> 1);
   }
   bit[32] = '\0';
   printf("(%u) %s\n",cv ,bit );
}


#define init_tlbcache(mp,sz,...) init_memphy(mp, sz, (1, ##__VA_ARGS__))

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */

void init_tlb_entry(BYTE *tlb, int valid, int pid, int pg_num, int frm_num) {
    pthread_mutex_lock(&cache_lock);
    *tlb = (valid & 0x1) << 31;               // 1 bit valid
    *tlb |= (pid & 0x1F) << 26;               // 5 bit pid
    *tlb |= (pg_num & 0x3FFF) << 12;          // 14 bit page number
    *tlb |= (frm_num & 0xFFF);  	      // 12 bit frame number
    pthread_mutex_unlock(&cache_lock);
}

// Get Valid from TLB entry
unsigned int get_valid(BYTE tlb_entry) {
    return (tlb_entry >> 31) & 0x1;
}

// get PID from TLB entry
unsigned int get_pid(BYTE tlb_entry) {
    return (tlb_entry >> 26) & 0x1F;
}

// Get page number from TLB entry
unsigned int get_pagenum(BYTE tlb_entry) {
    return (tlb_entry >> 12) & 0x3FFF;
}

// Get frame number from TLB entry
unsigned int get_framenum(BYTE tlb_entry) {
    return tlb_entry & 0xFFF;
}

unsigned int get_fpn(unsigned int num) {
  return num & 0xFFF;
}



int tlb_cache_read(struct memphy_struct * mp, int pid, int pgnum, BYTE value)
{
   /* TODO: the identify info is mapped to 
    *      cache line by employing:
    *      direct mapped, associated mapping etc.
    */ //DONE
   const unsigned int num_tlb_entries = mp->maxsz / 16;

   unsigned int concat_address = (pid<<14) + pgnum; // retrive the combined address
   unsigned int tlbnum = ((pid<<5) + pgnum%(1<<5)) % num_tlb_entries;

   unsigned int phy_adr = tlbnum * 16;
   unsigned int tag, wvalue[2];

   unsigned int valid_mask = (1<<31);

   wvalue[0] = TLBMEMPHY_read_word(mp, phy_adr);
   wvalue[1] = TLBMEMPHY_read_word(mp, phy_adr+8);

   if(wvalue[0]==-1 || wvalue[1]==-1) return -1;
   
   for(int i=0; i<2; i++, phy_adr+=8) {
      if((wvalue[i] & valid_mask) ==0) continue;

      tag = wvalue[i] % (1<<30);
      if(tag == concat_address) {
         uint32_t pte = TLBMEMPHY_read_word(mp, phy_adr+4);  
         if(PAGING_PAGE_PRESENT(pte)) {
            TLBMEMPHY_write(mp, phy_adr, ((wvalue[i] >> 24) | 64)); // Set LRU of this block to 1
            TLBMEMPHY_write(mp, phy_adr + (i==0?8:-8), ((wvalue[1-i] >> 24) & 95 )); // Set LRU of other block to 0 
            return PAGING_FPN(pte);
         }
      }
   }

   return -1;
}

/*
 *  tlb_cache_write write TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_write(struct memphy_struct *mp, struct pcb_t* proc, int pgnum)
{
   /* TODO: the identify info is mapped to 
    *      cache line by employing:
    *      direct mapped, associated mapping etc.
    */ // DONE
   uint32_t pid = proc->pid;
   
   const unsigned int num_tlb_entries = mp->maxsz / 16;
   unsigned int concat_address = (pid<<14) + pgnum; // retrive the combined address
   unsigned int tlbnum = ((pid<<5) + pgnum%(1<<5)) % num_tlb_entries;

   unsigned int phy_adr = tlbnum * 16;
   unsigned int tag = concat_address, wvalue;


   for(int i=0; i<2; i++, phy_adr+=8) {
      BYTE value;
      TLBMEMPHY_read(mp, phy_adr, &value);
      if((value & (1<<7)) == 0) { // If valid = 0
         
         wvalue = ((3<<30) | tag);
         TLBMEMPHY_write_word(mp, phy_adr, wvalue);

         //TLBMEMPHY_dump(mp);

         if(i==0) {
            TLBMEMPHY_read(mp, phy_adr + 8, &value);
            TLBMEMPHY_write(mp, phy_adr + 8, (value & 95)); // Set LRU other block to 0
         }
         else {
            TLBMEMPHY_read(mp, phy_adr - 8, &value);
            TLBMEMPHY_write(mp, phy_adr - 8, (value & 95)); 
         }

         unsigned int pte = proc->mm->pgd[pgnum];
         TLBMEMPHY_write_word(mp,phy_adr+4,pte);

         return PAGING_FPN(pte);
      }
   }

   // If no valid = 0

   for(int i=0; i<2; i++, phy_adr+=8) {
      BYTE value;
      TLBMEMPHY_read(mp, phy_adr, &value);
      if((value & (1<<6)) == 0) { // If LRU = 0 _ Must have
         wvalue = ((3<<30) | concat_address);
         TLBMEMPHY_write_word(mp, phy_adr, wvalue);
         if(i==0) {
            TLBMEMPHY_read(mp, phy_adr + 8, &value);
            TLBMEMPHY_write(mp, phy_adr + 8, (value & 95)); // Set LRU other block to 0
         }
         else {
            TLBMEMPHY_read(mp, phy_adr - 8, &value);
            TLBMEMPHY_write(mp, phy_adr - 8, (value & 95)); 
         }

         unsigned int pte = proc->mm->pgd[pgnum];
         TLBMEMPHY_write_word(mp,phy_adr+4,pte);

         return PAGING_FPN(pte);
      }
   }

   return 0;
}

int tlb_cache_set_invalid(struct memphy_struct *mp, struct pcb_t* proc, int pgnum) {
   int pid = proc->pid;
   const unsigned int num_tlb_entries = mp->maxsz / 16;

   unsigned int concat_address = (pid<<14) + pgnum; // retrive the combined address
   unsigned int tlbnum = ((pid<<5) + pgnum%(1<<5)) % num_tlb_entries;

   unsigned int phy_adr = tlbnum * 16;
   unsigned int tag, wvalue[2];

   unsigned int valid_mask = (1<<31);

   wvalue[0] = TLBMEMPHY_read_word(mp, phy_adr);
   wvalue[1] = TLBMEMPHY_read_word(mp, phy_adr+8);

   if(wvalue[0]==-1 || wvalue[1]==-1) return -1;
   
   for(int i=0; i<2; i++, phy_adr+=8) {
      if((wvalue[i] & valid_mask) ==0) continue;

      tag = wvalue[i] % (1<<30);
      if(tag == concat_address) {
         TLBMEMPHY_write_word(mp, phy_adr, 0);
         TLBMEMPHY_write_word(mp, phy_adr+4, 0);
         return 0;
      }
   }
   return 0;
}


/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */

unsigned int TLBMEMPHY_read_word(struct memphy_struct * mp, int addr) {
   unsigned int val = 0;
   for(int i=0; i<4; i++) {
      BYTE value;
      if( TLBMEMPHY_read(mp, addr+i, &value) == -1) {
         perror("Cache reading error!");
         return -1;
      }
      val = (val<<8) + (int)value;
   }
   return val;
}

int TLBMEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   *value = mp->storage[addr];

   return 0;
}

unsigned int TLBMEMPHY_write_word(struct memphy_struct * mp, int addr, unsigned int data) {
   //printf("DAT %u", data);
   unsigned int val = 0;
   unsigned int mask = 0b11111111;
   for(int i=3; i>=0; i--) {
      unsigned char value = (unsigned char)((data & mask)%256);
      //printf("BIT TEST %c, %u, %u - ", value, data, mask);
      //printBits((unsigned int)value);
      if( TLBMEMPHY_write(mp, addr+i, value) == -1) {
         perror("Cache writing error!");
         return -1;
      }
      data = (data >> 8);
   }
   return val;
}
/*
 *  TLBMEMPHY_write natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int TLBMEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   mp->storage[addr] = data;

   return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */


int TLBMEMPHY_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
    *     for tracing the memory content
    */
#ifdef OUTPUT_FOLDER
   FILE *output_file = mp->file;
   fprintf(output_file, "===== PHYSICAL MEMORY DUMP (TLB CACHE) =====\n");
#endif

   printf("\t\tPHYSICAL MEMORY (TLB CACHE) DUMP :\n");
   for (int i = 0; i < mp->maxsz; ++i)
   {
      if (mp->storage[i] != 0)
      {
#ifdef OUTPUT_FOLDER
         fprintf(output_file, "BYTE %08x: %d\n", i, mp->storage[i]);
#endif
         printf("BYTE %08x: %d\n", i, mp->storage[i]);
      }
   }
#ifdef OUTPUT_FOLDER
   fprintf(output_file, "===== PHYSICAL MEMORY END-DUMP (TLB CACHE)=====\n");
   fprintf(output_file, "================================================================\n");
#endif

   printf("\t\tPHYSICAL MEMORY END-DUMP\n");
   return 0;
}

int TLBMEMPHY_bin_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
    *     for tracing the memory content
    */

   printf("\t*** PHYSICAL MEMORY (TLB CACHE) BIN DUMP:\n");
   for (int i = 0; i < mp->maxsz; i+=4)
   {
      if (mp->storage[i] != 0)
      {
#ifdef OUTPUT_FOLDER
         fprintf(output_file, "BYTE %08x: %d\n", i, mp->storage[i]);
#endif
         printf("\t   (%d) %08x: ",i, i);
         printBits(TLBMEMPHY_read_word(mp, i));
      }
   }

   printf("\t*** PHYSICAL MEMORY END-DUMP\n");
   return 0;
}



/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct memphy_struct *mp, int max_size)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   mp->maxsz = max_size;

   mp->rdmflg = 1;

   return 0;
}

//#endif
