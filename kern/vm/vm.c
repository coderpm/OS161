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
#include <cpu.h>

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

struct lock *vm_fault_lock;

unsigned int swap_bit; // 0 means No Write , 1 means Yes Write
struct cv *cv_swap;
struct lock *ascopy_lock;

struct spinlock tlb_lock1;
struct spinlock tlb_lock2;
struct spinlock tlb_lock3;
struct spinlock tlb_lock4;

/*
DECLARRAY(cpu);
DEFARRAY(cpu, no inline);
static struct cpuarray allcpus;
*/



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

	vm_fault_lock = lock_create("vm_fault_lock");
	ascopy_lock = lock_create("as_copy_lock");


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
		coremap[i].locked=0;
	}

	/*for(int i=num_coremapPages;i<total_page_num;i++)
	{
		kprintf("COremap index %d with pa %d \n",i,coremap[i].ce_paddr);
	}
*/

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


		if(npages==1)
		{
			spinlock_acquire(&coremap_lock);
			//Call function to find the available page
			int index = find_page_available(npages);

			coremap[index].locked=1;

			spinlock_release(&coremap_lock);

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
				coremap[index].time= nanoseconds;
				coremap[index].locked=0;

				return va;
			}
			else
			{
				/**
				 * Meaning that coremap entry at index has to be evicted
				 * Call function to Find the page table entry marked at address and change the page table present bit to 0
				 */

				//Get the index of the the page which has to be swapped out

				//Call change coremap page entry in order to make the page available for you

				change_coremap_page_entry(index);


				va = PADDR_TO_KVADDR(coremap[index].ce_paddr);
				pa = coremap[index].ce_paddr;
				//Getting time
				time_t seconds;
				uint32_t nanoseconds;
				gettime(&seconds, &nanoseconds);

				coremap[index].chunk_allocated=1;
				coremap[index].as=curthread->t_addrspace;
				coremap[index].page_status=1;
				coremap[index].time= nanoseconds;



				as_zero_region(coremap[index].ce_paddr,npages);

				coremap[index].locked=1;

				return va;

			}

		}
		else if(npages >1)
		{
		//	panic("CHANGE PAGE ENTRY DOES NOT WORK HERE IN NPAGES>1");
			spinlock_acquire(&coremap_lock);
			int index = find_page_available(npages);

			spinlock_release(&coremap_lock);

			if(index<0)
				pa=0;

			else
			{
				//Meaning pages found to replace
				//As the pages are contiguous -- Iterate over them and change page entries one by one
				for(int i =index;i<index+npages;i++)
				{
					change_coremap_page_entry(i);

					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					coremap[i].page_status=1;
					coremap[i].chunk_allocated=0;
					coremap[i].as=curthread->t_addrspace;
					coremap[index].time= nanoseconds;
				}

				coremap[index].chunk_allocated=npages;
				as_zero_region(coremap[index].ce_paddr,npages);

			}
		} //End of if checking whether npages more than 1

		//Return the physical address retrieved from the coremap
		return va;
	}
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
				if(coremap[counter].page_status==0 && coremap[counter].locked==0)
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

	if(return_index == -1)
		panic("Wrong index returned in find_page_available");

	return return_index;
}

/**
 * Function to find available page entry to map the page va
 */

int
find_available_page()
{
	int counter=0;
	int index=-1;
	bool page_found=false;


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
		 */

		if(!(page_found))
		{
			index = find_oldest_page();
			page_found = true;
		}

	}//End of while loop in page_found

	if(index==-1)
		panic("Index returning as -1 in find_page_available");
	return index;

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
	int index_old=-1;
	unsigned int time_old=0;


	for(counter = 0;counter<total_systempages;counter++)
	{
		if(coremap[counter].page_status !=1)
		{
			if(coremap[counter].locked !=3)
			{
				if(index_old == -1)
				{
					//Means the first time
					index_old=counter;
					time_old=coremap[counter].time;
					continue;
				}
				else
				{
					if(time_old>coremap[counter].time)
					{
						time_old=coremap[counter].time;
						index_old=counter;
					}
				}
			}
		}
	}

	if(index_old==-1)
		panic("No INDEX FOUND FOR OLDEST");


	return index_old;
}




void
page_free(paddr_t paddr){
	spinlock_acquire(&coremap_lock);


	int coremap_entr= (( paddr- coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;
	as_zero_region(coremap[coremap_entr].ce_paddr,1);

	if(coremap[coremap_entr].chunk_allocated==0)
	{
		coremap[coremap_entr].page_status=0;
		coremap[coremap_entr].time=0;
		coremap[coremap_entr].as=NULL;
		coremap[coremap_entr].chunk_allocated=0;
		coremap[coremap_entr].locked=0;
	}
	else{
		for(int j=0; j< coremap[coremap_entr].chunk_allocated; j++){
			coremap[j].page_status=0;
			coremap[j].time=0;
			coremap[j].as=NULL;
			coremap[j].chunk_allocated=0;
			coremap[j].locked=0;
		}
	}

	spinlock_release(&coremap_lock);

	/*old = curthread->t_addrspace->page_table;

	while(curthread->t_addrspace->page_table != NULL)
	{
		int ind;
		ind = ((curthread->t_addrspace->page_table->pa - coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;

		if(coremap[ind].as != curthread->t_addrspace)
			panic("ADDRESS SPACE DO NOT MATCH IN after page free");



		curthread->t_addrspace->page_table = curthread->t_addrspace->page_table->next;
	}

	curthread->t_addrspace->page_table = old;
*/
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
					coremap[i].locked=0;
				}
				else{
					as_zero_region(coremap[i].ce_paddr, chunkzise);
					for(int j=i; j<i+chunkzise; j++){
						coremap[j].page_status=0;
						coremap[j].as=NULL;
						coremap[j].chunk_allocated=0;
						coremap[j].time=0;
						coremap[j].locked=0;
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
	panic("dumbvmXXXXXXXX tried to do tlb shootdown?!\n");
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

	lock_acquire(vm_fault_lock);
	//curthread->t_in_interrupt=false;




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
				if(paddr <1)
					panic("Wrong paddr returned");

				if(paddr>0)
				{
					address_found=true;
					break;
				}
				else
				{
					lock_release(vm_fault_lock);
					return EFAULT;
				}

			}
			else if(faultaddress >= as->heap_start && faultaddress < as->heap_end)
			{
				//meaning lies in the heap region
				paddr = handle_address(faultaddress,permissions,as,faulttype);
				if(paddr <1)
					panic("Wrong paddr returned");

				if(paddr>0)
				{
					address_found=true;
					break;
				}
				else
				{
						lock_release(vm_fault_lock);
						return EFAULT;
					}

			}
			else
			{
				//Now Iterate over the regions and check whether it exists in one of the region
				while(as->regions !=NULL)
				{
					if(faultaddress >= as->regions->va_start && faultaddress < as->regions->va_end)
					{
						paddr = handle_address(faultaddress,as->regions->set_permissions,as,faulttype);
						if(paddr <1)
							panic("Wrong paddr returned");
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
							lock_release(vm_fault_lock);
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
					lock_release(vm_fault_lock);
						return EFAULT;
			}


	}//End of while checking whether address found for the fault address

	KASSERT((paddr & PAGE_FRAME) == paddr);

	//TODO:::
	int coremap_entry_index;

	coremap_entry_index = ((paddr- coremap[coremap_pages].ce_paddr)/PAGE_SIZE)+ coremap_pages;

	coremap[coremap_entry_index].locked=0;

	free_coremap_locked(paddr);

	check_coremap(0);



	lock_release(vm_fault_lock);


	spl = splhigh();

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

//	 Disable interrupts on this CPU while frobbing the TLB.
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

	tlb_random(ehi,elo);

	splx(spl);
	return 0;




	kprintf("MY vm: Ran out of TLB entries - cannot handle page fault\n");
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
		* In such case -- Allocate space for the page table entry and DO
		* 1. find index where it can be mapped by calling -- find_available_page (take coremap lock)
		* 2. Map the pa to va in the page entry and update coremap index too
		*/


		as->page_table = (struct page_table_entry *) kmalloc(sizeof(struct page_table_entry));
		as->num_pages= as->num_pages+1;
		//Take the coremap lock and find an index to map the entry
		spinlock_acquire(&coremap_lock);

		int index;
		index = find_available_page();

		coremap[index].locked=1;

		spinlock_release(&coremap_lock);

		//Call change coremap page entry in order to make the page available for you
		change_coremap_page_entry(index);

		//Now that particular entry at coremap[index] is free for you

		//update the page table entries at that index
		as->page_table->pa = coremap[index].ce_paddr;
		as->page_table->permissions =permissions;
		as->page_table->present=1;
		as->page_table->va=faultaddr;
		as->page_table->next=NULL;

		as->pagelist_head = as->page_table;

		//Getting time
		time_t seconds;
		uint32_t nanoseconds;
		gettime(&seconds, &nanoseconds);

		//Update the coremap entries
		coremap[index].as=as;
		coremap[index].chunk_allocated=0;
		coremap[index].page_status=2;
		coremap[index].time=nanoseconds;

		pa = coremap[index].ce_paddr;

		//Zero the region
		as_zero_region(coremap[index].ce_paddr,1);

		check_coremap(1);

		return pa;

	} //End of checking if addrspace == NULL
	else if(as->page_table !=NULL)
	{
		struct page_table_entry *iter;
		iter = as->pagelist_head;

		while(iter != NULL)
		{
			if(iter->va == faultaddr)
			{
				if(iter->present == 1)
				{
					spinlock_acquire(&coremap_lock);

					//present bit is true -- means it is present in memory
					pa = iter->pa;

					int index;
					index = find_index(pa);

					if(pa != coremap[index].ce_paddr)
						panic("WRONG PA IN PRESENT == 1 , PA IS %d",pa);
					if(iter->pa != coremap[index].ce_paddr)
						panic("COremap PA DOES NOT MATCH IN PRESENT == 1");

					//Change the page status to dirty if faulttype is write
					switch (faulttype)
					{
						case VM_FAULT_WRITE:
							coremap[index].page_status=2;
						break;
					}




					spinlock_release(&coremap_lock);
					check_addrspace(as);


					return pa;

				}//End of IF checking present == 1
				else
				{
					//Meaning present == 0
					//Meaning that the page has been swapped out currently -- Read from file to SWAP BACK IN

					//Take the coremap lock and find an index to map the entry
					spinlock_acquire(&coremap_lock);

					int index;
					index = find_available_page();

					coremap[index].locked=1;

					spinlock_release(&coremap_lock);

					//Call change coremap page entry in order to make the page available for you
					change_coremap_page_entry(index);

					//Now that particular entry at coremap[index] is free for you -- update the page table entries at that index
					iter->pa = coremap[index].ce_paddr;
					iter->permissions=permissions;
					iter->present=1;
					iter->va=faultaddr;

					//Getting time
					time_t seconds;
					uint32_t nanoseconds;
					gettime(&seconds, &nanoseconds);

					//Update the coremap entries
					coremap[index].as=as;
					coremap[index].chunk_allocated=0;
					coremap[index].time=nanoseconds;

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

					int swapin_index= iter->swapfile_index;

					if(coremap[index].as->page_table != coremap[index].as->pagelist_head)
						panic("Head does not match with page table head in PRESENT==0");

					if(as->page_table != as->pagelist_head)
						panic("Head does not match with page table head in PRESENT==0 while checking the address");

					//Zero the region
					as_zero_region(coremap[index].ce_paddr,1);

					//Swap in the page from the swapfile
					swapin_page(pa,swapin_index);

					check_addrspace(as);


					return pa;

				}

			}//End of IF checking that va is the faultaddr
			iter = iter->next;
		} // End of while iterating over page_table entries
		if(pa==0)
		{

			//Meaning no page has been mapped till now for the fault address that is why pa has not been assigned till now
			struct page_table_entry *entry = (struct page_table_entry *) kmalloc(sizeof(struct page_table_entry));
			as->num_pages= as->num_pages+1;



			//Take the coremap lock and find an index to map the entry
			spinlock_acquire(&coremap_lock);

			int index;
			index = find_available_page();

			spinlock_release(&coremap_lock);

			//Call change coremap page entry in order to make the page available for you
			change_coremap_page_entry(index);
			//Now that particular entry at coremap[index] is free for you

			entry->pa = coremap[index].ce_paddr;
			entry->permissions =permissions;
			entry->present=1;
			entry->va=faultaddr;
			entry->next=NULL;

			//Getting time
			time_t seconds;
			uint32_t nanoseconds;
			gettime(&seconds, &nanoseconds);

			//Update the coremap entries
			coremap[index].as=as;
			coremap[index].chunk_allocated=0;
			coremap[index].page_status=2;
			coremap[index].time=nanoseconds;

			pa = coremap[index].ce_paddr;

			//Zero the region
			as_zero_region(coremap[index].ce_paddr,1);
			struct page_table_entry *head;

			head = as->page_table;
			while(as->page_table!=NULL)
			{
				if(as->page_table->next == NULL)
				{
					as->page_table->next = entry;
					break;
				}
				as->page_table = as->page_table->next;
			}

			as->page_table = head;
			if(coremap[index].as->page_table != coremap[index].as->pagelist_head)
				panic("Head does not match with page table head in PA==0");

			if(as->page_table != as->pagelist_head)
				panic("Head does not match with page table head in PA==0 while checking the address");

			check_addrspace(as);


			return pa;
		}

	}//End of if checking whether page table is not null

	return pa;

}



/*
 * Change_page_entry function
 */
void
change_coremap_page_entry(int index)
{
	//Now decide what to do based on the current page status at the coremap[index]

	if(coremap[index].page_status==0)
	{
		//Meaning the page was free
		//Do Nothing

	}
	else if(coremap[index].page_status==3)
	{
		//Meaning the page was clean and just needs to be evicted
		evict_coremap_entry(index);
	}
	else if(coremap[index].page_status==2)
	{
		/**
		* Meaning that the page is dirty --Give a call to find swapindex of the page at coremap[index]
		* Call function to swapout the page at the index
		*/


		swapout_page(index);
	}

		coremap[index].as=NULL;
		coremap[index].chunk_allocated=0;
		coremap[index].page_status=0;
		coremap[index].time=0;


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
	int flag=-1;
	paddr_t pa = coremap[index].ce_paddr;

	struct page_table_entry *head;
	head = coremap[index].as->pagelist_head;

	while(head != NULL)
	{
		if(pa == head->pa)
		{
			head->present = 0;
			flag=1;
			break;
		}
		head = head->next;
	}
	if(flag==-1)
		panic("Problem in evict");

	coremap[index].as=NULL;
	coremap[index].chunk_allocated=0;

	coremap[index].page_status=0;
	coremap[index].time=0;
}

/*
 * Function to swap out the page
 */
void
swapout_page(int index)
{

	paddr_t pa = coremap[index].ce_paddr;
	vaddr_t tlb_vaddr;
	int swapout_index=-1;

	struct page_table_entry *head;
	head = coremap[index].as->pagelist_head;

	while(head != NULL)
	{
		if(pa == head->pa)
		{
			head->present =0;
			swapout_index = find_swapfile_entry(coremap[index].as,head->va);
			if(swapout_index <1)
				panic("Wrong Swapout index given");

			tlb_vaddr = head->va;
			head->swapfile_index = swapout_index;

			break;
		}
		head = head->next;
	}
	if(swapout_index <1)
			panic("Wrong Swapout index given after for loop");


	my_tlb_shhotdown(tlb_vaddr);
	//Means the existing page needs to be swapped out

	write_page(pa,swapout_index);


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
				swap_info[counter]->addr=as;
				swap_info[counter]->va=va;
				break;
			}
		}
	}

	lock_release(swap_lock);

	if(index==-1)
		panic("INDEX WRONG in FINDING SWAPFILE ENTRY");
	return index;
}
void
write_page(paddr_t pa, int index)
{
	if(index<=0)
		panic("Wrong index given in write page");

	int result;
	struct iovec iov;
	struct uio uio;

	uio_kinit(&iov, &uio, (void*)PADDR_TO_KVADDR(pa), PAGE_SIZE, ((index-1)*PAGE_SIZE), UIO_WRITE);

	result= VOP_WRITE(swapfile_vnode, &uio);
	if(result){
		panic("Not able to write to SWAP FILE");
	}

}

void
read_page(paddr_t pa, int index)
{
	int result;

	struct iovec iov;
	struct uio uio;

	uio_kinit(&iov, &uio, (void*)PADDR_TO_KVADDR(pa), PAGE_SIZE, ((index-1)*PAGE_SIZE), UIO_READ);

	result= VOP_READ(swapfile_vnode, &uio);
	if(result){
		panic("Not able to read from SWAP FILE");
	}

}

int
make_swap_file()
{
	int result;

	swap_file_lock= lock_create("swapfile_lock");

	cv_swap = cv_create("swap_file_CV");

	char k_des[NAME_MAX];
	memcpy(k_des, SWAP_FILE,NAME_MAX);


	result = vfs_open(k_des,O_CREAT, 0, &swapfile_vnode);

	if (result) {
//		lock_release(swap_file_lock);
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

void
swapin_page(paddr_t pa,int index)
{
	read_page(pa,index);
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



void
free_coremap_locked(paddr_t pa)
{
	for(int i =0;i<total_systempages;i++)
	{
		if(coremap[i].ce_paddr == pa)
		{
			coremap[i].locked=0;
			break;
		}

	}
}
void
check_addrspace(struct addrspace *as)
{
	struct page_table_entry *head;
	head = as->pagelist_head;

	while(head != NULL)
	{
		if(head->pa < 1)
			panic("PA NOT PROPER");

		head = head->next;
	}

}


void
check_coremap(int f)
{
	f=0;
	/*struct page_table_entry *head;
	int flag=-1;

	for(int index=0;index<total_systempages;index++)
	{
		flag=-1;

		if((coremap[index].page_status != 1) && (coremap[index].page_status != 0) )
		{

			if(coremap[index].as->page_table != coremap[index].as->pagelist_head)
				panic("Head does not match with page table head at index %d",index);



			head = coremap[index].as->pagelist_head;

			while(coremap[index].as->pagelist_head != NULL)
			{
				if(coremap[index].ce_paddr == coremap[index].as->pagelist_head->pa)
				{
					flag=1;
					break;
				}
				coremap[index].as->pagelist_head= coremap[index].as->pagelist_head->next;
			}

			coremap[index].as->pagelist_head = head;

			if(flag<0)
				panic("problem at index %d called from %d",index,f);
		}
	}
*/


}
