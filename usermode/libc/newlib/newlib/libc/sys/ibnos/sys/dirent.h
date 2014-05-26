/* FIXME: From sys/sysvi386/sys */
#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

/*
 * This file was written to be compatible with the BSD directory
 * routines, so it looks like it.  But it was written from scratch.
 * Sean Eric Fagan, sef@Kithrup.COM
 *
 * Modified by michael@fds-team.de for ibnos.
 */

typedef struct __dirdesc {
	int		dd_fd;
	long	dd_loc;
	long	dd_size;
	char	*dd_buf;
	int		dd_len;
	long	dd_seek;
} DIR;

# define __dirfd(dp)	((dp)->dd_fd)

DIR *opendir (const char *);
struct dirent *readdir (DIR *);
int readdir_r (DIR *__restrict, struct dirent *__restrict,
              struct dirent **__restrict);
void rewinddir (DIR *);
int closedir (DIR *);

#include <sys/types.h>

#define MAXNAMLEN	255

#define d_ino	d_fileno	/* compatibility */

#define	DT_UNKNOWN	 0
#define	DT_FIFO		 1
#define	DT_CHR		 2
#define	DT_DIR		 4
#define	DT_BLK		 6
#define	DT_REG		 8
#define	DT_LNK		10
#define	DT_SOCK		12
#define	DT_WHT		14

struct dirent {
	off_t			d_off;
	unsigned long	d_fileno;
	unsigned short	d_reclen;
	unsigned short	d_namlen;
	char			d_name[MAXNAMLEN + 1];
	char			d_type;
};

/* FIXME: include definition of DIRSIZ() ? */

#endif
