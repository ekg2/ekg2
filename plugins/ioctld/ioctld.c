/* $Id$ */

/*
 *  (C) Copyright 2002 Pawe³ Maziarz <drg@infomex.pl>
 *                     Wojtek Kaniewski <wojtekka@irc.pl>
 *                     Robert J. Wo¼ny <speedy@ziew.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef __sun
#define _XOPEN_SOURCE 600
#endif
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#  include <sys/kbio.h>	
#endif
#ifdef __sun /* Solaris */
#  include <sys/kbd.h>
#  include <sys/kbio.h>
#endif 
#ifdef __linux__
#  include <linux/cdrom.h>		  
#  include <linux/kd.h>			 
#endif

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ioctld.h"

#ifndef PATH_MAX
#  ifdef MAX_PATH
#    define PATH_MAX MAX_PATH
#  else
#    define PATH_MAX _POSIX_PATH_MAX
#  endif
#endif


char sock_path[PATH_MAX] = "";

int blink_leds(int *flag, int *delay) 
{
    	int s, fd;
#ifdef __sun 
	int restore_data;

	if ((fd = open("/dev/kbd", O_RDONLY)) == -1)
		return -1;

	ioctl(fd, KIOCGLED, &restore_data);
#else
	if ((fd = open("/dev/console", O_WRONLY)) == -1)
		fd = STDOUT_FILENO;
#endif

	for (s = 0; flag[s] >= 0 && s <= IOCTLD_MAX_ITEMS; s++) {
#ifdef __sun
		int leds = 0;
		/* tak.. na sunach jest to troszkê inaczej */
		if (flag[s] & 1) 
			leds |= LED_NUM_LOCK;
		if (flag[s] & 2) 
			leds |= LED_SCROLL_LOCK;
		if (flag[s] & 4) 
			leds |= LED_CAPS_LOCK; 

		ioctl(fd, KIOCSLED, &leds);
#else
	    	ioctl(fd, KDSETLED, flag[s]);
#endif 
		if (delay[s] && delay[s] <= IOCTLD_MAX_DELAY)
			usleep(delay[s]);
	}

#ifdef __sun
	ioctl(fd, KIOCSLED, &restore_data);
#else
	ioctl(fd, KDSETLED, 8);
#endif
	
	if (fd != STDOUT_FILENO)
		close(fd);
	
	return 0;
}

int beeps_spk(int *tone, int *delay)
{
    	int s;
#ifndef __sun
	int fd;

    	if ((fd = open("/dev/console", O_WRONLY)) == -1)
		fd = STDOUT_FILENO;
#endif
		
	for (s = 0; tone[s] >= 0 && s <= IOCTLD_MAX_ITEMS; s++) {

#ifdef __FreeBSD__
		ioctl(fd, KIOCSOUND, 0);
#endif

#ifndef __sun
	    	ioctl(fd, KIOCSOUND, tone[s]);
#else
		/* ¿a³osna namiastka... */
		putchar('\a');
		fflush(stdout);
#endif
		if (delay[s] && delay[s] <= IOCTLD_MAX_DELAY)
			usleep(delay[s]);
	}

#ifndef __sun
	ioctl(fd, KIOCSOUND, 0);
	
	if (fd != STDOUT_FILENO)
		close(fd);
#endif

	return 0;
}

void quit() 
{
    	unlink(sock_path);
	exit(0);
}

int main(int argc, char **argv) 
{
    	int sock, length;
	struct sockaddr_un addr;
	struct action_data data;
	
	if (argc != 2) {
		printf("program ten nie jest przeznaczony do samodzielnego wykonywania!\n");
	    	exit(1);
	}
	
	if (strlen(argv[1]) >= PATH_MAX)
	    	exit(2);
	
	signal(SIGQUIT, quit);
	signal(SIGTERM, quit);
	signal(SIGINT, quit);
	
	umask(0177);

	close(STDERR_FILENO);
	
	strcpy(sock_path, argv[1]);
	
	unlink(sock_path);

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) 
		exit(1);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, sock_path);
	length = sizeof(addr);

	if (bind(sock, (struct sockaddr *)&addr, length) == -1) 
	    	exit(2);

	chown(sock_path, getuid(), -1);

	while (1) {
	    	if (recvfrom(sock, &data, sizeof(data), 0, (struct sockaddr *)&addr,&length) == -1) 
		    	continue;
		
		if (data.act == ACT_BLINK_LEDS)  
		    	blink_leds(data.value, data.delay);

		else if (data.act == ACT_BEEPS_SPK) 
		    	beeps_spk(data.value, data.delay);
	}
	
	exit(0);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
