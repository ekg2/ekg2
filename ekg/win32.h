#ifdef __MINGW32__
# define NO_POSIX_SYSTEM "mingw"
#else
# define EKG2_WIN32_H
# undef NO_POSIX_SYSTEM
#endif

#ifndef EKG2_WIN32_H
#define EKG2_WIN32_H

#include "ekg2-config.h"

#if 0
	typedef unsigned long guint32;
	typedef unsigned short guint16;
	typedef unsigned char guint8;
#endif

#include <windef.h>
#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef pipe
typedef unsigned __int64 u_int64_t;

struct utsname {
/* XXX, len */
	char sysname[30];
	char nodename[30];
	char release[30];
	char version[30];
	char machine[30];
};

#define THREAD(x) DWORD x(void *data)
typedef THREAD(thread_func_t);

# ifndef EKG2_WIN32_NOFUNCTION
pid_t fork(void);			/* unimpl */
HANDLE win32_fork(thread_func_t *addr, void *data);

/* fcntl.h */
#define F_SETFL		4
#define O_NONBLOCK	04000
int fcntl(int fd, int cmd, long arg);
/* ... */

int fchmod(int fildes, mode_t mode);	/* unimpl */
int pipe(int *filedes);

int ioctl(int fd, int request, void *flags);			/* BAD PROTOTYPE. I KNOW. XXX, emulate some things */
int uname(struct utsname *buf);					/* emulated ? */

#endif

#define EINPROGRESS WSAEINPROGRESS 

#define fileno(__F) ((__F)->_file)
#define sleep(x) Sleep(x * 1000)

#ifdef __cplusplus
}
#endif

#endif

