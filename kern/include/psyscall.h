#ifndef _PSYSCALL_H_
#define _PSYSCALL_H_

#include <types.h>
#include <thread.h>
#include <limits.h>


struct trapframe;
struct thread;

/**
 * Declared array for pid tracking
 */
extern struct process_control *process_array[PROCESS_MAX];

/**
* Structure for Process Control
*/
struct process_control
{
	/* process pid */

	pid_t parent_id;
	bool exit_status;
	int exit_code;

	struct thread *mythread;

	//Child Process
	struct child_process *childlist;

	//Semaphore to synchronize the the exit status
	struct lock *process_lock;
	struct cv *process_cv;
};

struct child_process{

	pid_t child_pid;
	struct child_process *next;
};

/* Function to allocate pid to the thread and initialize the contents of Process structure*/

pid_t
allocate_pid(void);

void
initialize_pid(struct thread *thr,pid_t processid);

int
sys___getpid(int32_t *retval);

int
sys___exit(int);

int
sys___waitpid(int ,userptr_t ,int );

void
deallocate_pid(pid_t processid);

int
sys___fork(struct trapframe *tf,pid_t  *returnval);

void
enter_process(void *tf,unsigned long addr);

void
copy_trapframe(struct trapframe *src,struct trapframe *dest);

#endif /* _PSYSCALL_H_ */
