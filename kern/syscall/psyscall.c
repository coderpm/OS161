
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
	if(curthread!=NULL)
		lock_acquire(pid_lock);
	struct process_control *p_array;

	p_array=kmalloc(sizeof(struct process_control));

	thr->t_pid=processid;

	p_array->parent_id=-1;
	p_array->childlist=NULL;
	p_array->exit_code=-1;
	p_array->exit_status=false;
	p_array->mythread=thr;
	p_array->waitstatus=false;
	p_array->process_sem = sem_create(thr->t_name,0);

	//Create the lock and CV
	p_array->process_lock=lock_create(thr->t_name);
	p_array->process_cv = cv_create(thr->t_name);

	//Copy back into the thread
	process_array[processid]=p_array;

	if(curthread!=NULL)
		lock_release(pid_lock);

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

void
deallocate_pid(pid_t processid)
{

	if(process_array[processid]==0 || process_array[processid]==NULL)
	{
		//Do Nothing
	}
	else
	{
		pid_t parent_id = process_array[processid]->parent_id;
		if(parent_id>PID_MIN)
		{
				int counter=0;
				for(counter=3;counter<__OPEN_MAX;counter++)
				{
					if(curthread->file_table[counter]!=0)
					{
						process_array[parent_id]->mythread->file_table[counter]->reference_count--;
					}
					else
					{
							//newthread->file_table[counter]=0;
					}
				}
		}

		sem_destroy(process_array[processid]->process_sem);
		lock_destroy(process_array[processid]->process_lock);
		cv_destroy(process_array[processid]->process_cv);
		kfree(process_array[processid]);
		process_array[processid]=0;

	}
}

int
sys___exit(int exit_code)
{
	//Check whether the process calling exit has no children?

	pid_t pid_process=curthread->t_pid;

	//Check whether to indicate exit by calling cv_broadcast as well
//	cv_broadcast(process_array[pid_process]->process_cv,process_array[pid_process]->process_lock);

	//Now using a semaphore and V when the thread exits
	if(process_array[pid_process]->waitstatus==true)
	{
		lock_acquire(process_array[pid_process]->process_lock);

		//Indicate Exit by calling changing the exit status in the process array
		process_array[pid_process]->exit_code= _MKWAIT_EXIT(exit_code);

		process_array[pid_process]->exit_status=true;

		cv_signal(process_array[pid_process]->process_cv,process_array[pid_process]->process_lock);

		lock_release(process_array[pid_process]->process_lock);

	}
		//V(process_array[pid_process]->process_sem);
	else
	{
		lock_acquire(process_array[pid_process]->process_lock);

		//Store the exit code passed in the argument
		process_array[pid_process]->exit_code= _MKWAIT_EXIT(exit_code);

		//Indicate Exit by calling changing the exit status in the process array
		process_array[pid_process]->exit_status=true;

		lock_release(process_array[pid_process]->process_lock);

	}

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
sys___waitpid(int processid,userptr_t  status,int options, int32_t *retval)
{
	int exit_code;
	int result;

	if( options!=0){
		return EINVAL;
	}
	//Check whether the pid exists
	if(processid<PID_MIN || process_array[processid]==0 ||process_array[processid]==NULL)
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


	if(!(curthread->t_pid == process_array[processid]->parent_id))
		return ECHILD;


	if(process_array[processid]->exit_status==true)
	{
		lock_acquire(process_array[processid]->process_lock);

		exit_code = process_array[processid]->exit_code;

		lock_release(process_array[processid]->process_lock);

		result = copyout(&exit_code,status,sizeof(userptr_t));
		if(result)
			return result;

		//Destroy Child's Process Structure - Call deallocate_pid

		*retval = processid;
		deallocate_pid(processid);

	}
	else if(process_array[processid]->exit_status==false)
	{
		lock_acquire(process_array[processid]->process_lock);

		process_array[processid]->waitstatus=true;
		cv_wait(process_array[processid]->process_cv,process_array[processid]->process_lock);

		//P(process_array[pid_process]->process_sem);
		exit_code = process_array[processid]->exit_code;

		lock_release(process_array[processid]->process_lock);

		result = copyout(&exit_code,status,sizeof(userptr_t));
			if(result)
					return result;

		//Destroy Child's Process Structure - Call deallocate_pid

		*retval = processid;
		deallocate_pid(processid);
	}

		return 0;
}

int
sys___kwaitpid(int processid,int *status,int options, int32_t *retval)
{
	int exit_code;

	if( options!=0){
		return EINVAL;
	}
	//Check whether the pid exists
	if(processid<PID_MIN || process_array[processid]==0 ||process_array[processid]==NULL)
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


	if(!(curthread->t_pid == process_array[processid]->parent_id))
		return ECHILD;

	if(process_array[processid]->exit_status==true)
	{
		lock_acquire(process_array[processid]->process_lock);

		exit_code = process_array[processid]->exit_code;

		lock_release(process_array[processid]->process_lock);

		status = &exit_code;

	/*	result = copyout(&exit_code,status,sizeof(userptr_t));
		if(result)
			return result;
*/
		//Destroy Child's Process Structure - Call deallocate_pid

		*retval = processid;
		deallocate_pid(processid);

	}
	else if(process_array[processid]->exit_status==false)
	{
		lock_acquire(process_array[processid]->process_lock);

		process_array[processid]->waitstatus=true;
		cv_wait(process_array[processid]->process_cv,process_array[processid]->process_lock);

		//P(process_array[pid_process]->process_sem);
		exit_code = process_array[processid]->exit_code;

		lock_release(process_array[processid]->process_lock);

		status = &exit_code;

		/*result = copyout(&exit_code,status,sizeof(userptr_t));
			if(result)
					return result;
*/
		//Destroy Child's Process Structure - Call deallocate_pid

		*retval = processid;
		deallocate_pid(processid);
	}

		return 0;
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
	struct thread *child;
	struct trapframe *parent_tf;

	parent_tf = kmalloc(sizeof(struct trapframe));
	if(parent_tf==NULL)
		return ENOMEM;

	memcpy(parent_tf,tf,sizeof(struct trapframe));
//	parent_tf=tf;

	pid_t current_pid = curthread->t_pid;

	unsigned long parent_pid;
	parent_pid = (unsigned long) current_pid;

	result = thread_fork(curthread->t_name,enter_process,parent_tf,parent_pid,&child);
	if(result){
	//	kfree(parent_tf);
	/*	if(process_array[child->t_pid] == 0 || process_array[child->t_pid] == NULL)
		{
			//Do Nothing
		}
		else
		{
			kfree(process_array[child->t_pid]);
		}

	//*/

		return result;
	}

	*returnval = child->t_pid;

	return 0;
}

void
enter_process(void *tf,unsigned long addr)
{
	struct trapframe *childframe,child_tf;
	struct addrspace *childspace;
	if(tf!=NULL)
	{

		tf = (struct trapframe *) tf;
		childframe = tf;
		//copy the trapframe info now into the child_tf


	//	child_tf = *childframe;

		memcpy(&child_tf,tf,sizeof(struct trapframe));

		child_tf.tf_a3=0;
		child_tf.tf_v0=0;

		child_tf.tf_epc +=4;


		pid_t parentid = (pid_t)addr;
		if(process_array[curthread->t_pid]->parent_id!=parentid)
			process_array[curthread->t_pid]->parent_id=parentid;

		if(!(curthread->t_addrspace==NULL))
		{
			childspace = curthread->t_addrspace;
			as_activate(childspace);
		}
		mips_usermode(&child_tf);
	}
}


int
sys___execv(char * p_name,char **ar )
{
	struct vnode *p_vnode;
		vaddr_t  stackptr;
		vaddr_t entrypoint;

		int result;
		size_t copied_length;
		char *kname;

		if(p_name==NULL)
			return EFAULT;

		if(ar==NULL)
			return EFAULT;

		if(p_name == '\0')
			return ENOEXEC;


		kname = (char *) kmalloc(sizeof(p_name));
		result = copyinstr((const_userptr_t)p_name,kname,NAME_MAX,&copied_length);
		if(copied_length == 1)
		{
			kfree(kname);
			return EINVAL;
		}
		if(result)
		{
			kfree(kname);
			return result;
		}

		char **karguments1 = kmalloc(100*sizeof(char*));
		result = copyin((const_userptr_t) ar,karguments1,(100*sizeof(char*)));
		if(result)
		{
			kfree(karguments1);
			kfree(kname);
			return result;
		}
		else
			kfree(karguments1);

		char **arguments_kernel =(char **) kmalloc(sizeof(char**));
		result = copyin((const_userptr_t) ar,arguments_kernel,(sizeof(char**)));
		if(result)
		{
			kfree(kname);
			kfree(arguments_kernel);
			return result;

		}

		char **karguments = (char **) kmalloc(sizeof(char **));

		int counter=0;
		size_t actual_lenght1;
		while(!(ar[counter]==NULL))
		{
			int string_length = strlen(ar[counter])+1;
			karguments[counter] = (char *) kmalloc(sizeof(char) * string_length);
			result = copyinstr((const_userptr_t) ar[counter],karguments[counter],string_length,&actual_lenght1);
			if(result)
			{
				kfree(kname);
				kfree(arguments_kernel);
				return result;
			}

			counter++;
		}
		karguments[counter] = NULL;

		result = vfs_open(kname, O_RDONLY, 0, &p_vnode);
		if (result) {
			return result;
		}

		as_destroy(curthread->t_addrspace);

		//Create a new address space.
		curthread->t_addrspace = as_create();
		if (curthread->t_addrspace==NULL)
		{
			vfs_close(p_vnode);
			return ENOMEM;
		}

		//Activate it.
		as_activate(curthread->t_addrspace);

		/* Load the executable. */
		result = load_elf(p_vnode, &entrypoint);
		if (result)
		{
			/* thread_exit destroys curthread->t_addrspace */
			vfs_close(p_vnode);
			return result;
		}

		/* Done with the file now. */
		vfs_close(p_vnode);

		//Get the pointer to the stack starting
		result = as_define_stack(curthread->t_addrspace, &stackptr);
		if (result)
		{
			/* thread_exit destroys curthread->t_addrspace */
			return result;
		}

		//Now copy the arguments
		int count =0;
		char **karray= kmalloc(sizeof(char**));
		size_t final_stack=0;

		while(karguments[count] != NULL)
		{
			int string_length = strlen(karguments[count])+1;
			char *k_des= karguments[count];

			int new_length = string_length;
			if((string_length) % 4 != 0)
			{
				while(new_length%4 !=0)
				{
					new_length++;
				}
				for(int i=string_length;i<=new_length;i++)
				{
					k_des[i]= '\0';
				}
			}

			//Argument aligned by 4

			size_t final_length= (size_t)new_length;

			if(count==0)
			{
				final_stack= stackptr- final_length;
			}
			else
			{
				final_stack= (size_t)karray[count-1]- final_length;
			}

			size_t actual_length1;
			result= copyoutstr(k_des, (userptr_t) (final_stack), final_length, &actual_length1);
			if(result)
			{
				return result;
			}

			karray[count]=  (char*)(final_stack);

			count++;
		} //End of While

		karray[count]= (char*)NULL;
		int value= count+1;
		int arr_length = (value+1)*sizeof(char*);
		final_stack= (size_t)karray[count-1]- arr_length;
		result= copyout(karray, (userptr_t) (final_stack),arr_length);
		if(result)
		{
			return result;
		}


			/* Warp to user mode. */
				enter_new_process(count /*argc*/, (userptr_t)(final_stack) /*userspace addr of argv*/,
						final_stack, entrypoint);

		return 0;
}
//End of Additions by PM

//Addition by Mohit
int
sys___sbrk(int amount, int *retval)
{
	if(amount <0)
		return EINVAL;

	struct addrspace *as = curthread->t_addrspace;

	if(as->heap_end+amount > 0x40000000)
		return ENOMEM;



	if((as->heap_end+amount)< as->heap_start){
		return EINVAL;
	}
	if(!(amount%4==0)){
		amount = amount + (amount%4);
	}
	vaddr_t heap_end= as->heap_end;

	if((heap_end+amount)>= as->stackbase_base){
		return ENOMEM;
	}

	as->heap_end= as->heap_end+amount;
	*retval= (int)heap_end;

return 0;
}


