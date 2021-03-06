/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.
struct cv *cv_male;
struct cv *cv_female;
struct cv *cv_matchmaker;


static volatile int male_count,female_count,matchmaker_count;
struct lock *matching_lock;



void whalemating_init() {
  /*
   * Author: Student
   * Declaring semaphores for male,female and matchmaker
   *
   */
	cv_male= cv_create("male");
	cv_female=cv_create("female");
	cv_matchmaker=cv_create("matchmaker");
	matching_lock= lock_create("matching_lock");
	male_count=0;
	female_count=0;
	matchmaker_count=0;

	return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {

	cv_destroy(cv_male);
	cv_destroy(cv_female);
	cv_destroy(cv_matchmaker);
	lock_destroy(matching_lock);
  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;


	  male_start();
	  lock_acquire(matching_lock);
	  male_count++;

	  if(female_count==0 || matchmaker_count ==0){
		  cv_wait(cv_male, matching_lock);
		  male_end();
	  }
	  else if(female_count>0 && matchmaker_count >0){
		 while(!(male_count== 0 || female_count== 0 || matchmaker_count ==0)){
		 		  if(cv_male->cv_wchan != NULL){
		 			 cv_signal(cv_male, matching_lock);
		 			 male_count--;
		 			 cv_signal(cv_female, matching_lock);
		 			 cv_signal(cv_matchmaker, matching_lock);

		 		  }
		 		  else{
		 			 cv_signal(cv_female, matching_lock);
		 			 cv_signal(cv_matchmaker, matching_lock);
		 			 male_count--;
		 		  }

		 		  male_end();

		 	  }
	  }



	  lock_release(matching_lock);



	// Implement this function
  /*
   * Author: Student
   * Implementing the semaphore for male
   */


//male_end();



  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  female_start();

  lock_acquire(matching_lock);
  	  female_count++;
  	  if(male_count ==0 || matchmaker_count== 0){
  		  cv_wait(cv_female, matching_lock);
  		female_end();
  	  }
  	  else if(male_count>0 && matchmaker_count >0){
  		while(!(male_count== 0 || female_count== 0 || matchmaker_count ==0)){
  		  		  if(cv_female->cv_wchan!= NULL){
  		  			  cv_signal(cv_male, matching_lock);
  		  			  male_count--;
  		  			  cv_signal(cv_female, matching_lock);
  		  			  female_count--;
  		  			  cv_signal(cv_matchmaker, matching_lock);
  		  			  matchmaker_count--;

  		  		  }
  		  		  else{
  		  			  cv_signal(cv_male, matching_lock);
  		  			  male_count--;
  		  			  cv_signal(cv_matchmaker, matching_lock);
  		  			  matchmaker_count--;
  		  			  female_count--;
  		  		  }

  		  		female_end();

  		  	  }
  	  }


  	  lock_release(matching_lock);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;

  matchmaker_start();
  lock_acquire(matching_lock);
  	  matchmaker_count++;
    	  if(male_count ==0 || female_count==0){
    		  cv_wait(cv_matchmaker, matching_lock);
    		  matchmaker_end();
    	  }
    	  else if(male_count>0 && female_count >0){
    		  while(!(male_count== 0 || female_count== 0 || matchmaker_count ==0)){
    		      if(cv_matchmaker->cv_wchan!= NULL){
    		         cv_signal(cv_male, matching_lock);
    		         male_count--;
    		         cv_signal(cv_female, matching_lock);
    		         female_count--;
    		         cv_signal(cv_matchmaker, matching_lock);
    		         matchmaker_count--;

    		      }
    		      else{
    		        cv_signal(cv_male, matching_lock);
    		        male_count--;
    		        cv_signal(cv_female, matching_lock);
    		        female_count--;
    		      	matchmaker_count--;
    		      }

        		  matchmaker_end();
    		  }
    	  }


    	  lock_release(matching_lock);
  //matchmaker_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}




/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct cv * cross_intersection;
struct lock * lock_intersection;
bool status_busy;
static volatile int quadrant[4];	//To signify which parts of the quadrant one is occupying, 0 signifies free and 1 signifies taken

void stoplight_init() {

	cross_intersection= cv_create("cross0");


	lock_intersection= lock_create("intersection_lock");
	status_busy=false;
	quadrant[0]=0;
	quadrant[1]=0;
	quadrant[2]=0;
	quadrant[3]=0;
  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
	cv_destroy(cross_intersection);

	lock_destroy(lock_intersection);
  return;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;
  /*
   * Author: Student
   */
  
  lock_acquire(lock_intersection);

  while(!(quadrant[direction]==0 && (quadrant[(direction+3)%4]==0))){
	  cv_wait(cross_intersection,lock_intersection);

  }//End of while

  quadrant[direction]=1;
  quadrant[(direction+3)%4]=1;

  lock_release(lock_intersection);

  inQuadrant(direction);
  inQuadrant((direction+3)%4);
  leaveIntersection();

  lock_acquire(lock_intersection);
  quadrant[direction]=0;
  quadrant[(direction+3)%4]=0;

	cv_signal(cross_intersection,lock_intersection);

  lock_release(lock_intersection);


  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;

  /*
   *  Author: Student
     */

  lock_acquire(lock_intersection);
  while(!(quadrant[direction]==0 && (quadrant[(direction+3)%4]==0)&& (quadrant[(direction+2)%4]==0))){
	  cv_wait(cross_intersection,lock_intersection);
  }//End of while

  quadrant[direction]=1;
  quadrant[(direction+3)%4]=1;
  quadrant[(direction+2)%4]=1;

  lock_release(lock_intersection);

  inQuadrant(direction);
  inQuadrant((direction+3)%4);
  inQuadrant((direction+2)%4);
  leaveIntersection();

  lock_acquire(lock_intersection);
  quadrant[direction]=0;
  quadrant[(direction+3)%4]=0;
  quadrant[(direction+2)%4]=0;

	cv_signal(cross_intersection,lock_intersection);

  lock_release(lock_intersection);


  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.

  /*
   *  Author: Student
     */

  lock_acquire(lock_intersection);

  while(!(quadrant[direction]==0)){
	  cv_wait(cross_intersection,lock_intersection);
  }//End of while

  quadrant[direction]=1;

  lock_release(lock_intersection);

  inQuadrant(direction);
  leaveIntersection();

  lock_acquire(lock_intersection);
  quadrant[direction]=0;

	cv_signal(cross_intersection,lock_intersection);

  lock_release(lock_intersection);

  V(stoplightMenuSemaphore);
  return;
}
