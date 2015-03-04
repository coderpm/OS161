
#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <lib.h>
#include <thread.h>
#include <limits.h>
#include <current.h>
#include <synch.h>
//#include <psyscall.h>

struct process_control *process_array[PROCESS_MAX];

/*
 * Fork System Call:: Forks a new process
 * Returns two values to child and Parent.


int
sys___fork(pid_t  *returnval, struct trapframe *tf)
{
	struct trapframe *child_trapframe;
	child_trapframe = tf;


	return returnval;
}
*/
/**
 * Function for allocation pid from global array of
 * process id
 * 1 : Signifies that index location is taken
 * Never initialize the array index at 1
 */

void
allocate_pid(void)
{
//Take lock before allocating the pid
	lock_acquire(pid_lock);

	for(int i=PID_MIN;i<PROCESS_MAX;i++)
	{
		if(process_array[i]==0)
		{
			struct process_control *p_array;

			p_array=kmalloc(sizeof(p_array));

			curthread->t_pid=i;

			p_array->parent_id=-1;
			p_array->childlist=NULL;
			p_array->exit_code=-1;
			p_array->mythread=curthread;
			p_array->exit_status=false;

			//create a semaphore for the exit status
			char *name = (char *)i;
			p_array->exit_semaphore = sem_create(name,0);

			//Copy back into the thread
			process_array[i]=p_array;
			break;
		}
	}

	lock_release(pid_lock);

}

void
deallocate_pid(void)
{

	lock_acquire(pid_lock);

	pid_t processid=curthread->t_pid;

	for(int32_t i=PID_MIN;i<PROCESS_MAX;i++)
	{
		if(i==processid)
		{
			process_array[i]->parent_id=-1;
			process_array[i]=0;
			//Write more to free the memory occupied and clear the child memory
			//Also destroy the synchronization variable

			break;
		}
	}

	lock_release(pid_lock);
}


int
sys___getpid(int32_t *retval)
{
	*retval = curthread->t_pid;
	return 0;
}

int
sys___exit(userptr_t exit_code)
{
	pid_t pid_process=curthread->t_pid;
	//Store the exit code passed in the argument
	process_array[pid_process]->exit_code= (int32_t) exit_code;

	//Indicate Exit by calling changing the exit status in the process array
	process_array[pid_process]->exit_status=true;

	//Check whether to indicate exit by calling cv_broadcast as well -- Approach Discarded
	//Now using a semaphore and V when the thread exits

	V(process_array[pid_process]->exit_semaphore);


	thread_exit();

	return 0;
}

int
sys___waitpid(userptr_t processid,int *status,userptr_t options)
{

	if((int32_t) options!=0){
		return -1;
	}
	//Check whether the pid exists in your child list
	pid_t pid_process= (int32_t) processid;

	while(!(process_array[pid_process]->exit_status))
	{
		P(process_array[pid_process]->exit_semaphore);
	}

	*status = (intptr_t) process_array[pid_process]->exit_code;

	//Destroy Child's Process Structure
	sem_destroy(process_array[pid_process]->exit_semaphore);
	process_array[pid_process]->childlist=NULL;
	kfree(process_array[pid_process]);

	process_array[pid_process]=0;

	return 0;
}



/**
 * Create lock for the pid allocation
*/

void
create_pidlock(void)
{
	pid_lock = lock_create("pid_lock");
}
