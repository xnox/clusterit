/* $Id: jcommon.c,v 1.1 2000/01/17 04:02:28 garbled Exp $ */
/*
 * Copyright (c) 1998, 1999, 2000
 *	Tim Rightnour.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tim Rightnour.
 * 4. The name of Tim Rightnour may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TIM RIGHTNOUR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TIM RIGHTNOUR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * these are routines that are used in all the jsd programs, and therefore
 * rather than having to fix them in n places, they are kept here.
 */

#include "jcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id: jcommon.c,v 1.1 2000/01/17 04:02:28 garbled Exp $");
#endif

void
shm_take_deeplock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 1;
	sops.sem_op = -1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_take_mainlock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 0;
	sops.sem_op = -1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_wait_deeplock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 1;
	sops.sem_op = 0;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_wait_mainlock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 0;
	sops.sem_op = 0;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_leave_deeplock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 1;
	sops.sem_op = 1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_leave_mainlock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 0;
	sops.sem_op = 1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}


void
shm_take_freelock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 2;
	sops.sem_op = -1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_take_seclock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 3;
	sops.sem_op = -1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_wait_freelock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 2;
	sops.sem_op = 0;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_wait_seclock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 3;
	sops.sem_op = 0;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_leave_freelock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 2;
	sops.sem_op = 1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}

void
shm_leave_seclock(semid)
	int semid;
{
	struct sembuf sops;

	sops.sem_flg = NULL;
	sops.sem_num = 3;
	sops.sem_op = 1;
	if (semop(semid, &sops, 1) == -1)
		log_bailout(__LINE__);
}
