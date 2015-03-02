
#include <types.h>
#include <clock.h>
#include <copyinout.h>
#include <psyscall.h>
#include <lib.h>
#include <thread.h>
#include <limits.h>
#include <current.h>
#include <synch.h>


/*
 * Fork System Call:: Forks a new process
 * Returns two values to child and Parent.

pid_t
sys___fork(pid_t  *returnval, struct trapframe *tf)
{

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

	for(int32_t i=PID_MIN;i<PID_MAX;i++)
	{
		if(global_pidarray[i]==0){
			global_pidarray[i]=1;
			curthread->t_pcb->process_id=i;
			break;
		}
	}

	lock_release(pid_lock);
}

int
sys___getpid(int32_t *retval)
{
	*retval = curthread->t_pcb->parent_id;
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
