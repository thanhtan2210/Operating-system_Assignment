
#include "queue.h"
#include "sched.h"
#include "mm.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i ;

	for (i = 0; i < MAX_PRIO; i ++)
		mlq_ready_queue[i].size = 0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	run_queue.slot = MAX_PRIO;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 */ // DONE
	pthread_mutex_lock(&queue_lock);
	for (int i = 0; i < MAX_PRIO; i++)
	{
		if (!empty(&mlq_ready_queue[i]))
		{
			proc = dequeue(&mlq_ready_queue[i]);
			if (proc != NULL) mlq_ready_queue[i].slot--;
			break;
		}
	}
	pthread_mutex_unlock(&queue_lock);

	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	mlq_ready_queue[proc->prio].slot++;
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
		pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}

void end_proc(struct pcb_t **proc)
{
	pthread_mutex_lock(&queue_lock);
	mlq_ready_queue[(*proc)->prio].slot++;
#ifdef CPU_TLB
	tlb_flush_tlb_of((*proc), (*proc)->tlb);
	struct vm_area_struct *vma = get_vma_by_num((*proc)->mm, 0);
	int pg_start = vma->vm_start, pg_end = (vma->vm_end - 1) / PAGING_PAGESZ;

	for (int i = pg_start; i < pg_end; i++)
	{
		int pte = (*proc)->mm->pgd[i];
		if (PAGING_PAGE_PRESENT(pte))
		{
			int fpn = PAGING_FPN(pte);
			MEMPHY_free_frame((*proc)->mram, fpn);
		}
		else
		{
			int fpn = PAGING_SWP(pte);
			MEMPHY_free_frame((*proc)->active_mswp, fpn);
		}
	}
#endif
	pthread_mutex_unlock(&queue_lock);
	free(*proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */ // DONE
	pthread_mutex_lock(&queue_lock);
	if (!empty(&ready_queue)) proc = dequeue(&ready_queue);
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


