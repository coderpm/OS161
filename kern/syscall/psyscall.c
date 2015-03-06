
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

	if(processid>4)
	{
		p_array->process_lock=lock_create(thr->t_name);
		p_array->process_cv = cv_create(thr->t_name);
	}
	else
	{
		p_array->process_lock=NULL;
		p_array->process_cv = NULL;
	}

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
	cv_broadcast(process_array[pid_process]->process_cv,process_array[pid_process]->process_lock);

	//Now using a semaphore and V when the thread exits -- Approach Discarded

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

	//TODO::Check whether the pid exists in your child list -- Complete after fork



	pid_t pid_process= (int32_t) processid;

	while(!(process_array[pid_process]->exit_status))
	{
		lock_acquire(process_array[pid_process]->process_lock);
		cv_wait(process_array[pid_process]->process_cv,process_array[pid_process]->process_lock);
	}

	status = (userptr_t) process_array[pid_process]->exit_code;

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
		lock_destroy(process_array[processid]->process_lock);
		cv_destroy(process_array[processid]->process_cv);

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
	struct addrspace *childaddr;
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

	struct addrspace *childspace;
	childspace = kmalloc(sizeof(struct addrspace));
	if(childspace==NULL)
			return ENOMEM;


	result = as_copy(curthread->t_addrspace, &childspace);
	if(result)
		return ENOMEM;

	sendtochild->childaddr=childspace;
	pid_t ada= curthread->t_pid;
	unsigned long data;
	data = (unsigned long) ada;
//	long addr = (unsigned long) childspace;

	result = thread_fork("nam",enter_process,sendtochild,data,&child);

	if(result)
		return result;

	*returnval = child->t_pid;

	return 0;
}

void
enter_process(void *tf,unsigned long addr)
{

	struct sendthing *childthings;
//	(void)tf;

	struct trapframe *temp_child,finalchild;

	childthings = (struct sendthing *)tf;

	curthread->t_addrspace = childthings->childaddr;
	temp_child = (struct trapframe *)childthings->childtf;

	finalchild = *temp_child;

	finalchild.tf_a3=0;
	finalchild.tf_v0=0;

	finalchild.tf_epc = finalchild.tf_epc + 4;

	pid_t parentid = (pid_t)addr;
	process_array[curthread->t_pid]->parent_id=parentid;

	if(!(curthread->t_addrspace==NULL))
		as_activate(curthread->t_addrspace);

	mips_usermode(&finalchild);

}

