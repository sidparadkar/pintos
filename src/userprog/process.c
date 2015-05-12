#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "devices/timer.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
	printf("in Process.c Execute function \n"); 
	printf("%s - Filename in Execution Function \n ",file_name); 
	char *fn_copy;
  tid_t tid;

	//This is the main entry point for any process.. the process_execute is called once the page has been loaded..it is here we will have to parse the arguments.

	struct child_status *child_thread_status;
	struct thread *curr;

	


  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

	char *command_name , *process_arguments; // we need to made the thread, and pass this thread
	//char **args = calloc((strlen (fn_copy)/2+1,sizeof(char *));
	command_name = strtok_r(fn_copy , " ",&process_arguments); // this will create a copy of the file name, along with the arguments seperated via a space.. this will help in debugging as well, when we need to see if a particular command is not working. This will be the parameter that we will pass inorder to create a new thread, instead of the current implementation of just passing the file_name and the copy, we will pass the command(file copy + arguments) and the args
	printf("%s - CommandName in Execution Function \n ",command_name); 
	printf("%s - process_Arguments in Execution Function \n ",process_arguments); 
  /* Create a new thread to execute FILE_NAME. */
//now passing the args and the command name which we want to execute.
  tid = thread_create (command_name , PRI_DEFAULT, start_process, process_arguments);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
	else
	{
		curr = thread_current();
		child_thread_status = calloc ( 1 , sizeof *child_thread_status);
//here we need to initialze the thread that will be taking care of executing the file..so we initialize the child_thread_status with the current Tid and set all the bools to false.
		if(child_thread_status != NULL)
		{
			child_thread_status->child_id = tid;
			child_thread_status->is_marked_for_exit = false;
			child_thread_status->syscall_wait_called = false;
			list_push_back(&curr->list_of_children, &child_thread_status->child_statuses);
		}
	}
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
	printf("in Process.c start_process \n");  
	
  char *file_name = file_name_;
printf ("fileName in start_process:%s: open failed\n", file_name);
  struct intr_frame if_;
  bool success;
	//this is the handler for the new process..we need to check the locks and check the status of the file load in order to proceed with the start..
	int load_status;
	


  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
	struct thread *t ;
	 t= thread_current();
printf ("fileName in start_process:%s: open failed\n", t->name);
  success = load (t->name, &if_.eip, &if_.esp);

  if (!success) 
    //if the process is not successful we set the status to -1
		load_status = -1 ;
	else
		load_status = 1; // success
	
	//here we will check if the current thread has a parent thread that is waiting on the completion of the load for this child(current thread).. we compare the status for each the child status the parent thread has and the loadstatus of the current thread..and we set the parent thread child's thread status property to the load_status..more for bookkeeping purposes...this will also help to keep track of what thread is doing what.	
	struct thread *current_thread;
	struct thread *curr_parent;
	current_thread = thread_current();
	curr_parent = get_thread(current_thread->parent_thread_id); // get the thread by using the property parent_thread_id on the current thread.
	if(curr_parent !=NULL)
	{
		lock_acquire(&curr_parent->child_lock);
		curr_parent->child_load_status = load_status;
		cond_signal(&curr_parent->child_cond, &curr_parent->child_lock);
		lock_release(&curr_parent->child_lock);
	}
	
	if(!success)
		thread_exit ();

	palloc_free_page (pg_round_down(file_name));
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
//this will have to change a lot because the process wait will occure if the current child thread is doing some processing and the parent thread has to wait upon the compeletion of this process.

	int status = -2123; // inintialze with a dummy number
	struct thread *current_thread;
	struct child_status *child_status = NULL;
	struct list_elem *element;

	current_thread = thread_current();
	element = list_tail (&current_thread->list_of_children);
	
	while ((element = list_prev (element)) != list_head (&current_thread->list_of_children))
 	{
		 child_status = list_entry(element, struct child_status, child_statuses);
		 if (child_status->child_id == child_tid)
		 		break;
 	}

	 if (child_status == NULL)
	 		status = -1;
	 else
	 {
		 lock_acquire(&current_thread->child_lock);
		 while (get_thread (child_tid) != NULL)
		 	cond_wait (&current_thread->child_cond,&current_thread->child_lock);
		 if (!child_status->is_marked_for_exit || child_status->syscall_wait_called)
		 		status = -1;
		 else
		 {
			 status = child_status->exit_status;
			 child_status->syscall_wait_called = true;
		 }
		 lock_release(&current_thread->child_lock);
	 }


	
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
// the process exit helps to define the way the process will have to behave when the exit is called..so the nature of the exit in our case has to depend on the status of the process of the process...couple of issues are that we need to be able to tell when the exit was due to a exception or just the process finished processing....
	struct thread *parent;
	struct list_elem *e;
	struct list_elem *next;
	struct child_status *child;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
	 /*free children list*/
	 e = list_begin (&cur->list_of_children);
	 while (e != list_tail(&cur->list_of_children))
	 {
		 next = list_next (e);
		 child = list_entry (e, struct child_status, child_statuses);
		 list_remove (e);
		 free (child);
		 e = next;
	 }

	 /*re-enable the file's writable property*/
	 if (cur->current_executable != NULL)
	 	file_allow_write (cur->current_executable);

	 /*free files whose owner is the current thread*/
	 close_file_by_owner (cur->tid);

	 parent = get_thread (cur->parent_thread_id);
	 if (parent != NULL)
	 {
		 lock_acquire (&parent->child_lock);
		 if (parent->child_load_status == 0)
			 parent->child_load_status = -1;
	
		 cond_signal (&parent->child_cond, &parent->child_lock);
		 lock_release (&parent->child_lock);
	 }
}


/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}


/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;



/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };



/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

//here is the main code..when setting up the stack we need to pass in the file name,because we are going to assign this name to the thread that will execute this file.
 static bool setup_stack (void **esp,const char *filename);
 static bool validate_segment (const struct Elf32_Phdr *, struct file *);
 static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
	printf ("load: %s: open failed\n", file_name);
  struct thread *t = thread_current ();
	//string threadName = t->name;
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open(file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
	t->current_executable = file;
	file_deny_write(file);
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp,file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
 static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
 static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
 static bool
setup_stack (void **esp,const char *file_name) 
{
		uint8_t *kpage;
		bool success = false;

		kpage = palloc_get_page (PAL_USER | PAL_ZERO);
		if (kpage != NULL) 
		  {
		    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
		    if (success) 
				{
				 *esp = PHYS_BASE -12;

				 uint8_t *argstr_head;
				 char *cmd_name = thread_current ()->name;
				 int strlength, total_length;
				 int argc =0;

				 /*push the arguments string into stack*/
				 strlength = strlen(file_name) + 1;
				 *esp -= strlength;
				 memcpy(*esp, file_name, strlength);
				 total_length += strlength;

				 /*push command name into stack*/
				 strlength = strlen(cmd_name) + 1;
				 *esp -= strlength;
				 argstr_head = *esp;
				 memcpy(*esp, cmd_name, strlength);
				 total_length += strlength;

				 /*set alignment, get the starting address, modify *esp */
				 *esp -= 4 - total_length % 4;

				 /* push argv[argc] null into the stack */
				 *esp -= 4;
				 * (uint32_t *) *esp = (uint32_t) NULL;

				 /* scan throught the file name with arguments string downward,
				 * using the cur_addr and total_length above to define boundary.
				 * omitting the beginning space or '\0', but for every encounter
				 * after, push the last non-space-and-'\0' address, which is current
				 * address minus 1, as one of argv to the stack, and set the space to
				 * '\0', multiple adjancent spaces and '0' is treated as one.
				 */
				 int i = total_length - 1;
				 /*omitting the starting space and '\0' */
				 while (*(argstr_head + i) == ' ' || *(argstr_head + i) == '\0')
				 {
					 if (*(argstr_head + i) == ' ')
					 {
					 	*(argstr_head + i) = '\0';
					 }
				 	i--;
				 }

				 /*scan through args string, push args address into stack*/
				 char *mark;
				 for (mark = (char *)(argstr_head + i); i > 0;
				 		i--, mark = (char*)(argstr_head+i))
				 {
					 /*detect args, if found, push it's address to stack*/
					 if ( (*mark == '\0' || *mark == ' ') &&
					 (*(mark+1) != '\0' && *(mark+1) != ' '))
					 {
						 *esp -= 4;
						 * (uint32_t *) *esp = (uint32_t) mark + 1;
						 argc++;
					 }
					 /*set space to '\0', so that each arg string will terminate*/
					 if (*mark == ' ')
					 	*mark = '\0';
				 }

				 /*push one more arg, which is the command name, into stack*/
				 *esp -= 4;
				 * (uint32_t *) *esp = (uint32_t) argstr_head;
				 argc++;

				 /*push argv*/
				 * (uint32_t *) (*esp - 4) = *(uint32_t *) esp;
				 *esp -= 4;

				 /*push argc*/
				 *esp -= 4;
				 * (int *) *esp = argc;

				 /*push return address*/
				 *esp -= 4;
				 * (uint32_t *) *esp = 0x0;
	 } else
			palloc_free_page (kpage);
			
			
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
