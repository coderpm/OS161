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
	 int32_t global_pidarray[PID_MAX];

int
sys___getpid(int32_t *retval);

//pid_t sys___fork(pid_t  *returnval, struct trapframe *);

void
allocate_pid(void);


struct lock *pid_lock;

void
create_pidlock(void);


/**
 * Structure for Process Control
 */
struct process_control{
	/* process pid */
	pid_t process_id;
	pid_t parent_id;
};


//End of additions by Pratham Malik




#endif /* _PSYSCALL_H_ */
