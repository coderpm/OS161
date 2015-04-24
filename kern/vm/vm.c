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
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <mips/vm.h>
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
struct coremap_entry *coremap;
bool coremap_initialized;
struct lock *coremap_lock;

//ENd of Additions by PM
void
vm_bootstrap(void)
{
	paddr_t firstpaddr, lastpaddr;

	//Create the global coremap lock
	coremap_lock = lock_create("coremap_lock");

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
	int num_coremapPages= (freepaddr-firstpaddr)/PAGE_SIZE;
	kprintf("total pages %d \n",total_systempages);
	kprintf("total coremap pages %d \n",num_coremapPages);
	/*
	 * Mark the coremap pages as status as fixed i.e. Set to 1
	 * i.e. pages from firstaddr to freeaddr
	 * i.e. pages from 0 to num_coremapPages
	 */
	for(int i=0;i<num_coremapPages;i++)
	{
		coremap[i].ce_paddr= firstpaddr+i*PAGE_SIZE;
		coremap[i].ce_vaddr= PADDR_TO_KVADDR(freepaddr+i*PAGE_SIZE);
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
	//	coremap[i].ce_vaddr= PADDR_TO_KVADDR(freepaddr+i*PAGE_SIZE);
		coremap[i].page_status=0;	//Signifying that it is free
	}


	/*
	 * Setting the value for coremap_initialized to be 1
	 * so that now alloc_kpages works differently before and after vm_bootstrap
	 */
	coremap_initialized= 1;
	for(int i=20;i<25;i++)
	{
		kprintf("P ADDR  %d \n",coremap[i].ce_paddr);
	//	kprintf("V ADDR  %d \n",coremap[i].ce_vaddr);
		kprintf("Status  %d \n",coremap[i].page_status);

	}
	kprintf("Exiting VM_Bootstrap \n");


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

		bool allocation_condition=false;
		lock_acquire(coremap_lock);
		//Run the While the allocation condition is not true
		while(!allocation_condition)
		{
			//First check if number of pages requested is 1
			if(npages==1)
			{

				for(int i=0;i<total_systempages;i++)
				{

					//Check if we find a page whose status is free
					if(coremap[i].page_status==0)
					{

						va = PADDR_TO_KVADDR(coremap[i].ce_paddr);
						coremap[i].page_status=1;
						//coremap[index].allocation_time=;
						as_zero_region(coremap[i].ce_paddr,npages);
						allocation_condition=true;
						break;
					}//End of If checking that page_status == 0


				}//End of FOR looping over all the coremap entries checking for free page i.e. page_status =0
				if(!allocation_condition)
				{
					for(int i=0;i<total_systempages;i++)
							{
								//Loop entered because no free page found
								//Now check if the page is not fixed then replace it
								if(coremap[i].page_status!=1)
								{
									va = PADDR_TO_KVADDR(coremap[i].ce_paddr);
									coremap[i].page_status=1;
									as_zero_region(coremap[i].ce_paddr,npages);
									allocation_condition=true;
									//coremap[index].allocation_time=;
									allocation_condition=true;
									break;
								}
							}
				}

			}//End of IF check the number of npages == 1
			else
			{
				//Meaning the number of pages requested is more than 1

				int index = find_npages(npages);
				if(index<0)
					pa=0;
				else
				{
					as_zero_region(coremap[index].ce_paddr,npages);
					va = PADDR_TO_KVADDR(coremap[index].ce_paddr);

				}

			}
		} // End of while checking the allocation_condition

		lock_release(coremap_lock);

		//Return the physical address retrieved from the coremap
		return va;
	}
}

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
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */

	(void)addr;
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	as = curthread->t_addrspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
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
}

