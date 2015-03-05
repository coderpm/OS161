/*
 * file_syscall.h
 *
 *  Created on: Mar 1, 2015
 *      Author: trinity
 */

#ifndef FILE_SYSCALL_H_
#define FILE_SYSCALL_H_


struct vnode;
struct lock;

struct file_descriptor{
	char *f_name;
	int f_flag;
	off_t f_offset;
	struct lock *f_lock;
	struct vnode *f_object;
	int reference_count;
};

int intialize_file_desc_tbl(struct file_descriptor *file_table[]);
struct file_descriptor* intialize_file_desc_con(struct vnode *v, int index, char *file_name, int open_type);
struct file_descriptor* file_descriptor_init(char *name);
void file_descriptor_cleanup(struct file_descriptor *fd);
int sys_open(userptr_t filename, int flags, int *return_val);
int sys_close(int fd);
int sys_read(int fd, userptr_t buf, size_t buflen, int *return_value);
int sys_write(int fd, userptr_t buf, size_t nbytes, int *return_value);
int dup2(int oldfd, int newfd, int *return_value);
int __getcwd(userptr_t buf, size_t buflen, int *return_value);
int chdir(userptr_t pathname);




#endif /* FILE_SYSCALL_H_ */
