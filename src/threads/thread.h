#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/file.h"// since we are adding the file 'object' to our thread.

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING       /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

//child status struct..essentially this will keep track of the child and its current status. The properties syscall_wait_called and is_marked_for_exit will be used to keep track of any thread that is, (A) is been waited on by the system call wait (B) has been marked for exit by the exist system call. This will help us keep track of threads being used for system calls.
struct child_status
{
	tid_t child_id;
	bool is_marked_for_exit;
	bool syscall_wait_called;
	int exit_status;
	struct list_elem child_statuses;

};

struct waiting_child
 {
	 tid_t child_id; // thread_id
	 int child_exit_status;
	bool is_terminated_by_kernel;
	 bool has_been_waited;
	 struct list_elem elem_waiting_child; // itself
 };



/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[50];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int64_t sleeping_time;                /*//[Project #1: AlarmClock]// Time for the thread will be sleeping for */ 
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    tid_t parent_thread_id;
		
		// we need to a way to store the child thread's status depending upon the exit status of its system load call. So essentially we will have 3 different status's.
		// --> 0 chilling..nothing has been loaded
		// --> -1 failed in load
		// --> 1 sucessfully loaded 

		int child_load_status;
		//our monitors(aka locks) which we will use to keep track if the child thread is being handled by the system call to wait, and if the child is waiting for load to occur.
	   struct lock child_lock;
		 struct condition child_cond;

		 struct list list_of_children;
		 // this file will represent the current file that is being run by the thread. so this file will be locked when the thread is executing it , and when the thread is done executing the file will again be opened to write.
		 struct file *current_executable;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
	
    int thread_original_priority;
    bool is_donated_priority; // to account for a thread's priority having acquired from some other thread.
    struct list locks;
    struct lock *lock_held_by;
 
    int nice;
    int recent_cpu;
 
    

  };


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

//[Project #1: AlarmClock]//
void thread_sleep (int64_t sleep_time);
void wakeup_thread(void );

//[Project #1: prioritySchedule]//
void thread_yeild_current(struct thread *);
void set_given_thread_priority( struct thread* , int, bool);
void donate_thread_priority(struct thread * , int);


//[Project #1: AdvancedPriority]

void thread_calc_advanced_priority (void);
void calc_advanced_priority (struct thread *, bool ForALL, void *aux);
void thread_calc_recent_cpu (void);
void calc_recent_cpu (struct thread *, bool ForALL, void *aux);
void calc_load_avg (void);

struct thread *get_thread(tid_t);

#endif /* threads/thread.h */
