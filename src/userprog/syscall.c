#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

//this is the main class that we will need to edit in order to handle the system calls.  The syscalls provides the interface between a user process and the OS.
//so open, exit , read, write, exec, shutdown..
//All of these will need to be implemented in this class



struct file_descriptor
{
 int fd_num;
 tid_t owner;
 struct file *file_struct;
 struct list_elem elem;
};

/* a list of open files, represents all the files open by the user process
 through syscalls. */
struct list open_files;

/* the lock used by syscalls involving file system to ensure only one thread
 at a time is accessing file system */
struct lock fs_lock;

static void syscall_handler (struct intr_frame * ) ;
static void halt (void);
static void exit (int);
static pid_t exec (const char *);
static int wait (pid_t);
static bool create (const char*, unsigned);
static bool remove (const char *);
static int open (const char *);
static int filesize (int);
static int read (int, void *, unsigned);
static int write (int, const void *, unsigned);
static void seek (int, unsigned);
static unsigned tell (int);
static void close (int);
static struct file_descriptor *get_open_file (int);
static void close_open_file (int);
bool valid_ptr(const void *);
static int allocate_fd (void);
void close_file_by_owner(tid_t);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
	list_init(&open_files);
	lock_init(&fs_lock);
}

 static void syscall_handler (struct intr_frame *f ) 
{
  // ONE GIANT SWTICH CASE
 printf("syscall_handler \n"); 
 uint32_t *esp;
 esp = f->esp;
 if (!valid_ptr(esp) || !valid_ptr(esp + 1) ||
 !valid_ptr(esp + 2) || !valid_ptr(esp + 3))
 {
 	exit (-1);
 }
 else
 {
		 int syscall_number = *esp;
 		printf("Sys_num %d ", syscall_number);
		 switch (syscall_number)
		 {
			 case SYS_HALT:
				 halt ();
				 break;
			 case SYS_EXIT:
				 exit (*(esp + 1));
				 break;
			 case SYS_EXEC:
				 f->eax = exec ((char *) *(esp + 1));
				 break;
			 case SYS_WAIT:
				 f->eax = wait (*(esp + 1));
				 break;
			 case SYS_CREATE:
				 f->eax = create ((char *) *(esp + 1), *(esp + 2));
				 break;
			 case SYS_REMOVE:
				 f->eax = remove ((char *) *(esp + 1));
				 break;
			 case SYS_OPEN:
				 f->eax = open ((char *) *(esp + 2));
				 break;
			 case SYS_FILESIZE:
				 f->eax = filesize (*(esp + 1));
				 break;
			 case SYS_READ:
				 f->eax = read (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
				 break;
			 case SYS_WRITE:
				 f->eax = write (*(esp + 2), (void *) *(esp + 6), *(esp + 3));
				 break;
			 case SYS_SEEK:
				 seek (*(esp + 1), *(esp + 2));
				 break;
			 case SYS_TELL:
				 f->eax = tell (*(esp + 1));
				 break;
			 case SYS_CLOSE:
				 close (*(esp + 1));
				 break;
			 default:
			 	 break;
 			}
 		}
	}


// Terminates the current user program, returning status to the kernel.

static void
exit (int status)
{
 /* later on, we need to determine if there is process waiting for it */
 /* process_exit (); */
 struct child_status *child;
 struct thread *cur = thread_current ();
 printf ("%s: exit(%d)\n", cur->name, status);
 struct thread *parent = get_thread (cur->parent_thread_id);
 if (parent != NULL)
 {
 struct list_elem *e = list_tail(&parent->list_of_children);
 while ((e = list_prev (e)) != list_head (&parent->list_of_children))
 {
 child = list_entry (e, struct child_status, child_statuses);
 if (child->child_id == cur->tid)
 {
 lock_acquire (&parent->child_lock);
 child->is_marked_for_exit = true;
 child->exit_status = status;
 lock_release (&parent->child_lock);
 }
 }
 }
thread_exit ();
}

static void
halt (void)
{
 shutdown_power_off ();
}

static pid_t
exec (const char *cmd_line)
{
 /* a thread's id. When there is a user process within a kernel thread, we
 * use one-to-one mapping from tid to pid, which means pid = tid
 */
 tid_t tid;
 struct thread *cur;
 /* check if the user pinter is valid */
 if (!valid_ptr(cmd_line))
 {
 exit (-1);
 }

 cur = thread_current ();

 cur->child_load_status = 0;
 tid = process_execute (cmd_line);
 lock_acquire(&cur->child_lock);
 while (cur->child_load_status == 0)
 cond_wait(&cur->child_cond, &cur->child_lock);
 if (cur->child_load_status == -1)
 tid = -1;
 lock_release(&cur->child_lock);
 return tid;
}

static int
wait (pid_t pid)
{
 return process_wait(pid);
}

static bool
create (const char *file_name, unsigned size)
{
 bool status;

 if (!valid_ptr(file_name))
 exit (-1);

 lock_acquire (&fs_lock);
 status = filesys_create(file_name, size);
 lock_release (&fs_lock);
 return status;
}

static bool
remove (const char *file_name)
{
 bool status;
 if (!valid_ptr(file_name))
 exit (-1);

 lock_acquire (&fs_lock);
 status = filesys_remove (file_name);
 lock_release (&fs_lock);
 return status;
}

static int
open (const char *file_name)
{
 struct file *f;
 struct file_descriptor *fd;
 int status = -1;

 if (!valid_ptr(file_name))
 exit (-1);

 lock_acquire (&fs_lock);

 f = filesys_open (file_name);
 if (f != NULL)
 {
 fd = calloc (1, sizeof *fd);
 fd->fd_num = allocate_fd ();
 fd->owner = thread_current ()->tid;
 fd->file_struct = f;
 list_push_back (&open_files, &fd->elem);
 status = fd->fd_num;
 }
 lock_release (&fs_lock);
 return status;
}

static int
filesize (int fd)
{
 struct file_descriptor *fd_struct;
 int status = -1;
 lock_acquire (&fs_lock);
 fd_struct = get_open_file (fd);
 if (fd_struct != NULL)
 status = file_length (fd_struct->file_struct);
 lock_release (&fs_lock);
 return status;
}

static int
read (int fd, void *buffer, unsigned size)
{
 struct file_descriptor *fd_struct;
 int status = 0;

 if (!valid_ptr(buffer) || !valid_ptr(buffer + size - 1))
 exit (-1);

 lock_acquire (&fs_lock);

 if (fd == STDOUT_FILENO)
 {
 lock_release (&fs_lock);
 return -1;
 }

 if (fd == STDIN_FILENO)
 {
 uint8_t c;
 unsigned counter = size;
 uint8_t *buf = buffer;
 while (counter > 1 && (c = input_getc()) != 0)
 {
 *buf = c;
 buffer++;
 counter--;
 }
 *buf = 0;
 lock_release (&fs_lock);
 return (size - counter);
 }

 fd_struct = get_open_file (fd);
 if (fd_struct != NULL)
 status = file_read (fd_struct->file_struct, buffer, size);

 lock_release (&fs_lock);
 return status;
}

static int
write (int fd, const void *buffer, unsigned size)
{
 struct file_descriptor *fd_struct;
 int status = 0;

 if (!valid_ptr(buffer) || !valid_ptr(buffer + size - 1))
 exit (-1);

 lock_acquire (&fs_lock);

 if (fd == STDIN_FILENO)
 {
 lock_release(&fs_lock);
 return -1;
 }

 if (fd == STDOUT_FILENO)
 {
 putbuf (buffer, size);
 lock_release(&fs_lock);
 return size;
 }

 fd_struct = get_open_file (fd);
 if (fd_struct != NULL)
 status = file_write (fd_struct->file_struct, buffer, size);
 lock_release (&fs_lock);
 return status;
}


static void
seek (int fd, unsigned position)
{
 struct file_descriptor *fd_struct;
 lock_acquire (&fs_lock);
 fd_struct = get_open_file (fd);
 if (fd_struct != NULL)
 file_seek (fd_struct->file_struct, position);
 lock_release (&fs_lock);
 return ;
}

static unsigned
tell (int fd)
{
 struct file_descriptor *fd_struct;
 int status = 0;
 lock_acquire (&fs_lock);
 fd_struct = get_open_file (fd);
 if (fd_struct != NULL)
 status = file_tell (fd_struct->file_struct);
 lock_release (&fs_lock);
 return status;
}

static void
close (int fd)
{
 struct file_descriptor *fd_struct;
 lock_acquire (&fs_lock);
 fd_struct = get_open_file (fd);
 if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
 close_open_file (fd);
 lock_release (&fs_lock);
 return ;
}

static struct file_descriptor *
get_open_file (int fd)
{
 struct list_elem *e;
 struct file_descriptor *fd_struct;
 e = list_tail (&open_files);
 while ((e = list_prev (e)) != list_head (&open_files))
 {
 fd_struct = list_entry (e, struct file_descriptor, elem);
 if (fd_struct->fd_num == fd)
 return fd_struct;
 }
 return NULL;
}

static void
close_open_file (int fd)
{
 struct list_elem *e;
 struct list_elem *prev;
 struct file_descriptor *fd_struct;
 e = list_end (&open_files);
 while (e != list_head (&open_files))
 {
 prev = list_prev (e);
 fd_struct = list_entry (e, struct file_descriptor, elem);
 if (fd_struct->fd_num == fd)
 {
 list_remove (e);
 file_close (fd_struct->file_struct);
 free (fd_struct);
 return ;
 }
 e = prev;
 }
 return ;
}


/* The kernel must be very careful about doing so, because the user can
 * pass a null pointer, a pointer to unmapped virtual memory, or a pointer
 * to kernel virtual address space (above PHYS_BASE). All of these types of
 * invalid pointers must be rejected without harm to the kernel or other
 * running processes, by terminating the offending process and freeing
 * its resources.
 */
bool
valid_ptr(const void *usr_ptr)
{
 struct thread *cur = thread_current ();
 if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
 {
 return (pagedir_get_page (cur->pagedir, usr_ptr)) != NULL;
 }
 return false;
}

static int
allocate_fd ()
{
 static int fd_current = 1;
 return ++fd_current;
}

void
close_file_by_owner (tid_t tid)
{
	 struct list_elem *e;
	 struct list_elem *next;
	 struct file_descriptor *fd_struct;
	 e = list_begin (&open_files);
	 while (e != list_tail (&open_files))
	 {
		 next = list_next (e);
		 fd_struct = list_entry (e, struct file_descriptor, elem);
	 if (fd_struct->owner == tid)
	 {
		 list_remove (e);
		 file_close (fd_struct->file_struct);
		 free (fd_struct);
	 }
	 e = next;
 	}
}


