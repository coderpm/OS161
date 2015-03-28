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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <file_syscall.h>

#include <copyinout.h>
/*
 *
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int

runprogram(char *progname, char **args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	if(args==NULL)
	{
		//Do Nothing

	}
	else
	{
		char **user_args;
		user_args = kmalloc(sizeof(char **));

		user_args[0]=args[0];

	}




	char k_des[PATH_MAX];
	memcpy(k_des, progname,PATH_MAX);





	/* Open the file. */
	result = vfs_open(k_des, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}


	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}

	/*
	* Added By Mohit
	*
	* Started for file table initialization
	*/
		int string_length = strlen(progname)+1;
		int new_length = string_length;
		if((string_length) % 4 != 0)
			{

				while(new_length%4 !=0)
				{
					new_length++;
				}

				for(int i=string_length;i<=new_length;i++)
				{
					progname[i]= '\0';
				}
			}
		size_t final_length= (size_t)new_length;
		size_t null_length= sizeof(NULL);
		size_t pointer_length= sizeof(char*);
		//size_t actual_length;
		size_t final_stack= stackptr- final_length-null_length-pointer_length;
		char *karray[2];
		karray[0]=  (char*)(final_stack+null_length+ pointer_length);
		karray[1]=  (char*)NULL;
		//char *x= &karray[0];
		result= copyout(karray, (userptr_t) (final_stack),sizeof(karray));
		if(result){
			return result;
		}
		size_t actual_length1;
		result= copyoutstr(progname, (userptr_t) (final_stack+ sizeof(karray)), final_length, &actual_length1);
		if(result){
			return result;
		}


/**
 * Author: Mohit Arora
 * Initialing the file table
 */

	int result1=100;
	//kprintf("Inside run program");
	result1= intialize_file_desc_tbl(curthread->file_table);
	if( intialize_file_desc_tbl(curthread->file_table)){
		kprintf("Error");
		return result1;
	}

	/*
	* Ended
	*/



//End of Additions by MA
	result = as_define_stack(curthread->t_addrspace, &stackptr);
		if (result) {
			/* thread_exit destroys curthread->t_addrspace */
			return result;
		}

	/* Warp to user mode. */
	enter_new_process(1 /*argc*/, (userptr_t)(final_stack) /*userspace addr of argv*/,
			final_stack, entrypoint);
	





	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}



