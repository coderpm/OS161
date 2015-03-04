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
	struct semaphore *exit_semaphore;

};

struct child_process{

	pid_t child_pid;
	struct child_process *next;
};

int
sys___getpid(int32_t *retval);

int
sys___exit(userptr_t user_seconds_ptr);

int
sys___waitpid(userptr_t processid,int *status,userptr_t options);

void
allocate_pid(void);

void
deallocate_pid(void);

struct lock *pid_lock;

void
create_pidlock(void);





/*
 * Functions for waitpid
 */
//waitpid(pid_t pid, int *status, int options);


/*
 * Fork syscall
 */
//int sys___fork(pid_t  *returnval, struct trapframe *tf);


#endif /* _PSYSCALL_H_ */
