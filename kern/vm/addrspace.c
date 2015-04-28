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
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	int i, spl;

	(void)as;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

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
		int sum=readable+writeable+executable;
		as->regions->set_permissions=sum;

		as->regions->next_region = NULL;
	}
	else
	{
		//regions is not NULL -- meaning that some region has been added before
		//Iterate to reach the end
		struct addr_regions *end;
		end = (struct addr_regions *) kmalloc(sizeof(struct addr_regions));

		while((as->regions->next_region) != NULL)
		{
			end = as->regions->next_region;
		}

		end->va_start = vaddr;
		end->region_numpages= npages;
		end->va_end = (end->va_start + (npages * PAGE_SIZE));

		end->next_region = NULL;

		//Set the permissions in other variable
		int sum = readable+writeable+executable;
		end->set_permissions=sum;

		as->regions->next_region = end;
	}

	//Now declare the heap start and heap end
	as->heap_start = (vaddr+sz) & PAGE_FRAME;
	as->heap_end = (vaddr+sz) & PAGE_FRAME;

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
	kprintf("Stack TOP is %d",as->stackbase_top);
	kprintf("\nStack Base is %d",as->stackbase_base);

	kprintf("\nHeap Start is %d",as->heap_start);
	kprintf("\nHeap End is %d",as->heap_end);



	struct addr_regions *head;
	 head = as->regions;
	if(as->regions!=NULL)
	{
		//Iterate over the regions

		while(as->regions !=NULL)
		{
			kprintf("\nRegion Start is %d",as->regions->va_start);
			kprintf("\nRegion End is %d",as->regions->va_end);

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
	(void) old;
	(void)ret;
	return 0;
}
