/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <clock.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->stackbase_top=0;
	as->stackbase_base=0;

	as->heap_end=0;
	as->heap_start=0;

	as->lock_page_table = lock_create("page_table");
	as->page_table = NULL;

	as->regions=NULL;


	return as;
}

void
as_destroy(struct addrspace *as)
{

	if(as!= NULL){
		struct addr_regions *next= as->regions;
		while(as->regions != NULL){
			next= as->regions->next_region;
			as->regions->region_numpages=0;
			as->regions->set_permissions=0;
			as->regions->va_end=0;
			as->regions->va_start=0;
			kfree(as->regions);
			as->regions= next;
		}
		struct page_table_entry *next_page= as->page_table;
		while(as->page_table!= NULL){
			lock_acquire(as->lock_page_table);
			next_page = as->page_table->next;
			if(as->page_table->present==0){
				//Acquiring lock for swap array

					swap_info[as->page_table->swapfile_index]->va= 0;

			}
			else{
				page_free(as->page_table->pa);
			}
			as->page_table->permissions=0;
			as->page_table->present=0;
			as->page_table->va=0;
			as->page_table->swapfile_index=0;
			lock_release(as->lock_page_table);
			kfree(as->page_table);
			as->page_table= next_page;
		}
		as->heap_end=0;
		as->heap_start=0;
		as->stackbase_base=0;
		as->stackbase_top=0;
		kfree(as->lock_page_table);
	}
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	int i, spl;

	(void)as;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();
	/*if(curthread->t_cpu->c_number){
			spinlock_acquire(&tlb_lock1);
		}else if(curthread->t_cpu->c_number==2){
			spinlock_acquire(&tlb_lock2);
		}else if(curthread->t_cpu->c_number==3){
			spinlock_acquire(&tlb_lock3);
		}else if(curthread->t_cpu->c_number==4){
			spinlock_acquire(&tlb_lock4);
		}*/
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	/*if(curthread->t_cpu->c_number==1){
				spinlock_release(&tlb_lock1);
		}else if(curthread->t_cpu->c_number==2){
				spinlock_release(&tlb_lock2);
		}else if(curthread->t_cpu->c_number==3){
				spinlock_release(&tlb_lock3);
		}else if(curthread->t_cpu->c_number==4){
				spinlock_release(&tlb_lock4);
		}*/
	splx(spl);
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/**
	 * Now define a new structure of struct addr_regions type
	 * Allocate the fields
	 * Assign the structure to the addr space
	 */


	if(as->regions==NULL)
	{
		//Means it is the first region

		//Allocate all parameters and equate to the new region
		as->regions = (struct addr_regions *) kmalloc(sizeof(struct addr_regions));

		as->regions->va_start = vaddr;
		as->regions->region_numpages= npages;
		as->regions->va_end = (as->regions->va_start + (npages * PAGE_SIZE));


		//Set the permissions in other variable
		//int sum=
		as->regions->set_permissions=readable+writeable+executable;

		as->regions->next_region = NULL;
	}
	else
	{
		//regions is not NULL -- meaning that some region has been added before
		//Iterate to reach the end
		struct addr_regions *end;
		end = (struct addr_regions *) kmalloc(sizeof(struct addr_regions));

		end->va_start = vaddr;
		end->region_numpages= npages;
		end->va_end = (end->va_start + (npages * PAGE_SIZE));

		end->next_region = NULL;

		//Set the permissions in other variable
		int sum = readable+writeable+executable;
		end->set_permissions=sum;

		struct addr_regions *head;

		head = as->regions;

		while(as->regions != NULL)
		{
			as->regions=as->regions->next_region;
		}
		as->regions=end;

		head->next_region=as->regions;
		as->regions=head;


	}

	//Now declare the heap start and heap end
	as->heap_start = (vaddr & PAGE_FRAME)+ npages*PAGE_SIZE;
	as->heap_end = (vaddr & PAGE_FRAME)+ npages*PAGE_SIZE;

	//Declare the Stack start and stack end
	as->stackbase_top = (USERSTACK);
	as->stackbase_base =  USERSTACK - (VM_STACKPAGES * PAGE_SIZE);

	return 0;
}

void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}


int
as_prepare_load(struct addrspace *as)
{
	/*kprintf("Stack TOP is %d",as->stackbase_top);
	kprintf("\nStack Base is %d",as->stackbase_base);

	kprintf("\nHeap Start is %d",as->heap_start);
	kprintf("\nHeap End is %d",as->heap_end);

*/

	struct addr_regions *head;
	 head = as->regions;
	if(as->regions!=NULL)
	{
		//Iterate over the regions

		while(as->regions !=NULL)
		{
/*
			kprintf("\nRegion Start is %d",as->regions->va_start);
			kprintf("\nRegion End is %d",as->regions->va_end);
*/

			as->regions=as->regions->next_region;
		}

		as->regions = head;
	}

	return 0;
}


int
as_complete_load(struct addrspace *as)
{
	int sum;
	struct addr_regions *head;
		 head = as->regions;
	//Iterate over the regions and reset the permissions
		//Iterate over the regions
	while(as->regions !=NULL)
	{

		//Get the old permission
		sum = as->regions->set_permissions;
//TODO:: DECIDE WHOM TO GIVE PERMISSIONS-- REGIONS OR THE PAGE AND THEN RELOAD THEM HERE

/*
		if((sum&1) != 0)
			as->regions->execute_permission=1;
		else
			as->regions->execute_permission=0;

		if((sum&2) != 0)
			as->regions->write_permission=2;
		else
			as->regions->write_permission=0;

		if((sum&4) != 0)
			as->regions->read_permission=4;
		else
			as->regions->read_permission=0;
*/


		as->regions=as->regions->next_region;

		}
	as->regions = head;

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->stackbase_top!= 0);

	*stackptr = USERSTACK;
	return 0;
}


int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;
	//Creating new address space calling as create, which will initialize lock also;
	new = as_create();
	if (new==NULL)
	{
		return ENOMEM;
	}

	//Coping addr_regions to new address space addr_regions structure
	struct addr_regions *newregionshead;
	struct addr_regions *oldregionshead;
	oldregionshead = old->regions;
	int count=0;

	while(old->regions !=NULL)
	{
		if(count==0)
		{
			lock_acquire(vm_fault_lock);
			new->regions=(struct addr_regions*) kmalloc(sizeof(struct addr_regions ));
			lock_release(vm_fault_lock);
			newregionshead= new->regions;
			new->regions->va_start= old->regions->va_start;
			new->regions->region_numpages= old->regions->region_numpages;
			new->regions->set_permissions= old->regions->set_permissions;
			new->regions->va_end= old->regions->va_end;
			old->regions= old->regions->next_region;
			if(old->regions!= NULL)
			{
				lock_acquire(vm_fault_lock);
				new->regions->next_region= (struct addr_regions*) kmalloc(sizeof(struct addr_regions ));
				lock_release(vm_fault_lock);
				new->regions= new->regions->next_region;
			}
			else
			{
				new->regions->next_region= NULL;
			}

		}
		else
		{
				new->regions->va_start= old->regions->va_start;
				new->regions->region_numpages= old->regions->region_numpages;
				new->regions->set_permissions= old->regions->set_permissions;
				new->regions->va_end= old->regions->va_end;
				old->regions= old->regions->next_region;
				if(old->regions!= NULL){
					lock_acquire(vm_fault_lock);
					new->regions->next_region= (struct addr_regions*) kmalloc(sizeof(struct addr_regions ));
					lock_release(vm_fault_lock);
					new->regions= new->regions->next_region;
				}
				else
				{
					new->regions->next_region= NULL;
				}
			}
		count++;
		}

		//Setting the pointer to the start of the list
		old->regions= oldregionshead;
		new->regions= newregionshead;
		new->heap_end= old->heap_end;
		new->heap_start= old->heap_start;
		new->stackbase_base= old->stackbase_base;
		new->stackbase_top= old->stackbase_top;
/*
		//Call copy function to copy the link list from old to new pagetable
		pagetable_copy(old->page_table,&new->page_table,new);
		*ret = new;
		return 0;
*/

		//Setting the pointer to the start of the list
		old->regions= oldregionshead;
		new->regions= newregionshead;
		new->heap_end= old->heap_end;
		new->heap_start= old->heap_start;
		new->stackbase_base= old->stackbase_base;
		new->stackbase_top= old->stackbase_top;

		//Coping page table to new address space page table structure
		struct page_table_entry *new_ptentry_head;
		struct page_table_entry *old_ptentry_head;
		old_ptentry_head = old->page_table;
		int count_pt= 0;
		while(old->page_table != NULL)
		{
			int ind = ((old->page_table->pa - coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;
			coremap[ind].locked=1;
			int new_index;
			if(count_pt==0)
			{
				lock_acquire(vm_fault_lock);
				new->page_table=(struct page_table_entry*) kmalloc(sizeof(struct page_table_entry));
				lock_release(vm_fault_lock);
				new_ptentry_head= new->page_table;
				new->page_table->pa= alloc_newPage(new,&new_index,old);

				if(old->page_table->present== 1)
				{
					if(coremap[ind].as!=old)
						panic("Old Address table does not match with coremap entry");

					memmove((void *)PADDR_TO_KVADDR(new->page_table->pa),(const void *)PADDR_TO_KVADDR(old->page_table->pa),PAGE_SIZE);
				}
				else
				{
					swapin_page(old->page_table->pa, old->page_table->swapfile_index);
				}

				new->page_table->permissions= old->page_table->permissions;
				new->page_table->present= 1;
				new->page_table->va= old->page_table->va;
				new->page_table->swapfile_index=0;

				coremap[ind].locked=0;
				old->page_table= old->page_table->next;

				if(old->page_table!= NULL)
				{
					lock_acquire(vm_fault_lock);
					new->page_table->next=(struct page_table_entry*) kmalloc(sizeof(struct page_table_entry));
					lock_release(vm_fault_lock);
					new->page_table= new->page_table->next;
				}
				else
				{
					new->page_table->next= NULL;
				}
				coremap[ind].locked=0;
				coremap[new_index].locked=0;
			}
			else
			{
				new->page_table->pa= alloc_newPage(new,&new_index,old);

				if(old->page_table->present== 1)
				{
					memmove((void *)PADDR_TO_KVADDR(new->page_table->pa),(const void *)PADDR_TO_KVADDR(old->page_table->pa),PAGE_SIZE);
				}
				else
				{
					swapin_page(old->page_table->pa, old->page_table->swapfile_index);
				}

				new->page_table->permissions= old->page_table->permissions;
				new->page_table->present= 1;
				new->page_table->va= old->page_table->va;
				new->page_table->swapfile_index=0;

				old->page_table= old->page_table->next;

				if(old->page_table!= NULL)
				{
					lock_acquire(vm_fault_lock);
					new->page_table->next=(struct page_table_entry*) kmalloc(sizeof(struct page_table_entry));
					lock_release(vm_fault_lock);
					new->page_table= new->page_table->next;
				}
				else
				{
					new->page_table->next= NULL;
				}
				coremap[ind].locked=0;
				coremap[new_index].locked=0;
			}
			count_pt++;
		}

		//Setting the head to the start of the list
		old->page_table= old_ptentry_head;
		new->page_table= new_ptentry_head;
		//lock_release(old->lock_page_table);

		//Coping rest of the remaining entries in the old structure to new structure


		struct page_table_entry *head;
		head = old->page_table;
		while(old->page_table!=NULL)
		{
			int ind;
			ind = ((old->page_table->pa - coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;

			coremap[ind].locked=0;

			old->page_table=  old->page_table->next;
		}
		old->page_table= head;

		struct page_table_entry *nh;
		nh = new->page_table;
		while(new->page_table!=NULL)
		{
			int ind;
			ind = ((new->page_table->pa - coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;

			coremap[ind].locked=0;

			new->page_table=  new->page_table->next;

		}
		new->page_table= nh;



		*ret = new;
		return 0;



}


paddr_t
alloc_newPage(struct addrspace *new,int *index,struct addrspace *old)
{

	paddr_t newaddr=0;
	int new_page_index;

	//Take the coremap lock and find an index to map the entry
	spinlock_acquire(&coremap_lock);

	new_page_index = find_available_page();

	if(coremap[new_page_index].as ==new)
		panic("COREMAP INDEX ADDRESS SPACE MATCHES WITH NEW");

	if(coremap[new_page_index].as ==old)
		panic("COREMAP INDEX ADDRESS SPACE MATCHES WITH OLD");


	coremap[new_page_index].locked=1;

	spinlock_release(&coremap_lock);

	lock_acquire(vm_fault_lock);

	//Call change coremap page entry in order to make the page available for you
	if(coremap[new_page_index].page_status!=0)
		change_coremap_page_entry(new_page_index);

	lock_release(vm_fault_lock);

	//Now that particular entry at coremap[new_page_index] is free for you

	//Getting time
	time_t seconds;
	uint32_t nanoseconds;
	gettime(&seconds, &nanoseconds);

	//Update the coremap entries
	coremap[new_page_index].as=new;
	coremap[new_page_index].chunk_allocated=0;
	coremap[new_page_index].page_status=2;
	coremap[new_page_index].time=nanoseconds;

	newaddr = coremap[new_page_index].ce_paddr;

	*index= new_page_index;

	//Zero the region
	as_zero_region(coremap[new_page_index].ce_paddr,1);

	return newaddr;

}
