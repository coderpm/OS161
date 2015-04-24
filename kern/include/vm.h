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

/*
 * Added By Mohit & Pratham
 */

struct coremap_entry{
	paddr_t ce_paddr;		//Physical Address of Page
	vaddr_t ce_vaddr;		//Virtual Address of Page
//	bool is_allocated:1;	//Set to 1 if Page is allocated
//	bool is_kernPage:1;
//	bool is_dirty:1;

	/**
	 * Value settings for page status
	 * Set to 0 if page status is Free
	 * Set to 1 if page status is Fixed
	 * Set to 2 if page status is Dirty
	 * Set to 3 if page status is Clean
	 */
	int32_t page_status;
	pid_t process_id;		//Stores the process id of the process which is accessing the coremap entry
	int chunk_allocated;	//Stores the number of chunk allocated so that it is easy to free
};

extern struct coremap_entry *coremap;
extern bool coremap_initialized;

//Variable to store the total number of pages in the coremap entry
extern int32_t total_systempages;

//Global variable for coremap_lock
extern struct lock *coremap_lock;

//Function to find continuous npages from coremap array entry
int
find_npages(int npages);



#endif /* _VM_H_ */
