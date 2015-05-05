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
#include <kern/fcntl.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <mips/vm.h>
#include<clock.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12


/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

/**
 * Added by Pratham Malik
 * Redeclaration of global variable for:
 * 1. Storing the total number of pages
 * 2. Coremap entry structure
 * 3. Coremap initialization checking boolean
 * 3. Coremap global lock
 */

int32_t total_systempages;
int32_t coremap_pages;
struct coremap_entry *coremap;
bool coremap_initialized;
struct spinlock coremap_lock= SPINLOCK_INITIALIZER;

struct swap_elements *swap_info[SWAP_MAX];
struct lock *swap_lock;
struct vnode *swapfile_vnode;
struct lock *swap_file_lock;

struct spinlock tlb_lock1;
struct spinlock tlb_lock2;
struct spinlock tlb_lock3;
struct spinlock tlb_lock4;

//ENd of Additions by PM
void
vm_bootstrap(void)
{
	paddr_t firstpaddr, lastpaddr;

	//Create the global coremap lock
	spinlock_init(&tlb_lock1);
	spinlock_init(&tlb_lock2);
	spinlock_init(&tlb_lock3);
	spinlock_init(&tlb_lock4);
	//Get the first and last physical address of RAM
	ram_getsize(&firstpaddr, &lastpaddr);

	//Calculate the total number of pages --
	//TODO ADD COde to Roundoff the result or int will itself do the round off
	int total_page_num = (lastpaddr- firstpaddr) / PAGE_SIZE;
	total_systempages = total_page_num;
	/* pages should be a kernel virtual address !!  */
	coremap = (struct coremap_entry*)PADDR_TO_KVADDR(firstpaddr);
	paddr_t freepaddr = firstpaddr + total_page_num * sizeof(struct coremap_entry);

	//Store the number of coremap Pages
//	int noOfcoremapPages= (freepaddr-firstpaddr)/PAGE_SIZE;
	int num_coremapPages= (freepaddr-firstpaddr)/PAGE_SIZE+1;
	kprintf("\ntotal pages %d \n",total_systempages);
	kprintf("total coremap pages %d \n",num_coremapPages);

	coremap_pages=num_coremapPages;
	/*
	 * Mark the coremap pages as status as fixed i.e. Set to 1
	 * i.e. pages from firstaddr to freeaddr
	 * i.e. pages from 0 to num_coremapPages
	 */

	for(int i=0;i<num_coremapPages;i++)
	{
		coremap[i].ce_paddr= firstpaddr+i*PAGE_SIZE;
		coremap[i].page_status=1;	//Signifying that it is fixed by kernel

	}

	/*
	 * Mark the pages other than coremap with status as free i.e. Set to 0
	 * i.e. pages from firstaddr to freeaddr
	 * i.e. pages from 0 to num_coremapPages
	 */

	for(int i=num_coremapPages;i<total_page_num;i++)
	{
		coremap[i].ce_paddr= firstpaddr+i*PAGE_SIZE;
		coremap[i].page_status=0;	//Signifying that it is free
		coremap[i].chunk_allocated=0;
		coremap[i].as=NULL;
		coremap[i].time= 0;
	}


	/*
	 * Setting the value for coremap_initialized to be 1
	 * so that now alloc_kpages works differently before and after vm_bootstrap
	 */

	coremap_initialized= 1;
//	kprintf("Exiting VM_Bootstrap \n");


}

paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */

vaddr_t
alloc_kpages(int npages)
{
	paddr_t pa;
	vaddr_t va;

	if(!coremap_initialized){
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}

	else
	{
		//Means that coremap has been initialized and now allocate pages from the coremap

		spinlock_acquire(&coremap_lock);

		if(npages==1)
		{
			//Call function to find the available page
			int index = find_page_available(npages);

			//Check if the page has to be evicted or was it free
			if(coremap[index].page_status==0)
			{
				//means page was free
				va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

				as_zero_region(coremap[index].ce_paddr,npages);

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				coremap[index].chunk_allocated=1;
				coremap[index].as=curthread->t_addrspace;
				coremap[index].page_status=1;
				coremap[index].time= seconds+nanoseconds;
			}
			else
			{
				/**
				 * Meaning that coremap entry at index has to be evicted
				 * Call function to Find the page table entry marked at address and change the page table present bit to 0
				 */

				//TODO:: CHNAGE THIS
		//		change_page_entry(index);

				if(coremap[index].page_status==0)
				{
					//Just allocate

					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

					as_zero_region(coremap[index].ce_paddr,npages);

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					coremap[index].chunk_allocated=1;
					coremap[index].as=curthread->t_addrspace;
					coremap[index].page_status=1;
					coremap[index].time= seconds+nanoseconds;

					spinlock_release(&coremap_lock);


				}
				else if(coremap[index].page_status==3)
				{
					//Page is clean -- evict the current page
					//Take the page table lock

					evict_coremap_entry(index);
					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

					as_zero_region(coremap[index].ce_paddr,npages);

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					coremap[index].chunk_allocated=1;
					coremap[index].as=curthread->t_addrspace;
					coremap[index].page_status=1;
					coremap[index].time= seconds+nanoseconds;

					spinlock_release(&coremap_lock);

				}
				else if(coremap[index].page_status==2)
				{
					//Page is dirty -- swap out the page first and then make the page available
					int swap_index;
					swap_index = swapout_change_coremap_entry(index);
					lock_acquire(swap_lock);
						vaddr_t tlb_vaddr= swap_info[swap_index]->va;
					lock_release(swap_lock);
					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					coremap[index].chunk_allocated=1;
					coremap[index].as=curthread->t_addrspace;
					coremap[index].page_status=1;
					coremap[index].time= seconds+nanoseconds;

					spinlock_release(&coremap_lock);

					//Now write the file -- Swapout the old page
					my_tlb_shhotdown(tlb_vaddr);
					write_page(coremap[index].ce_paddr,swap_index);

					//Zero the region
					as_zero_region(coremap[index].ce_paddr,1);

				}
			}

		}
		else if(npages >1)
		{
			spinlock_acquire(&coremap_lock);
			int index = find_page_available(npages);
			if(index<0)
				pa=0;
			else
			{
				//Meaning pages found to replace
				//As the pages are contiguous -- Iterate over them and change page entries one by one
				for(int i =index;i<index+npages;i++)
				{
					//Take the page table lock
					lock_acquire(coremap[index].as->lock_page_table);

					panic("CHANGE PAGE ENTRY DOES NOT WORK HERE IN NPAGES>1");
					//TODO:: CHNAGE THIS
//					change_page_entry(i);

					//Release page table lock
					lock_release(coremap[index].as->lock_page_table);

					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					coremap[i].page_status=1;
					coremap[i].chunk_allocated=0;
					coremap[i].as=curthread->t_addrspace;
					coremap[index].time= seconds+nanoseconds;
				}

				coremap[index].chunk_allocated=npages;
				as_zero_region(coremap[index].ce_paddr,npages);

			}
		} //End of if checking whether npages more than 1
		spinlock_release(&coremap_lock);

		//Return the physical address retrieved from the coremap
		return va;
	}
}

void
page_free(paddr_t paddr){
	spinlock_acquire(&coremap_lock);
	int coremap_entry= ((paddr- coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;
	as_zero_region(coremap[coremap_entry].ce_paddr,coremap[coremap_entry].chunk_allocated);
	if(coremap[coremap_entry].chunk_allocated==0){
		coremap[coremap_entry].page_status=0;
		coremap[coremap_entry].time=0;
		coremap[coremap_entry].as=NULL;
		coremap[coremap_entry].chunk_allocated=0;
	}
	else{
		for(int j=0; j< coremap[coremap_entry].chunk_allocated; j++){
			coremap[j].page_status=0;
			coremap[j].time=0;
			coremap[j].as=NULL;
			coremap[j].chunk_allocated=0;
		}
	}

	spinlock_release(&coremap_lock);
}


void
free_kpages(vaddr_t addr)
{
	/*for(int i =0; i<total_systempages; i++){
		kprintf("Page status of pages %d %d\n",i,coremap[i].page_status);
	}*/
	spinlock_acquire(&coremap_lock);
	paddr_t p= KVADDR_TO_PADDR(addr);
	for(int i= coremap_pages; i<= total_systempages; i++){
		if(p== coremap[i].ce_paddr){
			if(curthread->t_addrspace== coremap[i].as){
				int chunkzise= coremap[i].chunk_allocated;
				if(chunkzise==0){
					as_zero_region(coremap[i].ce_paddr, chunkzise);
					coremap[i].page_status=0;
					coremap[i].as=NULL;
					coremap[i].chunk_allocated=0;
					coremap[i].time=0;
				}
				else{
					as_zero_region(coremap[i].ce_paddr, chunkzise);
					for(int j=i; j<i+chunkzise; j++){
						coremap[j].page_status=0;
						coremap[j].as=NULL;
						coremap[j].chunk_allocated=0;
						coremap[j].time=0;
					}
				}
				break;
			}
		}
	}
	spinlock_release(&coremap_lock);


	//(void)addr;
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	int i, spl;
	spl = splhigh();

	i = tlb_probe(ts->ts_vaddr, 0);
	if(i>0){
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{

	//Variable Declaration
	struct addrspace *as;
	vaddr_t stackbase, stacktop;
	paddr_t paddr=0;

	int permissions=0;
	//For calling page alloc
	bool address_found=false;

	//For TLB
	int i;
	uint32_t ehi, elo;
	int spl;
	//End of variable declaration

	switch(faulttype)
	{
		case VM_FAULT_READONLY:
			panic("HAVE TO DO IT PLEASE WAITdumbvm: got VM_FAULT_READONLY\n");
		case VM_FAULT_READ:
		case VM_FAULT_WRITE:
		break;

		default:
		return EINVAL;
	}

	as = curthread->t_addrspace;
	if (as == NULL)
	{
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		*/
		return EFAULT;

	}

	//INCLUDE STACK BASE AND TOP CHECKS LATER

//	 Assert that the address space has been set up properly.
//	KASSERT(as->heap_start != 0);
//	KASSERT(as->heap_end != 0);
//	KASSERT(as->stackbase_top != 0);
//	KASSERT(as->stackbase_base != 0);

	//Check whether addrspace variables are aligned properly
	KASSERT((as->heap_start & PAGE_FRAME) == as->heap_start);

//	KASSERT((as->stackbase_top & PAGE_FRAME) == as->stackbase_top);
//	KASSERT((as->stackbase_base & PAGE_FRAME) == as->stackbase_end);

//	Check the regions start and end by iterating through the regions

	struct addr_regions *head;

	head = as->regions;
	if(as->regions!=NULL)
	{
		//Iterate over the regions
		while(as->regions !=NULL)
		{
			KASSERT(as->regions->va_start != 0);
			KASSERT(as->regions->region_numpages != 0);

			//Check whether the regions are aligned properly
			KASSERT((as->regions->va_start & PAGE_FRAME) == as->regions->va_start);

			//Iterate to the next region
			as->regions=as->regions->next_region;
		}

		//Assign the region back to head
		as->regions = head;
	}

	//End of checking the regions

	//End of checking the addrspace setup

	//Align the fault address
	faultaddress &= PAGE_FRAME;


	stackbase = USERSTACK - VM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;


	/*
	 * Check which region or stack or heap does the fault address lies in
	 * Assign vbase1 and vtop1 based on those findings
	*/

	while(!(address_found))
	{
			//First check within the stack bases
			if(faultaddress >= stackbase && faultaddress < stacktop)
			{

				//Means faultaddress lies in stackpage
				paddr = handle_address(faultaddress,permissions,as,faulttype);
				if(paddr>0)
				{
					address_found=true;
					break;
				}
				else
					return EFAULT;
			}
			else if(faultaddress >= as->heap_start && faultaddress < as->heap_end)
			{
				//meaning lies in the heap region
				paddr = handle_address(faultaddress,permissions,as,faulttype);
				if(paddr>0)
				{
					address_found=true;
					break;
				}
				else
					return EFAULT;

			}
			else
			{
				//Now Iterate over the regions and check whether it exists in one of the region
				while(as->regions !=NULL)
				{
					if(faultaddress >= as->regions->va_start && faultaddress < as->regions->va_end)
					{
						paddr = handle_address(faultaddress,as->regions->set_permissions,as,faulttype);
						if(paddr>0)
						{
							//mark found as true
							address_found=true;

							break;
						}
						else
						{
							//Assign the region back to head
							as->regions = head;
							return EFAULT;
						}

					}

					//Iterate to the next region
					as->regions=as->regions->next_region;
				}

				//Assign the region back to head
				as->regions = head;
			}//End of else checking if it exists in the region

			if(!address_found)
			{
				//meaning fault address not found in any of the regions
				return EFAULT;
			}


	}//End of while checking whether address found for the fault address


	KASSERT((paddr & PAGE_FRAME) == paddr);

//	 Disable interrupts on this CPU while frobbing the TLB.
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
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

	tlb_random(ehi,elo);
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
	return 0;


	for (i=0; i<NUM_TLB; i++)
	{
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID)
		{
			continue;
		}

		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}


	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
		splx(spl);
		return EFAULT;

	return ENOMEM;
}


/**
 * Author; Pratham Malik
 * Function to handle the fault address and assign pages and update the coremap entries
 * and page table entries.
 * Each condition detail is given inside the function
 */

paddr_t
handle_address(vaddr_t faultaddr,int permissions,struct addrspace *as,int faulttype)
{
	paddr_t pa=0;
	/**
	 * First check if the address space is not null and if page table entry exists
	 */

	if(as->page_table==NULL)
	{
			/**
			 * Meaning that there are no entries for the page table as of now
			 * In such case -- Allocate space for the page table entry(Take page table lock) and DO
			 * 1. find index where it can be mapped by calling -- alloc_upages (take coremap lock)
			 * 2. Map the pa to va in the page entry and update coremap index too
			 * 3. release pagetable and coremap lock and assign the pa
			 */

			lock_acquire(as->lock_page_table);

			as->page_table = (struct page_table_entry *) kmalloc(sizeof(struct page_table_entry));

			//Take the coremap lock and find an index to map the entry
			spinlock_acquire(&coremap_lock);

			int index = alloc_upages();

			//Now decide what to do based on the current page status at the coremap[index]

			if(coremap[index].page_status==0)
			{
				//Means the page was free -- NO SWAP OUT OR ANYTHING ELSE REQUIRED

				//update the page table entries at that index
				as->page_table->pa = coremap[index].ce_paddr;
				as->page_table->permissions =permissions;
				as->page_table->present=1;
				as->page_table->va=faultaddr;
				as->page_table->next=NULL;

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);

				return pa;

			}
			else if(coremap[index].page_status==2)
			{
				/**
				 * Means page was dirty -- swap out the previous page to swap file
				 * And change entries for the current page
				 */

				int swap_index;
				swap_index = swapout_change_coremap_entry(index);
				lock_acquire(swap_lock);
					vaddr_t tlb_vaddr= swap_info[swap_index]->va;
				lock_release(swap_lock);
				//update the page table entries at that index
				as->page_table->pa = coremap[index].ce_paddr;
				as->page_table->permissions =permissions;
				as->page_table->present=1;
				as->page_table->va=faultaddr;
				as->page_table->next=NULL;


				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);
				my_tlb_shhotdown(tlb_vaddr);
				write_page(coremap[index].ce_paddr,swap_index);

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				return pa;

			}
			else if(coremap[index].page_status==3)
			{
				//Means the current mapped page was clean -- EVICT the Page
				evict_coremap_entry(index);

				//update the page table entries at that index
				as->page_table->pa = coremap[index].ce_paddr;
				as->page_table->permissions =permissions;
				as->page_table->present=1;
				as->page_table->va=faultaddr;
				as->page_table->next=NULL;

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);

				return pa;
			}
			return pa;
	}
	else if(as->page_table!=NULL)
	{
		/**
		 * Meaning entries exist in the page table of addrspace
		 * First check if the fault address entry exists in the page table entries
		 * 1. Take the pagetable lock and iterate over the page map entries
		 * 2. if entry found -- Do
		 * 	  2.1 Check if present bit is 1 -- Then just assign the entry and return the pa mapped to the entry
		 * 	  2.2 If present bit is 0 -- Then take coremap lock and find an index where the entry can be mapped by calling alloc_upages
		 * 	      map the entry at the index and update the coremap index too
		 * 3. Release the coremap lock and page table lock
		 */

		lock_acquire(as->lock_page_table);

		struct page_table_entry *head;
		head= as->page_table;

		//Iterate over the page table entries

		while(as->page_table !=NULL)
		{
			//Check if faultaddress has been mapped till now
			if(as->page_table->va == faultaddr)
			{
				//Mapping exists - Now check if the present bit for the mapping is true or false
				if(as->page_table->present == 1)
				{
					//Take the coremap lock and find an index to map the entry
					spinlock_acquire(&coremap_lock);

					//present bit is true -- means it is present in memory
					pa = as->page_table->pa;
					int coremap_entry_index;
					coremap_entry_index = ((pa- coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;

					//Change the page status to dirty if faulttype is write
					switch (faulttype)
					{
						case VM_FAULT_WRITE:
							coremap[coremap_entry_index].page_status=2;
						break;
					}
					//Re-assign back the head
					as->page_table = head;


					spinlock_release(&coremap_lock);
					lock_release(as->lock_page_table);
					return pa;

				}
				else if(as->page_table->present == 0)
				{
					//Meaning that the page has been swapped out currently -- Read from file to SWAP BACK IN

					spinlock_acquire(&coremap_lock);

					//Get a new page
					int index = alloc_upages();

					//Check and perform operation as per the current page status at coremap[index]

					if(coremap[index].page_status==0)
					{
						//Means page at index is currently free

						//update the page table entries at that index
						as->page_table->pa = coremap[index].ce_paddr;
						as->page_table->permissions=permissions;
						as->page_table->present=1;
						as->page_table->va=faultaddr;

						//Zero the region
						as_zero_region(coremap[index].ce_paddr,1);

						//Getting time
						time_t seconds;
						uint32_t nanoseconds;
						gettime(&seconds, &nanoseconds);

						//Update the coremap entries
						coremap[index].as=as;
						coremap[index].chunk_allocated=0;
						coremap[index].time=seconds+nanoseconds;

						pa = coremap[index].ce_paddr;

						//Decide page status as per faulttype
						switch (faulttype)
						{
							case VM_FAULT_READ:
								coremap[index].page_status=3;
								break;

							case VM_FAULT_WRITE:
								coremap[index].page_status=2;
								break;

						}

						int swap_index= as->page_table->swapfile_index;

						//Re-assign back the head
						as->page_table = head;

						spinlock_release(&coremap_lock);
						lock_release(as->lock_page_table);

						//Zero the region
						as_zero_region(coremap[index].ce_paddr,1);

						//Swap in the page from the swapfile
						read_page(pa,swap_index);

						return pa;


					}
					else if(coremap[index].page_status == 2)
					{
						/**
						 * Means page was dirty -- swap out the previous page to swap file
						 * And change entries for the current page
						 * Then swap in the new page
						 */

						int swap_index;
						//Called swapout_change_coremap_entry(index) to change the present bit of the page at
						//that index of coremap to 0 signifying that the page has been swapped out
						//Return the swap_index of file where we are supposed to write the file

						swap_index = swapout_change_coremap_entry(index);
						lock_acquire(swap_lock);
							vaddr_t tlb_vaddr= swap_info[swap_index]->va;
						lock_release(swap_lock);
						//update the page table entries at that index
						as->page_table->pa = coremap[index].ce_paddr;
						as->page_table->permissions=permissions;
						as->page_table->present=1;
						as->page_table->va=faultaddr;

						//Getting time
						time_t seconds;
						uint32_t nanoseconds;
						gettime(&seconds, &nanoseconds);

						//Update the coremap entries
						coremap[index].as=as;
						coremap[index].chunk_allocated=0;
						coremap[index].time=seconds+nanoseconds;

						pa = coremap[index].ce_paddr;

						int new_swap_index = as->page_table->swapfile_index;

						//Decide page status as per faulttype
						switch (faulttype)
						{
							case VM_FAULT_READ:
								coremap[index].page_status=3;
								break;

							case VM_FAULT_WRITE:
								coremap[index].page_status=2;
								break;

						}

						//Re-assign back the head
						as->page_table = head;

						//Release the coremap lock
						spinlock_release(&coremap_lock);

						//Release the page table lock
						lock_release(as->lock_page_table);

						//Now swap out the previous entry and swap in the new entry
						my_tlb_shhotdown(tlb_vaddr);
						write_page(coremap[index].ce_paddr,swap_index);

						//Zero the region
						as_zero_region(coremap[index].ce_paddr,1);

						//Swap in the page from the swapfile
						read_page(pa,new_swap_index);

						return pa;

					}
					else if(coremap[index].page_status==3)
					{
						//Means page at index is currently clean -- need to be evicted
						//Means the current mapped page was clean -- EVICT the Page

						evict_coremap_entry(index);

						//update the page table entries at that index
						as->page_table->pa = coremap[index].ce_paddr;
						as->page_table->permissions=permissions;
						as->page_table->present=1;
						as->page_table->va=faultaddr;

						//Getting time
						time_t seconds;
						uint32_t nanoseconds;
						gettime(&seconds, &nanoseconds);

						//Update the coremap entries
						coremap[index].as=as;
						coremap[index].chunk_allocated=0;
						coremap[index].time=seconds+nanoseconds;

						pa = coremap[index].ce_paddr;

						//Decide page status as per faulttype
						switch (faulttype)
						{
							case VM_FAULT_READ:
								coremap[index].page_status=3;
								break;

							case VM_FAULT_WRITE:
								coremap[index].page_status=2;
								break;

						}


						int swap_index;
						swap_index= as->page_table->swapfile_index;

						//Re-assign back the head
						as->page_table = head;

						//Release the coremap lock
						spinlock_release(&coremap_lock);
						//Release the page table lock
						lock_release(as->lock_page_table);

						//Zero the region
						as_zero_region(coremap[index].ce_paddr,1);


						//Swap in the page from the swapfile
						read_page(pa,swap_index);

						return pa;

					}
				}
			}

			//Move to next entry in the page table link list
			as->page_table = as->page_table->next;

		} // End of while iterating over the page table entries

		//Re-assign back the head
		as->page_table = head;

		if(pa==0)
		{
			//Meaning no page has been mapped till now for the fault address that is why pa has not been assigned till now
			struct page_table_entry *entry = (struct page_table_entry *) kmalloc(sizeof(struct page_table_entry));

			//Take the coremap lock and find an index to map the entry
			spinlock_acquire(&coremap_lock);

			int index = alloc_upages();

			//Do operations as per the index
			if(coremap[index].page_status==0)
			{
				//Meaning a free page
				//update the page table entries at that index
				entry->pa = coremap[index].ce_paddr;
				entry->permissions =permissions;
				entry->present=1;
				entry->va=faultaddr;

				entry->next=head;

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				/*add the entry created to the head of the page table list*/
				as->page_table= entry;

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);

				return pa;

			}
			else if(coremap[index].page_status==2)
			{
				//Meaning a swapped out page
				int swap_index;
				//Called swapout_change_coremap_entry(index) to change the present bit of the page at
				//that index of coremap to 0 signifying that the page has been swapped out
				//Return the swap_index of file where we are supposed to write the file
				swap_index = swapout_change_coremap_entry(index);
				lock_acquire(swap_lock);
					vaddr_t tlb_vaddr= swap_info[swap_index]->va;
				lock_release(swap_lock);
				//update the page table entries at that index
				entry->pa = coremap[index].ce_paddr;
				entry->permissions =permissions;
				entry->present=1;
				entry->va=faultaddr;

				entry->next=head;

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				/*add the entry created to the head of the page table list*/
				as->page_table= entry;

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);

				//Swapout the old page
				my_tlb_shhotdown(tlb_vaddr);
				write_page(coremap[index].ce_paddr,swap_index);

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				return pa;



			}
			else if(coremap[index].page_status==3)
			{
				//Meaning a clean page
				evict_coremap_entry(index);

				//update the page table entries at that index
				entry->pa = coremap[index].ce_paddr;
				entry->permissions =permissions;
				entry->present=1;
				entry->va=faultaddr;

				entry->next=head;

				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				//Update the coremap entries
				coremap[index].as=as;
				coremap[index].chunk_allocated=0;
				coremap[index].page_status=2;
				coremap[index].time=seconds+nanoseconds;

				pa = coremap[index].ce_paddr;

				/*add the entry created to the head of the page table list*/
				as->page_table= entry;

				//Zero the region
				as_zero_region(coremap[index].ce_paddr,1);

				//Release the coremap lock
				spinlock_release(&coremap_lock);
				//Release the page table lock
				lock_release(as->lock_page_table);

				return pa;

			}




		}////Meaning no page has been mapped till now for the fault address that is why pa has not been assigned till now



	}//End of IF checking whether the page table entries is not NULL



	return pa;
}

/**
 * Author: Pratham Malik
 *Call alloc for user pages
 *Returns the index of the coremap entry
 *Any function calling alloc_upages should be having the coremap lock
 */

int
alloc_upages()
{
	int counter=0;
	int index=-1;
	bool page_found=false;
	//Acquire the lock and find a free page

	while(!(page_found))
	{
		for(counter=coremap_pages;counter<total_systempages;counter++)
		{
			if(coremap[counter].page_status==0)
			{
				//Means found the page with status as free
				index=counter;
				page_found=true;

				break;
			}
		}

		/**
		 * Means no page found which is free --
		 * FIND To be EVICTED PAGE - Call find_oldest_page to find the (oldest and clean page) or (just the oldest)in the coremap
		 * Check whether the index returned is returning what kind of page -- clean or dirty or swapped
		 */

		if(!(page_found))
		{
			index = find_oldest_page();

	//			change_page_entry(index);


			page_found = true;
		}


	}//End of while loop in page_found

	return index;

}

/**
 * Author: Pratham Malik
 * Function to normally evict the coremap entry at a particular index
 * This is done only to clean pages -- hence no need to save the page in memory
 * Iterate the page table at that index and change its present bit to 0
 * NOTE: Currently we assume that the coremap lock is being acquired before this function is called
 */

void
evict_coremap_entry(int index)
{
	//Take the page table lock
	lock_acquire(coremap[index].as->lock_page_table);

	struct page_table_entry *head;

	if(coremap[index].as!=NULL)
	{
		head= coremap[index].as->page_table;
		//Iterate over the page table entries

		while(coremap[index].as !=NULL)
		{
			if(coremap[index].ce_paddr == coremap[index].as->page_table->pa)
			{
				//Entry found where pa has been mapped -- Change to 0
				coremap[index].as->page_table->present=0;
				break;
			}

			coremap[index].as->page_table = coremap[index].as->page_table->next;

		}
		coremap[index].as->page_table = head;
	}

	//Release page table lock
	lock_release(coremap[index].as->lock_page_table);

}


/**
 * Author: Pratham Malik
 * Function to find and return the index in the coremap
 * Currently being used by alloc_kpages
 */
int
find_page_available(int npages)
{
	int return_index=-1;
	int counter=0;
	bool found_status=false;

	//Check whether npages is 1 or more
	if(npages==1)
	{
		while(!(found_status))
		{
			//First check for free pages
			for(counter=coremap_pages;counter<total_systempages;counter++)
			{
				if(coremap[counter].page_status==0)
				{
					//Page is free
					return_index=counter;
					found_status=true;
					break;
				}
			} //End of for loop for checking for free page

			if(!(found_status))
			{
				//Meaning no page found till now
				//Call function to find the earliest non-kernel page and return its index
				//panic("No free Kpage found\n");
				return_index = find_oldest_page();
				found_status=true;

			}
		}

	}//End of IF checking whether number of pages requested is one or more
	else if(npages >1)
	{
		/**
		 * Means the number of pages requested is more than 1
		 * As per current understanding this can be called only through kernel
		 */
		return_index = find_npages(npages);

	}//End of else if checking whether the number of pages requested is more than 1

	return return_index;
}

/**
 * Author: Pratham Malik
 * The function find_oldest_page checks oldest iterating over the coremap entries
 * It checks whether the page:
 *  1. Is not a kernel page
 *  2. Is the oldest and cleanest as per timestamp
 */

int
find_oldest_page()
{
	int counter=0;

	//Initialize the time and index as the entry for the first system page
	int index_old=coremap_pages;
	int32_t time_old=coremap[index_old].time;

/*
	int index_clean=0;
	int32_t time_clean=0;
*/

	//Run algorithm to over coremap entries
	for(counter=coremap_pages+1;counter<total_systempages;counter++)
	{
		//Check if the page is not kernel page
		if( (coremap[counter].page_status !=1) )
		{
/*
			if(coremap[counter].page_status == 3)
			{
				//Meaning page is clean
				if(index_clean==0)
				{
					//Means first time in clean -- Initiate the clean variables
					index_clean=counter;
					time_clean=coremap[counter].time;
				}
				else
				{
					//Compare the time
					if(time_clean>coremap[counter].time)
					{//Assign the time and index
						index_clean=counter;
						time_clean=coremap[counter].time;
					}
				}
			}//End of checking page is clean
*/

			//Means page status is not fixed and address space is not same as current thread address space
			if(time_old>coremap[counter].time)
			{
				//Means coremap entry time is less than the earlier time
				time_old=coremap[counter].time;
				index_old= counter;
			}
		}//End of checking if page is not fixed
	}

/*
	if(index_clean!=0)
		return index_clean;
	else
*/
		return index_old;
}

/**
 * Author: Pratham Malik
 * The function find_npages finds the continuous set of pages iterating over the coremap entries
 * It checks whether the page:
 *  1. Is not a kernel page
 */

int
find_npages(int npages)
{
	int32_t startindex=-1;
	bool found_range= false;
	int32_t count=0;

	//While code for checking it it is free
	while(!found_range)
	{
		for(int i=startindex+1;i<total_systempages;i++)
		{
			if(coremap[i].page_status==0)
			{
				//Meaning that page at index 'i' is free
				//CHeck till npages if it is free
				for(int j=i;j< i+ npages;j++)
				{
					if(coremap[j].page_status == 0)
					{
						count++;
					}
					else
						break;
				}//End of for looping from i till npages

				//Check if count is equal to npages
				if(count==npages)
				{
					//Means contiguous n pages found
					startindex=i;
					found_range=true;
				}
				else
				{
					count=0;

				}

			}// End of if checking whether page is free
		}// End of for loop over all the pages
	}//end of main while

/*
	if(!found_range)
	{
		//panic("Couldnt find the pages\n");
	}
*/
	startindex=-1;
	found_range= false;
	count=0;

	while(!found_range)
	{
		for(int i=startindex+1;i<total_systempages;i++)
		{
			if(coremap[i].page_status==0 || coremap[i].page_status!=1)
			{
				//Meaning that page at index 'i' is free
				//CHeck till npages if it is free
				for(int j=i;j< i+ npages;j++)
				{
					if(coremap[j].page_status == 0 || coremap[j].page_status!=1)
					{
						count++;
					}
					else
						break;
				}//End of for looping from i till npages

				//Check if count is equal to npages
				if(count==npages)
				{
					//Means contiguous n pages found
					startindex=i;
					found_range=true;
				}
				else
				{
					count=0;

				}
			}// End of if checking whether page is free
		}// End of for loop over all the pages
	}//end of main while


	return startindex;
}



/**
 * Author: Pratham Malik
 * The function change_page_entry checks the coremap at index received
 * iterates over page table entries for the page available there
 * and call swapout or just evict
 * and then change the present bit in page table entry for which pa == pa of coremap entry at index
 * THINGS TO NOTE: THIS IS ONLY CALLED IF FREE PAGE IS NOT FOUND
 */

/*void
change_page_entry(int index)
{
	//Check the current status of the page at coremap index
	if(coremap[index].page_status==3)
	{
		//Call Evict for the page
		evict_coremap_entry(index);
	}
	else if(coremap[index].page_status==2)
	{
		//Swap out the page
		swapout_coremap_entry(index);
	}
}*/

void
write_page(paddr_t pa, int index)
{

	char k_des[NAME_MAX];
	memcpy(k_des,SWAP_FILE,NAME_MAX);

	//struct vnode *v;
	int result;
	/*result = vfs_open(k_des,O_RDWR , 0, &v);
	if (result) {

		panic("Not able to open the SWAP FILE");
	}*/
	lock_acquire(swap_file_lock);
	struct iovec iov;
	struct uio uio;

	uio_kinit(&iov, &uio, (void*)PADDR_TO_KVADDR(pa), PAGE_SIZE, (index-1)*PAGE_SIZE, UIO_WRITE);

	result= VOP_WRITE(swapfile_vnode, &uio);
	if(result){
		lock_release(swap_file_lock);
		panic("Not able to write to SWAP FILE");
	}
	lock_release(swap_file_lock);

}

void
read_page(paddr_t pa, int index)
{

	char k_des[NAME_MAX];
	memcpy(k_des, SWAP_FILE,NAME_MAX);

	//struct vnode *v;
	int result;
	/*result = vfs_open(k_des,O_RDWR , 0, &v);
	if (result) {

		panic("Not able to open the SWAP FILE");
	}*/
	lock_acquire(swap_file_lock);
	struct iovec iov;
	struct uio uio;

	uio_kinit(&iov, &uio, (void*)PADDR_TO_KVADDR(pa), PAGE_SIZE, index*PAGE_SIZE, UIO_READ);

	result= VOP_READ(swapfile_vnode, &uio);
	if(result){
		lock_release(swap_file_lock);
		panic("Not able to read from SWAP FILE");
	}
	lock_release(swap_file_lock);
}

int
make_swap_file()
{
	int result;

	swap_file_lock= lock_create("swapfile_lock");
	char k_des[NAME_MAX];
	memcpy(k_des, SWAP_FILE,NAME_MAX);


	result = vfs_open(k_des,O_CREAT, 0, &swapfile_vnode);

	if (result) {
		lock_release(swap_file_lock);
		return result;
	}

	swap_lock = lock_create("swap_lock");

	struct swap_elements *info;
	for(result=0;result<SWAP_MAX;result++)
	{
		info = kmalloc(sizeof(struct swap_elements));

		info->addr=0;
		info->va=0;
		swap_info[result]=info;

	}

return 0;
}

/**
 * Author: Pratham Malik
 * Function to change the page status if swapping out the coremap entry
 * This is done only to dirty pages -- before saving the page in memory
 * Iterate the page table at that index and change its present bit to 0
 * NOTE: Currently we assume that the coremap lock is being acquired before this function is called
 * It will return the swap_file index of the page being decided to swap out
 */
//TODO:::: Change the logic as to what to do in order to find the swap entry
int
swapout_change_coremap_entry(int index)
{
	int swap_index=-1;
	//Take the page table lock
	lock_acquire(coremap[index].as->lock_page_table);

	struct page_table_entry *head;

	if(coremap[index].as->page_table!=NULL)
	{
		head= coremap[index].as->page_table;
		//Iterate over the page table entries

		while(coremap[index].as->page_table !=NULL)
		{
			if(coremap[index].ce_paddr == coremap[index].as->page_table->pa)
			{
				//Entry found where pa has been mapped -- Change to 0
				coremap[index].as->page_table->present=0;

				//Call function to write this to the swap file
				swap_index = find_swapfile_entry(coremap[index].as,coremap[index].as->page_table->va);

				//Write back the index to the page table entry
				coremap[index].as->page_table->swapfile_index = swap_index;
				break;
			}

			coremap[index].as->page_table = coremap[index].as->page_table->next;

		} //While ends

		coremap[index].as->page_table = head;
	}

	//Release page table lock
	lock_release(coremap[index].as->lock_page_table);

	return swap_index;
}

int
find_swapfile_entry(struct addrspace *as,vaddr_t va)
{
	int counter=0;
	int index=-1;

	//Take swapfile lock
	lock_acquire(swap_lock);


	//Scan the swapfile array
	for(counter=1;counter<SWAP_MAX;counter++)
	{
		if((swap_info[counter]->va==0))
			continue;
		else if((swap_info[counter]->addr == as) && (swap_info[counter]->va == va))
		{
			index=counter;
			break;
		}
	}
	if(index==-1)
	{
		//Means no entry found -- Allocate an entry for the same
		for(counter=1;counter<SWAP_MAX;counter++)
		{
			if((swap_info[counter]->va == 0))
			{
				index=counter;
				break;
			}
		}
	}
	swap_info[index]->addr= as;
	swap_info[index]->va= va;
	lock_release(swap_lock);


	return index;
}


