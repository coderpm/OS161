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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>


////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
                wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }
        
        // add stuff here as needed
        lock->lock_wchan = wchan_create(lock->lk_name);
        	if (lock->lock_wchan == NULL) {
        		kfree(lock->lk_name);
        		kfree(lock);
        		return NULL;
        	}
        spinlock_init(&lock->lk_spinlock);
        lock->lock_hold=0;
        lock->lk_thread= NULL;
        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
        wchan_destroy(lock->lock_wchan);
        spinlock_cleanup(&lock->lk_spinlock);
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this
	struct thread *mythread;
	KASSERT(lock != NULL);
    KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&lock->lk_spinlock);

	while(lock->lock_hold==1){
		wchan_lock(lock->lock_wchan);
		spinlock_release(&lock->lk_spinlock);
		wchan_sleep(lock->lock_wchan);
		spinlock_acquire(&lock->lk_spinlock);
	}
/*	else if(lock->lock_hold==0){
		if (CURCPU_EXISTS()) {
				mythread = curthread;
				if (lock->lk_thread == mythread) {
					panic("Deadlock on thread %p\n", lock);
				}
			}
			else {
				mythread = NULL;
			} */

	if(curthread!= NULL){
				 mythread = curthread;
				 lock->lk_thread = mythread;
	}
	lock->lock_hold= 1;

	spinlock_release(&lock->lk_spinlock);

	  //KASSERT(lock->lock_count > 0);

       //(void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
        // Write this
    KASSERT(lock != NULL);
    spinlock_acquire(&lock->lk_spinlock);

    lock->lock_hold=0;
    KASSERT(lock->lock_hold== 0);
    lock->lk_thread= NULL;
    wchan_wakeone(lock->lock_wchan);

    spinlock_release(&lock->lk_spinlock);
       // (void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this
	if (!CURCPU_EXISTS()) {
			return true;
		}
//	if(lock->lk_holder== curcpu->c_self){
//		return true;
//	}
	//KASSERT(lock->lk_thread != NULL);
	//KASSERT(lock != NULL);
	bool value= false;

        //(void)lock;  // suppress warning until code gets written
	spinlock_acquire(&lock->lk_spinlock);
        value= (lock->lk_thread== curthread); // dummy until code gets written
        spinlock_release(&lock->lk_spinlock);
        return value;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
        
        // add stuff here as needed
        /*
         * @Author: Student
         * Implementing Condition Variable by defining the name
         * and initializing the wait channel
         */

        cv->cv_wchan= wchan_create(cv->cv_name);
		if (cv->cv_wchan == NULL) {
			kfree(cv->cv_name);
			kfree(cv);
			return NULL;
		}


        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
        
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
//        (void)cv;    // suppress warning until code gets written
  //      (void)lock;  // suppress warning until code gets written

        /*
         * Author: Student
         */
        if(lock_do_i_hold(lock)) {
        
		wchan_lock(cv->cv_wchan);
		//Release the supplied lock
		lock_release(lock);
			wchan_sleep(cv->cv_wchan);

		lock_acquire(lock);
				
        }
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
//	(void)cv;    // suppress warning until code gets written
//	(void)lock;  // suppress warning until code gets written

	/*
	 * Author: Student
	 * Wake up the wchan which is sleeping on the CV
	 */
	if(lock_do_i_hold(lock))
	{
		wchan_wakeone(cv->cv_wchan);
	}
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
//	(void)cv;    // suppress warning until code gets written
//	(void)lock;  // suppress warning until code gets written
	if(lock_do_i_hold(lock)) {
	wchan_wakeall(cv->cv_wchan);
	}
}
///////////////////////////////////////////////
//RW Locks


struct rwlock *
rwlock_create(const char *name)
{
	struct rwlock *rw;

	rw = kmalloc(sizeof(rw));
	if(rw==NULL){
		return NULL;
	}

	rw->rwlock_name=kstrdup(name);
	if(rw->rwlock_name==NULL){
		kfree(rw);
		return NULL;
	}


	//Create Semaphore
	rw->rwlock_semaphore = sem_create(rw->rwlock_name,MAX_READ);
	if (rw->rwlock_semaphore == NULL) {
			kfree(rw->rwlock_name);
			kfree(rw);
			return NULL;
	}

	//Create Lock
	rw->rwlock_lock=lock_create(rw->rwlock_name);
   	if (rw->rwlock_lock == NULL) {
   		kfree(rw->rwlock_name);
   		kfree(rw);
	    return NULL;
	}

	return rw;
}

void
rwlock_destroy(struct rwlock *rw_lock)
{
	KASSERT(rw_lock != NULL);

	        // add stuff here as needed
		sem_destroy(rw_lock->rwlock_semaphore);
		lock_destroy(rw_lock->rwlock_lock);
	    kfree(rw_lock->rwlock_name);
	    kfree(rw_lock);
}
void
rwlock_acquire_read(struct rwlock *rw_lock)
{
	lock_acquire(rw_lock->rwlock_lock);

	P(rw_lock->rwlock_semaphore);

	lock_release(rw_lock->rwlock_lock);
}

void
rwlock_release_read(struct rwlock *rw_lock){

	V(rw_lock->rwlock_semaphore);
}

void
rwlock_acquire_write(struct rwlock *rw_lock){

	lock_acquire(rw_lock->rwlock_lock);

	while(1)
	{
		if(rw_lock->rwlock_semaphore->sem_count!=MAX_READ)
			continue;
		else if(rw_lock->rwlock_semaphore->sem_count==MAX_READ)
		{
			rw_lock->rwlock_semaphore->sem_count=1;
			P(rw_lock->rwlock_semaphore);
			break;
		}
	}

	lock_release(rw_lock->rwlock_lock);
}

void
rwlock_release_write(struct rwlock *rw_lock){

	rw_lock->rwlock_semaphore->sem_count=MAX_READ-1;
	V(rw_lock->rwlock_semaphore);


}


