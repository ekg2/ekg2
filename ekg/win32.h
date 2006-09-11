#ifdef __MINGW32__
# define NO_POSIX_SYSTEM "mingw"
#else
# define EKG2_WIN32_H
# undef NO_POSIX_SYSTEM
#endif

#ifndef EKG2_WIN32_H
#define EKG2_WIN32_H

#include "ekg2-config.h"

#ifdef PLUGIN_SHARED_LIBS
# define EKG2_WIN32_SHARED_LIB "da! we want shared libs...... DLL's HELL! :> yeah, let's rock."
# define EKG2_WIN32_HELPERS
#endif

#if 0
	typedef unsigned long uint32_t;
	typedef unsigned short uint16_t;
	typedef unsigned char uint8_t;
#endif

#include <windef.h>
#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

struct timezone { /* XXX */
	int tz_minuteswest;     /* minutes west of Greenwich */
	int tz_dsttime;         /* type of DST correction */
};
/* shared */
#ifdef EKG2_WIN32_SHARED_LIB
# define EKG2_WIN32_NOFUNCTION 1
# error "Currently --enable-shared don't work automagicly pass --enable-static to ./configure if you really want. contact with me. (jid:darkjames@chrome.pl)"
# include "win32_helper.h"
#endif

#define THREAD(x) DWORD x(void *data)
typedef THREAD(thread_func_t);

# ifndef EKG2_WIN32_NOFUNCTION
pid_t fork(void);			/* unimpl */
HANDLE win32_fork(thread_func_t *addr, void *data);

/* fcntl.h */
#define F_SETFL		4
#define O_NONBLOCK      04000
int fcntl(int fd, int cmd, long arg);
/* ... */

int fchmod(int fildes, mode_t mode);	/* unimpl */
int pipe(int *filedes);

int gettimeofday(struct timeval *tv, struct timezone *tz);	/* emulated */
int ioctl(int fd, int request, void *flags);			/* BAD PROTOTYPE. I KNOW. XXX, emulate some things */
int uname(struct utsname *buf);					/* emulated ? */

#endif

#define EINPROGRESS WSAEINPROGRESS 

#define fileno(__F) ((__F)->_file)
#define sleep(x) Sleep(x * 1000)

#endif

