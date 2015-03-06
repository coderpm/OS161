#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <lib.h>
#include <thread.h>
#include <limits.h>
#include <current.h>
#include <synch.h>
#include <psyscall.h>
#include <kern/errno.h>


void
allocate_pid(struct thread *thr)
{
	for(int i=PID_MIN;i<PROCESS_MAX;i++)
	{
		if(process_array[i]==0)
		{
			struct process_control *p_array;

			p_array=kmalloc(sizeof(p_array));

			thr->t_pid=i;

			p_array->parent_id=-1;
			p_array->childlist=NULL;
			p_array->exit_code=-1;
			p_array->exit_status=false;
		//	p_array->exit_semaphore = NULL;
			p_array->mythread=thr;

			if(i>4)
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
			process_array[i]=p_array;
			break;
		}
	}
}

int
sys___exit(int exit_code)
{

	//Check whether the process calling exit has no children?

	pid_t pid_process=curthread->t_pid;

	//Store the exit code passed in the argument
	process_array[pid_process]->exit_code= exit_code;

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

	lock_destroy(process_array[processid]->process_lock);
	cv_destroy(process_array[processid]->process_cv);

	kfree(process_array[processid]);
	process_array[processid]=0;
}


/*
 * Fork System Call:: Forks a new process
 * Returns two values to child and Parent.



int
sys___fork(struct trapframe *tf, pid_t *returnval)
{

	struct thread *child_thread;
	allocate_pid(child_thread);
	char nm= (char) child_thread->t_pid;



	thread_fork(nm,enter_forked_process,tf,nm,child_thread);

	return returnval;
}
void
enter_forked_process(struct trapframe *tf)
{
	(void)tf;
}

*/
