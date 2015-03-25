/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static bool check_priority(const struct list_elem *TH_A, const struct list_elem *TH_B, void *aux);
static bool check_lock_priority(const struct list_elem *Lock_A, const struct list_elem *Lock_B, void *aux);
static bool check_semaphore_lock_priority(const struct list_elem *Lock_A, const struct list_elem *Lock_B, void *aux);

static bool check_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  const struct thread *TH_A , *TH_B;
	TH_A = list_entry(a,const struct thread, elem);
	TH_B = list_entry(b,const struct thread, elem);
	return TH_A->priority < TH_B->priority;
}

static bool check_lock_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	const struct lock *A = list_entry(a,struct lock, elem_lock);
	const struct lock *B = list_entry(b,struct lock, elem_lock);
	return A->priority_lock < B->priority_lock;
}


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      //Project #1 : Priority Problem 2
      list_insert_ordered(&sema->waiters,&thread_current()-> elem,check_priority, NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  //project #1 : Priority Probelm #2
  struct thread *curr = thread_current();
  struct thread *topThread = NULL;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
	
	//project #1 : Priority Probelm #2
  //we need to make sure that the top thread in the semaphore's waiters list
  // has a higher priority then the current running thread. If yes then we need to
  // prempt the current thread ( yeild the processor)
  while(!list_empty (&sema->waiters))
	{
		topThread = list_entry (list_pop_front (&sema->waiters),struct thread, elem);
		thread_unblock ( topThread);
	} 
sema->value++;
  if( topThread != NULL && topThread->priority > curr->priority)
	{
		thread_yeild_current(curr); // only the thread with the highest priority can run..
	}

  
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
	
//project #1 : Priority Probelm #2
	lock->priority_lock = -1;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
	//project #1 : Priority Probelm #2
	// this is when stuff gets really confusing..cause now we need to acquire a lock and now 
	// we will have donations..and nest donations. So the idea is simple.. i think.. but the
	// way i am apporaching this is we have the list of waiters at the semaphore,and we know that the top thread of the waiters list needs to have the highest priority..even higher then the current thread inorder to acquire the lock. So first we need to make sure the current holder of the lock has a lower priority then the current thread..then the current thread will donate its priority to that "holder" thread. Now we need to keep moving down the list of the waiters in the semaphores waiters list and check if they have lower priority then the current thread..and then make the donation. Like the document suggests i am going to make the depth of the search == 8..so that means i will only go down 8 levels to make the donation.

	struct thread *curr;
	struct thread *current_lock_holder;
	struct lock *next_lock;
	int currentInterval;
	int MAX_INTERVAL=8;
	
	// the acquire lock operation needs to happen atomically
	enum intr_level old_level;
	old_level = intr_disable();
	curr = thread_current();
	current_lock_holder = lock->holder;
	next_lock = lock;
	currentInterval =0;

	if(current_lock_holder !=NULL)
		curr->lock_held_by=lock;// this tells us that the current thread has been blocked by following lock, which is held by a specific thread, which we can find out by seeing the holder property of the lock.

while(!thread_mlfqs && current_lock_holder !=NULL 
				&& current_lock_holder->priority < curr->priority)
{
	set_given_thread_priority(current_lock_holder, curr->priority, true);
	
	//digging time
	if(next_lock-> priority_lock < curr->priority)
	{
		//single donation
		next_lock-> priority_lock = curr->priority;
	}
	//multiple donations
	if(current_lock_holder->lock_held_by != NULL && currentInterval < MAX_INTERVAL)
	{
		//So this should work..i am getting the next lock that is blocking the current holder of the lock we ( currentThread) are blocked by. So we set that as the next lock..we make the current holder to be the current holder for that lock and increment the interval. this will essentially in the next loop iteration we will set the priority for this new current lock holder to be that of the main thread..check if the lock held by this new current holder has a low prioity and then repeat until MAX_INTERVAL...hopefully.
		next_lock = current_lock_holder->lock_held_by;
		current_lock_holder = current_lock_holder->lock_held_by->holder;
		currentInterval++;
	}
	else 
		break;
}
  sema_down (&lock->semaphore);

	lock->holder = curr;// after all that.. the current thread better have the lock it needs..
	if(!thread_mlfqs)
	{
		//reset everything..and add this lock to the thread list of held locks
		curr->lock_held_by=NULL;
		list_insert_ordered(&curr->locks,&lock->elem_lock,check_lock_priority,NULL);
	}
	intr_set_level(old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  //releasing the lock in theory should be the same as the acquire only in reverse...sooo...
  lock->holder = NULL;
  sema_up (&lock->semaphore);

	struct thread *curr = thread_current();
	enum intr_level old_level;
	old_level=intr_disable();
	
	if(!thread_mlfqs) // for part 3...of project 1..not priority for mlfqs..
	{
		list_remove ( &lock->elem_lock);
		lock->priority_lock = -1;
		if( list_empty(&curr->locks))
		{
			curr->is_donated_priority = false;
			thread_set_priority(curr->thread_original_priority);
		}
		// now the nasty part..and a little bit of assumption made by me..so remember when we aquired the lock..at the end we inserted the lock for the thread into the thread's locks list..using 
			//list_insert_ordered(&curr->locks,&lock->elem_lock,check_lock_priority,NULL);
// this ensures us that the lock with the highest priority is at the top of this list..so we can use this priority and set it as the priority of our current thread, because this priority will be higher then the current thread's original priority. So if there is at least one thread in the semaphores waiters list, we can use that thread's priority. If not then we have the reached the end and the must assigned the originial priority to the current thread.
		else
		{
			struct lock *topLock;
			topLock = list_entry ( list_front(&curr->locks),struct lock, elem_lock);
			if(topLock->priority_lock != -1)
			{
				set_given_thread_priority(curr, topLock->priority_lock,true);
			}
			else
			{
				thread_set_priority(curr->thread_original_priority);
			}
		}
	}
 intr_set_level(old_level);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
		int semaphore_priority;
  };

static bool check_semaphore_lock_priority(const struct list_elem *Lock_A, const struct list_elem *Lock_B,void *aux UNUSED)
{
	const struct semaphore_elem *sem_a = list_entry(Lock_A,struct semaphore_elem, elem);
  const struct semaphore_elem *sem_b = list_entry(Lock_B,struct semaphore_elem, elem);
	return (sem_a->semaphore_priority > sem_b->semaphore_priority);
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  //we need to put the waiters in the list with decending priority otherwise we wont be able to make the assumption the top thread has the highest.
	list_insert_ordered(&cond->waiters,&waiter.elem,check_semaphore_lock_priority,NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
