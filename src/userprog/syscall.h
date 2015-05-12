#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

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

void syscall_init (void);
typedef int pid_t;

/*
 void syscall_handler (struct intr_frame *);
 struct file_descriptor *get_open_file (int);
 void close_open_file (int);
 bool is_valid_ptr (const void *);
 int allocate_fd (void);
 void close_file_by_owner (tid_t);

 void halt (void);
 void exit (int);
 pid_t exec (const char *);
 int wait (pid_t);
 bool create (const char*, unsigned);
 bool remove (const char *);
 int open (const char *);
 int filesize (int);
 int read (int, void *, unsigned);
 int write (int, const void *, unsigned);
 void seek (int, unsigned);
 unsigned tell (int);
 void close (int);
*/
//void syscall_init (void);
void close_file_by_owner (tid_t);


#endif /* userprog/syscall.h */
