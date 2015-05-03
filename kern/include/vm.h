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

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */


#include <machine/vm.h>
#include <types.h>
#include <synch.h>
#include <addrspace.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/


/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);
void free_kpages(vaddr_t addr);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void);
void vm_tlbshootdown(const struct tlbshootdown *);

/**
 * Added for Assignment 3
 */
paddr_t
getppages(unsigned long npages);
vaddr_t
alloc_kpages(int npages);
void
free_kpages(vaddr_t addr);
void
vm_tlbshootdown_all(void);
void
vm_tlbshootdown(const struct tlbshootdown *ts);
int
vm_fault(int faulttype, vaddr_t faultaddress);
void
page_free(paddr_t paddr);

/*
 * Added By Mohit & Pratham
 */

struct coremap_entry{
	paddr_t ce_paddr;		//Physical Address of Page

	/**
	 * Value settings for page status
	 * Set to 0 if page status is Free
	 * Set to 1 if page status is Fixed
	 * Set to 2 if page status is Dirty
	 * Set to 3 if page status is Clean
	 */
	int32_t page_status;

	struct addrspace *as; 	//Stores the address space pointer of the process which is mapped to the coremap entry
	int chunk_allocated;	//Stores the number of chunk allocated so that it is easy to free
	int32_t time;
};

extern struct coremap_entry *coremap;
extern bool coremap_initialized;

//Variable to store the total number of pages in the coremap entry
extern int32_t total_systempages;
extern int32_t coremap_pages;

//Global variable for coremap_lock
extern struct spinlock coremap_lock;
extern struct spinlock tlb_lock1;
extern struct spinlock tlb_lock2;
extern struct spinlock tlb_lock3;
extern struct spinlock tlb_lock4;

//Function to find continuous npages from coremap array entry
int
find_npages(int npages);


//Function to remove user level pages
void
free_upages(void);

//Function to allocate user level pages
int
alloc_upages(void);

int
find_page_available(int npages);


int
find_oldest_page(void);

int
find_npages(int npages);

void
evict_coremap_entry(int index);

paddr_t
handle_address(vaddr_t faultaddr,int permissions,struct addrspace *as);





#endif /* _VM_H_ */
