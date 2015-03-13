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



	char k_des[PATH_MAX];
	memcpy(k_des, progname,PATH_MAX);


	/**
	 * Author:Pratham Malik
	 * adding arguments to stack
	 */
	/*
	char **user_args;
	char *temp;
	char *final;
	int string_length;
	int new_length;
	int i;

	counter=0;
	string_length=0;


	user_args = kmalloc(sizeof(char **));

	while(args[counter] != NULL)
	{
		temp = kstrdup(args[counter]);
		kprintf("The argument is %s",temp);

		string_length = strlen(temp);
		if((string_length) % 4 == 0)
		{
			user_args[counter] = kmalloc(sizeof(string_length));
//			result= copyout(temp,(userptr_t) user_args[counter],sizeof(string_length));
//			if(result)
//				return result;

			user_args[counter] = temp;
		}
		else
		{

			new_length = string_length;
			while(new_length%4 !=0)
			{
				new_length++;
			}

			final=temp;

			for(i=string_length;i<=new_length;i++)
			{
				final[i]= '\0';
			}

			user_args[counter] = kmalloc(sizeof(new_length));
			user_args[counter] = final;
//			result = copyout(final,(userptr_t) user_args[counter],sizeof(new_length));
				if(result)
					return result;


		}
		counter++;
	}
*/
/*
	char **userspacearg = kmalloc(sizeof(user_args));
	result = copyout(user_args,(userptr_t) userspacearg,sizeof(userspacearg));
	if(result)
		return result;
*/

	//End of additions by PM

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
	//char *uspace_args;
	int counter=0;

	while(args[counter] != NULL){
		//uspace_args= kstrdup(args[counter]);
		size_t actual_length;
		char *final=args[counter];
		int string_length = strlen(args[counter]);
		if((string_length) % 4 != 0)
		{
			int new_length = string_length;
			while(new_length%4 !=0)
			{
				new_length++;
			}

			for(int i=string_length;i<=new_length;i++)
			{
				final[i]= '\0';
			}

			/*user_args[counter] = kmalloc(sizeof(new_length));
			user_args[counter] = final;*/
		}
		char *u_argsv;
		result = as_define_stack(curthread->t_addrspace, &stackptr);
		if (result) {
			/* thread_exit destroys curthread->t_addrspace */
			return result;
		}
		result= copyoutstr(final, (userptr_t) (stackptr-PATH_MAX), PATH_MAX, &actual_length);
		if(result){
			return result;
		}
		counter++;

	}




	//char *u_argsv;
	/*size_t actual_length;
	result= copyoutstr(*args, (userptr_t) (stackptr-ARG_MAX), ARG_MAX, &actual_length);
	if(result){
		return result;
	}*/



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


	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	





	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}



