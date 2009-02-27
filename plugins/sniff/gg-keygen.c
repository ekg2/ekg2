/*
 *  (C) Copyright 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* ustaw co chcesz */

static const char digit[] = "\0abcdefghijklmnoprstuwxyz";	/* bo kto tak naprawde korzysta z trudnych hasel? */
// static const char digit[] = "\0abcdefghijklmnoprstuwxyzABCDEFGHIJKLMOPRSTUWXYZ1234567890";

#define MAX_PASS_LEN 15	/* dlugosc hasla, tak naprawde to jest+1, nie przejmowac sie. */

#define ULTRA_DEBUG	0	/* sprawdza czy dobrze generujemy hasla (w/g digit, b. niepotrzebne i b. wolne) */
#define ULTRA_VERBOSE	0	/* rysuje kropki */
#define ULTRA_SAFE	0	/* sprawdza czy nie bedziemy rysowac po pamieci jesli haslo zacznie miec wiecej niz MAX_PASS_LEN znakow */
#define ULTRA_SMP	4	/* ile masz procków? (jak masz 1 wpisz 0 - wyłacza SMP) */

#define NOT_STOP_ON_FIRST 0

/* sample, smb has this seed/hash. 
 *	change it to your data.
 *
 *	you can benchmark && check if you gg-keygen.c generate good data with it.
 *	tested under athlon-xp 2500 XP+
 */

#if 0	/* kasza */	/*   0.115s */
#define SEED 0xe2cf3809
#define HASH 0x4d940c10
#endif

#if 0	/* agakrht */	/* 7.834s */
#define SEED 0xd3c742b6
#define HASH 0x9f9b9205
#endif

/* with apended 'q2' to digit: [static const char digit[] = "\0abcdefghijklmnoprstuwxyzq2"] */
#if 0	/* qwerty2 */	/* */
#define SEED 0xb2b9eec8
#define HASH_SHA1 "a266db74a7289913ec30a7872b7384ecc119e4ec"
#endif

#if 0		/* 5.154s */
#define TEXT "alamaka"		/* algo test */
#endif

/* nie zmieniać chyba że wiesz co robisz */

#define DIGIT_SIZE (sizeof(digit)-2)	/* -2 bo początkowe \0 i końcowe \0, yeah shitty. */ 

#if ULTRA_SMP	/* dla smp */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

#if ULTRA_VERBOSE
#warning "Dots sucks in SMP mode, disabling"
#undef ULTRA_VERBOSE
#define ULTRA_VERBOSE 0
#endif

static pid_t pids[ULTRA_SMP];
static int   pipes[ULTRA_SMP];

static int digit_start;
static int digit_end;

#define DIGIT0_START digit_start
#define DIGIT0_SIZE digit_end

#define DIGIT0_ONE	(DIGIT_SIZE/ULTRA_SMP)
#else
#define DIGIT0_START 1
#define DIGIT0_SIZE DIGIT_SIZE

#endif

static unsigned char pass[MAX_PASS_LEN];
static unsigned char realpass[MAX_PASS_LEN+1];
static size_t pass_pos = 0;

#if defined(HASH_SHA1)		/* (HAS_SHA1) */
#warning "You'd chosen SHA-1 cracking"
#include "gg-keygen-sha1.h"

#elif defined(HASH)		/* !(HAS_SHA1) && HASH */
#warning "You'd chosen old HASH cracking"
#include "gg-keygen-gg32.h"

#elif defined(TEXT)		/* !(HAS_SHA1) && !(HASH) && TEXT */
#warning "You didn't defined HASH_SHA1 nor HASH, generator test?"
#include "gg-keygen-text.h"

#else				/* !(HASH_SHA1) && !(HASH) && !(TEXT) */
#warning "You didn't defined HASH_SHA1 nor HASH nor TEXT, generator test?"

#endif

static inline void bonce(size_t i) {
	for (; i < pass_pos + 1; i++) {
		pass[i] = 1;	realpass[i] = digit[1];
	}
}

static inline void incr() {
	int i;

	for (i = pass_pos; i > 0; i--) {
		if (pass[i] < DIGIT_SIZE) {
			pass[i]++;	realpass[i] = digit[pass[i]];

			bonce(i+1);
			return;
		}
	}

	/* outside loop */
	if (pass[0] < DIGIT0_SIZE) {
#if ULTRA_VERBOSE
		/* jesli ktos bardzo lubi kropki, lepiej jest nie lubiec. */
		putchar('.');
		fflush(stdout);
#endif
		pass[0]++;	realpass[0] = digit[pass[0]];
		bonce(0+1);
		return;
	}

#if ULTRA_SAFE 
	/* po co to komu? */
	if (pass_pos == MAX_PASS_LEN) {
		fprintf(stderr, "%s:%d pass_pos == MAX_PASS_LEN, incr MAX_PASS_LEN?\n", __FILE__, __LINE__);
		exit(1);
	}
#endif
	pass_pos++;
	printf("Len: %d\n", pass_pos+1);

	pass[0] = DIGIT0_START;	realpass[0] = digit[DIGIT0_START];
	bonce(1);
}

int main() {
#if ULTRA_SMP
	int z;
#endif

#ifdef HASH_SHA1
	unsigned char digest[20];
	uint32_t digstate[5];
	int i;

/* HASH w SHA1 najpierw z 40 znakowego, ascii-printable znakow od [0-f] zamieniamy na binarna 20 znakow tablice.. */

	for (i = 0; i < 40; i++) {
		uint8_t znak;

		if (HASH_SHA1[i] == '\0') { fprintf(stderr, "BAD SHA1 hash: %s\n", HASH_SHA1);	return 1; }

		if (tolower(HASH_SHA1[i]) >= 'a' && tolower(HASH_SHA1[i]) <= 'f')
			znak = 10 + tolower(HASH_SHA1[i]-'a');
		else if (HASH_SHA1[i] >= '0' && HASH_SHA1[i] <= '9')
			znak = HASH_SHA1[i]-'0';
		else { fprintf(stderr, "BAD SHA1 char!\n"); return 1; }

		if (i % 2)
			digest[i / 2] |= znak;
		else	digest[i / 2] = znak << 4;

	}
	if (HASH_SHA1[40] != '\0') { fprintf(stderr, "BAD SHA1 hash: %s\n", HASH_SHA1);  return 1; }

	printf("%s == ", HASH_SHA1);
	for (i = 0; i < 20; i++) 
		printf("%.2x", digest[i]);

	printf("\n");

/* a teraz zamienmy na tablice 32bitowych liczb.. */
	digstate[0] = digest[ 0] << 24 | digest[ 1] << 16 | digest[ 2] << 8 | digest[ 3];
	digstate[1] = digest[ 4] << 24 | digest[ 5] << 16 | digest[ 6] << 8 | digest[ 7];
	digstate[2] = digest[ 8] << 24 | digest[ 9] << 16 | digest[10] << 8 | digest[11];
	digstate[3] = digest[12] << 24 | digest[13] << 16 | digest[14] << 8 | digest[15];
	digstate[4] = digest[16] << 24 | digest[17] << 16 | digest[18] << 8 | digest[19];

/* substract SHA1 initial vectors, to speedup computing @ loop... 4 add instruction less per loop */
	digstate[0] -= SHA_STATE0;
	digstate[1] -= SHA_STATE1;
	digstate[2] -= SHA_STATE2;
	digstate[3] -= SHA_STATE3;
	digstate[4] -= SHA_STATE4;
#endif

#if ULTRA_SMP
	printf("SMP with: %d processors, DIGIT_SIZE: %d\n", ULTRA_SMP, DIGIT_SIZE);
	digit_start = 1;

	for (z = 0; z < ULTRA_SMP; z++) {
		digit_end = digit_start + DIGIT0_ONE;

		if (digit_end > DIGIT_SIZE)
			digit_end = DIGIT_SIZE;

		if (digit_start < DIGIT_SIZE) {
			int fd[2] = { -1, -1 };
			pid_t tmp = -1;

			if (pipe(fd) != -1) {
				if (!(tmp = fork())) {	/* child */
					dup2(fd[1], 1);
					dup2(fd[1], 2);
					close(fd[0]);
					close(fd[1]);
					break;
				}
			}

			if (fd[1] != -1)
				close(fd[1]);

			pids[z] = tmp;
			pipes[z] = fd[0];

			if (tmp == -1) {
				fprintf(stderr, "Process%d not started! (not enough mana)\n", z);
				/* XXX */
				if (fd[0] != -1) { close(fd[0]); }
				if (fd[1] != -1) { close(fd[1]); }
			} else
				printf("Process%d calculating '%c'..'%c' (%d chars) pid: %d\n", 
					z, digit[digit_start], digit[digit_end], 1+(digit_end-digit_start), tmp);

			digit_start = digit_end+1;
		} else {
			pids[z] = -1;
			fprintf(stderr, "Process%d not started! (not enough data)\n", z);
		}
	}

	if (z == ULTRA_SMP) {	/* parent of all */
		int threads = ULTRA_SMP;

		printf("Calculating...\n");
		do {
			int status;
			pid_t pid;

			struct timeval stv;
			int maxfd;
			fd_set rd;

			stv.tv_sec = 10;
			stv.tv_usec = 0;

			maxfd = 0;

			/* XXX, zoptymalizowac */
			FD_ZERO(&rd);
			for (z = 0; z < ULTRA_SMP; z++) {
				if (pipes[z] != -1) {
					FD_SET(pipes[z], &rd);

					if (maxfd < pipes[z])
						maxfd = pipes[z];
				}
			}

			status = select(maxfd + 1, &rd, NULL, NULL, &stv);
			if (status == -1 && errno != EINTR) {
				printf("select() == -1, run!\n");
				break;
			}

			if (status > 0) {
				for (z = 0; z < ULTRA_SMP; z++) {
					if (pipes[z] != -1) {
						if (FD_ISSET(pipes[z], &rd)) {
							char buf[1024];
							int ret;

							ret = read(pipes[z], buf, sizeof(buf)-1);
							if (ret > 0) {
								buf[ret] = '\0';
								printf("[ MESSAGE FROM PROCESS #%d]\n", z);
								printf("%s", buf);
								printf("=======================\n");
							}
						}
					}
				}
			}

			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				for (z = 0; z < ULTRA_SMP; z++) {
					if (pids[z] == pid) {
						pids[z] = -1;
						close(pipes[z]);
						pipes[z] = -1;
						break;
					}
				}
				/* XXX */

				printf("died %d with %d\n", pid, status);

				threads--;
			}
			if (!NOT_STOP_ON_FIRST && (threads != ULTRA_SMP))
				break;

		} while (threads);

		for (z = 0; z < ULTRA_SMP; z++) {
			if (pids[z] != -1) {
				kill(pids[z], SIGKILL);
				close(pipes[z]);
				/* XXX */
			}
		}
		printf("done!\n");
		exit(0);
	}

	printf("Starting from: %c to %c\n", digit[DIGIT0_START], digit[DIGIT0_SIZE]);
	pass[0] = DIGIT0_START-1; realpass[0] = digit[DIGIT0_START-1];

#else
	printf("Without SMP\n");
#endif

	do {
		do {
			incr();
		} while 
#if defined(HASH_SHA1)
			(gg_login_sha1hash(pass, pass_pos+1, SEED, digstate));
#elif defined(HASH)
			(gg_login_hash() != HASH);
#elif defined(TEXT)
			(check_text());
#else
			(1);
#endif
		printf("Collision found: %s\n", realpass);
	} while(NOT_STOP_ON_FIRST);
	return 0;
}

