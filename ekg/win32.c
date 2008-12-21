#include "win32.h"

#ifdef NO_POSIX_SYSTEM

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <winbase.h>

#include "debug.h"

#define ERRNO_NOT 0	/* not implemented */

HANDLE win32_fork(thread_func_t addr, void *data) {
	HANDLE ret	= (HANDLE) -1;
	DWORD thread_id = 0;
	
	ret = CreateThread(NULL, 0, (void *) addr, data, 0, &thread_id);
	debug_function("NO_POSIX_SYSTEM: win32_fork() ADDR=0x%x data=0x%x; thread_id = %d result = %d\n", addr, data, thread_id, ret);

	if (!ret) {	/* fails */
		return NULL; /* -1 ? */
	}
	return ret;
}

pid_t fork(void) {
	debug_function("NO_POSIX_SYSTEM: fork() ON NO POSIX SYSTEM DON'T USE FORK() REIMPLEMENT FUNCTION TO USE win32_fork()\n");
	errno = ERRNO_NOT;
	return -1;
}

int fchmod(int fildes, mode_t mode) {
	errno = ERRNO_NOT;
	return -1;
}

int pipe(int filedes[2]) {
	HANDLE pread, pwrite;
	int res = CreatePipe(&pread, &pwrite, NULL, 0);

	debug_function("NO_POSIX_SYSTEM: pipe() read=%d write=%d result=%d\n", pread, pwrite, res);
	if (res == 0) { /* fails, XXX, errno && GetLastError? */
		return -1;
	}
	filedes[0] = (int) pread;
	filedes[1] = (int) pwrite;
	return 0;
}

int fcntl(int fd, int cmd, long arg) {
	if (cmd == F_SETFL) {
		int no_block = -1;
		if (arg & O_NONBLOCK) { no_block = 1; arg -= O_NONBLOCK; }

		debug_function("NO_POSIX_SYSTEM: fcntl() fd: %d F_SETFL  _NO_BLOCK: %d REST_args: %d\n", fd, no_block, arg);
		/* XXX */
		if (no_block != -1) ioctlsocket(fd, FIONBIO, (u_long *) &no_block);

		if (arg) { errno = ERRNO_NOT; return -1; }
		return 0;
	}
	debug_function("NO_POSIX_SYSTEM: fcntl() fd: %d cmd:%d arg:%d\n", fd, cmd, arg);

	errno = ERRNO_NOT;
	return -1;
}

int ioctl(int fd, int request, void *flags) {
	if (request == FIONBIO) {
		return fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	debug_function("NO_POSIX_SYSTEM: ioctl() fd: %d req: %d flags: 0x%x", fd, request, flags);
	errno = ERRNO_NOT;
	return -1;
}


/* code of (gettimeofday && friends) in windows ripped from http://www.tcpdump.org/lists/workers/2005/12/msg00003.html
 *    (c) Gisle Vanem 
 */

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
 #define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
 #define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif

static u_int64_t filetime_to_unix_epoch (const FILETIME *ft) {
	u_int64_t res = (u_int64_t) ft->dwHighDateTime << 32;
	res |= ft->dwLowDateTime;
	res /= 10;		     /* from 100 nano-sec periods to usec */
	res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
	return res;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
	FILETIME  ft;
	u_int64_t tim;

	if (!tv) {
		errno = EINVAL;
		return -1;
	}
	GetSystemTimeAsFileTime (&ft);
	tim = filetime_to_unix_epoch (&ft);
	tv->tv_sec  = (long) (tim / 1000000L);
	tv->tv_usec = (long) (tim % 1000000L);
	return 0;
}

int uname(struct utsname *buf) {
	if (!buf) {
		errno = EFAULT;
		return -1;
	}

	/* XXX, NO_POSIX_SYSTEM */
	return -1;
}

#endif

