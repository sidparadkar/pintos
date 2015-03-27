#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "threads/math.h"
#ifdef USERPROG
#include "userprog/process.h"


#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;


/*[Project #1: AlarmClock]  This is when the sechduler has not yet sechduled this thread, so intead of  using the old time_sleep implementation where it would spin, this list will be able to hold this thread along with any other, with an assocaited "SLEEP" time.*/
static struct list waiting_list;
static struct lock waiting_list_lock;
/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
static int load_avg;

static void kernel_thread (thread_func *, void *aux UNUSED);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

//since there is not FIFO que for pintos(they use a doubly linked list), we need a way to order our list of waiting threads. We will order then by amount of the time is left for them to be "sleeping"..this function acts like a handler for the list function delegat(list_less_func .. see list.c class)..it takes two arugments thread elements and compares them and returns true or false if the first element is smaller then the second..

bool sleep_list_less_func(const struct list_elem *a , const struct list_elem *b,void *aux); // the signature for the less function needs to match the one needed for the list,since we are passing the function as a argument.


static bool check_priority(const struct list_elem *TH_A, const struct list_elem *TH_B, void *aux);


static bool check_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  const struct thread *TH_A , *TH_B;
	TH_A = list_entry(a,const struct thread, elem);
	TH_B = list_entry(b,const struct thread, elem);
	return TH_A->priority > TH_B->priority;
}
/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  //initialize list for waiting threads
  list_init (&waiting_list);
	lock_init (&waiting_list_lock);	

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
	
	load_avg = 0;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;	

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

   //[Project #1: AlarmClock]//	
   wakeup_thread();
}

//[Project #1: AlarmClock]//
void wakeup_thread(void)
{
	/*
		Essentailly checks the thread in the waiting_list and picks out the threads which have the sleeping_time value less then the current clock tick.It keeps doing this until it gets to a thread which has a value greater than the current clock tick, at which point it breaks the loop.	
*/
	struct thread *t;
	struct list_elem *currentElement;
	enum intr_level old_level;
	
	if(list_empty(&waiting_list))
		return;
	
	currentElement = list_begin(&waiting_list);
	
	while(currentElement != list_end(&waiting_list))
	{
		struct list_elem *temp;
		temp = list_next (currentElement);
		
		t = list_entry ( currentElement, struct thread , elem);
		if(t->sleeping_time > timer_ticks())
			break;
		
		old_level = intr_disable();
		list_remove(currentElement);
		thread_unblock(t);
		intr_set_level(old_level);

		currentElement = temp;
	}				
}

//[Project #1: AlarmClock]//
bool sleep_list_less_func(const struct list_elem *a , const struct list_elem *b, void *aux UNUSED)
{
	const struct thread *TH_A , *TH_B;
	TH_A = list_entry(a,const struct thread, elem);
	TH_B = list_entry(b,const struct thread, elem);
	return TH_A->sleeping_time < TH_B->sleeping_time;
}

//[Project #1: AlarmClock]//
void thread_sleep(int64_t sleep_time)
{
	//this is the main function that is going to be called when the timer.c's sleep functions is called . This will place the threads with their assocaited sleeping_time's within the waiting_list.This needs to be done atomically(no interupts). We take the current thread make sure that the current thread was running, and then place the current thread with a sleeping_time assocaited with it in the waiting list , and block the thread. This will place the thread in the THREAD_BLOCKED status, so when we wake up the thread, essentailly we are going from THREAD_BLOCKED status to THREAD_RUNNING.
		if(sleep_time <=0)
			return;

		struct thread *currentTH = thread_current ();
		enum intr_level old_level;

		ASSERT(!intr_context());
		ASSERT(currentTH->status == THREAD_RUNNING);

		old_level = intr_disable();

		currentTH->sleeping_time = sleep_time + timer_ticks();
		list_insert_ordered (&waiting_list , &currentTH->elem,sleep_list_less_func,NULL);
		thread_block();

		intr_set_level (old_level);
		
}
/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);
	if (thread_mlfqs)
	{
		 calc_recent_cpu (t,false, NULL);
		 calc_advanced_priority (t,false, NULL);
		 calc_recent_cpu (thread_current(),false, NULL);
		 calc_advanced_priority(thread_current(),false,NULL);
	 }


	if(t->priority > thread_current()->priority)
	{
		//shouldnt happen...
		thread_yeild_current(thread_current());
	}
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //need to make sure the thread with the highest priority is run first
	list_insert_ordered(&ready_list, &t->elem, check_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
	{
		// here we need to make sure that the ready list is ordred in a decending order..so that the thread with the highest priority is the first on the ready list.
    //list_push_back (&ready_list, &cur->elem);
		
		list_insert_ordered(&ready_list,&cur->elem,check_priority,NULL);
	}	
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

void thread_yeild_current (struct thread *cur) 
{
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
	{
		// here we need to make sure that the ready list is ordred in a decending order..so that the thread with the highest priority is the first on the ready list.
    //list_push_back (&ready_list, &cur->elem);
		
		list_insert_ordered(&ready_list,&cur->elem,check_priority,NULL);
	}	
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  //thread_current ()->priority = new_priority;
	set_given_thread_priority(thread_current(),new_priority,false);
}

void set_given_thread_priority(struct thread *cur , int priority_new, bool donated_priority)
{
	enum intr_level old_level;
	old_level = intr_disable();
	//...this is where we will handle how to give a particular priority to a thread..but we need to make sure to preserve the current aka orignial priority of the thread. This we will do by taking into account the fact that the current thread has a priority assocaited with it..sooo lets try...
	if(!donated_priority)
	{
		//here we have a senario where we are setting the priority for the thread , where the priority for the thread was might have been originally donated.. so here we need to first check if the cur thread's priority is > then the new priority we are setting. If not this means we are setting the curr thread back to its original priority and we need to set the original_priority property of the thread accordingly. If the curr thread's priority was not donated, then we need to set the curr thread origial priority and current priority equal to the new priority, because the value for the priority was not donated..
		if(cur->is_donated_priority && cur->priority >= priority_new)
			cur->thread_original_priority = priority_new;
		else
			cur->priority = cur->thread_original_priority = priority_new;
	}
	else
	{
			cur->priority=priority_new;
			cur->is_donated_priority=true;
	}
//we need to ensure that the readylist has all the threads in decending order of priority
// so i am going to remove the elem and reinsert it using the list_insert_ordered if the thread is in THREAD_READY state..if not and the thread is in THREAD_RUNNING state we need to check if the curr running thread has a higher priority then the next thread in the runlist..if not then we need to prempt and yeild the cpu.
	 if(cur->status == THREAD_READY)
		{
			list_remove(&cur->elem);
			list_insert_ordered(&ready_list, &cur->elem,check_priority,NULL);
		}
	 else if(cur->status == THREAD_RUNNING)
		{
			struct thread *temp ;
			temp = list_entry(list_begin(&ready_list), const struct thread,elem);
			if(temp->priority > cur->priority)
			{
				thread_yeild_current(cur);
			}	
		}
	intr_set_level(old_level);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

//[PROJECT 1 : PART 3] // all of the priority functions
void calc_priority_adv(struct thread *curr,void *aux UNUSED)
{
	if (curr != idle_thread)
 {
	 /* convert to integer nearest for (recent_cpu / 4) instead
	 * of the whole priority.
	 */
curr->priority = 31-CONVERT_TO_INT_NEAREST(DIV_INT (curr->recent_cpu, 4)) - curr->nice * 2;
	 /* Make sure it falls in the priority boundry */
	 if (curr->priority < 0)
	 {
	 	 curr->priority = 0;
	 }
	 else if (curr->priority > 31)
	 {
	 	curr->priority = 31;
	 }
 }
}
void thread_calc_advanced_priority (void)
{
	calc_advanced_priority( thread_current() , false, NULL);
}

//The ForAll cause we need to calcualate the priority every 4th tick
void calc_advanced_priority (struct thread *curr, bool ForALL, void *aux UNUSED)
{
	if(!ForALL)
	{
		calc_priority_adv(curr,NULL);
	}
	else
	{
		thread_foreach (calc_priority_adv, NULL);
		
		if (!list_empty (&ready_list))
		 {
		 		list_sort (&ready_list, check_priority, NULL);
		 }
	}
}

void calculate_recent_cpu(struct thread *curr, void *aux)
{
	//recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice
 
	ASSERT (is_thread (curr));
 if (curr != idle_thread)
 {
	 /* load_avg and recent_cpu are fixed-point numbers */
	 int load = MULT_INT (load_avg, 2);
	 int coefficient = DIVIDE (load, ADD_INT (load, 1));
	 curr->recent_cpu = ADD_INT (MULTIPLE (coefficient, curr->recent_cpu),
	 curr->nice);
 }

}


void thread_calc_recent_cpu (void)
{
	calc_recent_cpu (thread_current (),false, NULL);

}

void calc_recent_cpu (struct thread *curr, bool ForALL, void *aux)
{
	if(!ForALL)
	{
		calculate_recent_cpu(curr,NULL);
	}
	else
	{
		thread_foreach (calculate_recent_cpu, NULL);
	}
}



void calc_load_avg (void)
{
	// Finally, load_avg, often known as the system load average, estimates the average number of threads ready to run over the past minute. Like recent_cpu, it is an exponentially weighted moving average. Unlike priority and recent_cpu, load_avg is system-wide, not thread-specific. At system boot, it is initialized to 0. Once per second thereafter, it is updated according to the following formula:

//load_avg = (59/60)*load_avg + (1/60)*ready_threads,

//where ready_threads is the number of threads that are either running or ready to run at time of update (not including the idle thread). 

	struct thread *cur;
	 int ready_list_threads;
	 int ready_threads;

	 cur = thread_current ();
	 ready_list_threads = list_size (&ready_list);

	 if (cur != idle_thread)
	 {
	 ready_threads = ready_list_threads + 1;
	 }
	 else
	 {
	 ready_threads = ready_list_threads;
	 }
	 load_avg = MULTIPLE (DIV_INT (CONVERT_TO_FP (59), 60), load_avg) +
	 MULT_INT (DIV_INT (CONVERT_TO_FP (1), 60), ready_threads);
}
/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int value) 
{
  // so when we set the niceness of the thread we need to recalc the priority of the thread, and if the new priority of the thread is lower than the next thread in the ready list we have to prempt the thread...this will stop any thread from starving, because the recent_cpu for a thread that has never had the cpu will be 0, so if the nice value we are setting is small enough the thread should be sechudled next.
	
struct thread *cur;
cur = thread_current ();
cur->nice = value;

 if (cur != idle_thread)
 {
	 if (cur->status == THREAD_READY)
	 {
		 enum intr_level old_level;
		 old_level = intr_disable ();
		list_remove (&cur->elem);
		 list_insert_ordered (&ready_list, &cur->elem, check_priority, NULL);
		 intr_set_level (old_level);
	  }
	 else if (cur->status == THREAD_RUNNING )
	 {
			bool temp_value = list_entry (list_begin (&ready_list),struct  thread,elem)->priority > cur->priority ;
			if(temp_value)
				thread_yeild_current (cur);
	 }
 }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return CONVERT_TO_INT_NEAREST (MULT_INT (load_avg, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return CONVERT_TO_INT_NEAREST (MULT_INT (thread_current ()->recent_cpu,100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
	t->sleeping_time=0;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

	//init all this priority stuff
	t->thread_original_priority = priority;
	t->is_donated_priority = false;
	t->lock_held_by = NULL;
	list_init(&t->locks);
	
	// part 3: project 1
	if(thread_mlfqs)
	{
		if (t == initial_thread)
		 {
			 t->nice = 0;
			 t->recent_cpu = 0;
		 }
	 else
	 {
			 t->nice = thread_get_nice ();
			 t->recent_cpu = thread_get_recent_cpu ();
	 }
	}	
	

}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
