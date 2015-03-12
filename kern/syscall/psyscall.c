
#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <lib.h>
#include <limits.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <psyscall.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <mips/trapframe.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <file_syscall.h>



void
initialize_pid(struct thread *thr,pid_t processid)
{
	struct process_control *p_array;

	p_array=kmalloc(sizeof(struct process_control));

	thr->t_pid=processid;

	p_array->parent_id=-1;
	p_array->childlist=NULL;
	p_array->exit_code=-1;
	p_array->exit_status=false;
	p_array->mythread=thr;

	p_array->process_sem = sem_create(thr->t_name,0);


	/*if(processid>4)
	{
		p_array->process_lock=lock_create(thr->t_name);
		p_array->process_cv = cv_create(thr->t_name);
	}
	else
	{
		p_array->process_lock=NULL;
		p_array->process_cv = NULL;
	}*/

	//Copy back into the thread
	process_array[processid]=p_array;


}

pid_t
allocate_pid(void)
{
	for(int i=PID_MIN;i<PROCESS_MAX;i++)
	{
		if(process_array[i]==0)
		{
			return i;
			break;
		}
	}
	return -1;
}

int
sys___exit(int exit_code)
{

	//Check whether the process calling exit has no children?

	pid_t pid_process=curthread->t_pid;

	//Store the exit code passed in the argument
	process_array[pid_process]->exit_code= _MKWAIT_EXIT(exit_code);

	//Indicate Exit by calling changing the exit status in the process array
	process_array[pid_process]->exit_status=true;

	//Check whether to indicate exit by calling cv_broadcast as well
//	cv_broadcast(process_array[pid_process]->process_cv,process_array[pid_process]->process_lock);

	//Now using a semaphore and V when the thread exits -- Approach Discarded
	V(process_array[pid_process]->process_sem);

	thread_exit();

	return 0;
}

int
sys___getpid(int32_t *retval)
{
	*retval = curthread->t_pid;
	return 0;
}

int
sys___waitpid(int processid,userptr_t  status,int options)
{
	pid_t pid_process= (int32_t) processid;
	int exit_code;
	int result;

	userptr_t user_status;

	if(processid<PID_MIN)
		return ESRCH;

	if( options!=0){
		return EINVAL;
	}
	//Check whether the pid exists
	if(process_array[processid]==0 ||process_array[processid]==NULL)
	{
		return ESRCH;
	}

	//CHeck if the status is not NULL
	if(status==NULL)
		return EFAULT;

	if (processid < PID_MIN)
		return EINVAL;

	if(processid > PID_MAX)
		return ESRCH;

	if (processid == curthread->t_pid)
		return ECHILD;

	//TODO::Check whether the pid exists in your child list -- Complete after fork - Completed below
	if(!(curthread->t_pid == process_array[processid]->parent_id))
		return ECHILD;

	//Check if the return status is not invalid by copyin
	result = copyin(status,user_status,sizeof(status));
	if(result)
		return result;

	P(process_array[pid_process]->process_sem);
	exit_code = process_array[pid_process]->exit_code;

	//Copyout the status
	result = copyout((void *)exit_code,user_status,sizeof(user_status));
	if(result)
			return result;

	status=user_status;
	//Destroy Child's Process Structure - Call deallocate_pid
	deallocate_pid(pid_process);

	return 0;
}

void
deallocate_pid(pid_t processid)
{

	if(process_array[processid]==0 || process_array[processid]==NULL)
	{
		//Do Nothing
	}
	else
	{
		/*lock_destroy(process_array[processid]->process_lock);
		cv_destroy(process_array[processid]->process_cv);
*/
		sem_destroy(process_array[processid]->process_sem);
		kfree(process_array[processid]);
		process_array[processid]=0;
	}
}


/*
 * Fork System Call:: Forks a new process
 * Returns two values to child and Parent.

*/
struct sendthing
{
	struct addrspace *parentaddr;
	struct trapframe *childtf;
};
int
sys___fork(struct trapframe *tf, pid_t *returnval)
{
	int result;
	struct sendthing *sendtochild;
	struct thread *child;


	sendtochild = kmalloc(sizeof(struct sendthing));
	if(sendtochild==NULL)
		return ENOMEM;

	sendtochild->childtf = tf;


	sendtochild->parentaddr= curthread->t_addrspace;

	pid_t ada= curthread->t_pid;
	unsigned long data;
	data = (unsigned long) ada;
//	long addr = (unsigned long) childspace;

	result = thread_fork(curthread->t_name,enter_process,sendtochild,data,&child);
	if(result){
		if(child->t_pid > 0)
		deallocate_pid(child->t_pid);
		return result;

	}



	*returnval = child->t_pid;

	return 0;
}

void
enter_process(void *tf,unsigned long addr)
{

	/*int result;
	struct addrspace *childspace;
*/

	struct sendthing *childthings;
	//	(void)tf;

	childthings = kmalloc(sizeof(struct sendthing));
	if(childthings!=NULL)
	{
		struct trapframe *temp_child,finalchild;

		childthings = (struct sendthing *)tf;
		//Create a new address space and copy the address from pointer received from the parent

	/*	struct addrspace *childspace;
		int result;
		childspace = kmalloc(sizeof(struct addrspace));
		if(childspace!=NULL)
		{
			result = as_copy(childthings->parentaddr, &childspace);
			if(result==0)
				curthread->t_addrspace = childspace;
		}
*/
		//copy the trapframe info now

		temp_child = childthings->childtf;

		finalchild = *temp_child;

		finalchild.tf_a3=0;
		finalchild.tf_v0=0;

		finalchild.tf_epc = finalchild.tf_epc + 4;

		pid_t parentid = (pid_t)addr;
		if(process_array[curthread->t_pid]->parent_id!=parentid)
			process_array[curthread->t_pid]->parent_id=parentid;

		if(!(curthread->t_addrspace==NULL))
			as_activate(curthread->t_addrspace);

		mips_usermode(&finalchild);

	}

}

int
sys___execv(userptr_t p_name,userptr_t argument)
{
	struct vnode *p_vnode;
	vaddr_t entrypoint, stackptr;
	int result;

	//Check for EFAULT
	if(p_name==NULL || argument == NULL)
		return EFAULT;

	char **arguments = (char **) argument;

	//Convert the program name from userptr to char ptr
//	p_name = (char *) p_name;

	//Create a variable to store the program name
	char * program_name = (char *) kmalloc(sizeof(p_name));
	size_t actual_length;
	result = copyinstr((const_userptr_t) p_name,program_name,sizeof(p_name),&actual_length);
	if(result)
		return result;


	//Create a variable to store all the arguments in array
	char **saved_args;
	//Allocate space for saved_args
	saved_args= (char **) kmalloc(sizeof(arguments));
	if(saved_args==NULL)
		return ENOMEM;

	//Copyin the arguments all the arguments
	result = copyin((const_userptr_t) arguments,saved_args,sizeof(arguments));
	if(result)
		return result;




		/* Open the file. */
		//char *p_name = (char *) program_name;
		result = vfs_open(program_name, O_RDONLY, 0, &p_vnode);
		if (result) {
			return result;
		}

		/* We should be a new thread. */
		KASSERT(curthread->t_addrspace == NULL);

		/* Create a new address space. */
		curthread->t_addrspace = as_create();
		if (curthread->t_addrspace==NULL) {
			vfs_close(p_vnode);
			return ENOMEM;
		}


		/* Activate it. */
		as_activate(curthread->t_addrspace);

		/* Load the executable. */
		result = load_elf(p_vnode, &entrypoint);
		if (result) {
			/* thread_exit destroys curthread->t_addrspace */
			vfs_close(p_vnode);
			return result;
		}

		/* Done with the file now. */
		vfs_close(p_vnode);

		/* Define the user stack in the address space */
		result = as_define_stack(curthread->t_addrspace, &stackptr);
		if (result) {
			/* thread_exit destroys curthread->t_addrspace */
			return result;
		}
		int result1=100;
		kprintf("Inside run program");
		result1= intialize_file_desc_tbl(curthread->file_table);
		if( intialize_file_desc_tbl(curthread->file_table)){
			kprintf("Error");
			return result1;
		}
		/* Warp to user mode. */
		enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
				  stackptr, entrypoint);

		/*
			 * Added By Mohit
			 *
			 * Started for file table initialization
			 */

			/*
			 * Ended
			 */

		/* enter_new_process does not return. */
		panic("enter_new_process returned\n");
		return EINVAL;
	return 0;
}


