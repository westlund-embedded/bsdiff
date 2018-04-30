/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright      2018 Johan Westlund
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "tinf.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define RAM_SIZE	256	// Actual RAM usage is RAM size x 4 (oldfile, newfile, patch)

static off_t offtin(uint8_t *buf) {
	off_t y;

	y = buf[7] & 0x7F;
	y = y * 256;
	y += buf[6];
	y = y * 256;
	y += buf[5];
	y = y * 256;
	y += buf[4];
	y = y * 256;
	y += buf[3];
	y = y * 256;
	y += buf[2];
	y = y * 256;
	y += buf[1];
	y = y * 256;
	y += buf[0];

	if (buf[7] & 0x80)
		y = -y;

	return y;
}

/* Reads compressed data until decompressed length */
size_t uzRead(TINF_DATA *d, int fd, uint8_t *buffer, size_t length) {
	int i, ret = TINF_OK, rd_length = 0, src_length = 0, dst_length = 0;
	
	uint8_t *tmp = (uint8_t*)malloc(RAM_SIZE);

	if (d->curlen)
	{
		if ((rd_length = read(fd, &tmp[1], RAM_SIZE - 1)) < 0)
			err(1, "reading src");
		tmp[0] = d->source[0];	
	}
	else
	{
		if ((rd_length = read(fd, tmp, RAM_SIZE)) < 0)
			err(1, "reading src");
	}

	printf("culen=%i, lzoff=%i\n", d->curlen, d->lzOff);

	d->source = tmp;
	d->dest = buffer;
	d->destSize = length;

	ret = uzlib_uncompress(d);

	if (ret != TINF_OK)
		err(1, "Decompression error\n");

	src_length = d->source - tmp;
	dst_length = d->dest - buffer;
	free(tmp);

	/* adjust file pointer */
	int off = src_length - rd_length;
	int roff = lseek(fd, off, SEEK_CUR);
		
	return dst_length;			
}

/* Opens and validates compressed data */
int uzReadOpen(TINF_DATA *d, int fd) {
	uint8_t header[10];
	
	/* Read header = 10 because FLG = 0 */
	if (read(fd, header, 10) != 10)
		return -1;

	d->source = header;

	uzlib_uncompress_init(d, NULL, 0);
	return uzlib_gzip_parse_header(d);
}

int main(int argc, char * argv[]) {
		
	int fd_old, fd_new, fd_patch;
	int uzfctrl, uzfdata, uzfextra;
	TINF_DATA tctrl, tdata, textra;

	ssize_t oldsize, newsize;
	ssize_t uzctrllen, uzdatalen;
	uint8_t header[36], *buf;
	off_t oldpos, newpos;
	off_t ctrl[3];
	off_t lenread;
	off_t i;

	uint8_t old[RAM_SIZE];
	uint8_t new[RAM_SIZE];
		
	buf = (uint8_t*)malloc(24);

	uzlib_init();

	if (argc != 4)
		errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

	/* Open patch file */
	if ((fd_patch = open(argv[3], O_RDONLY)) < 0)
		err(1, "%s", argv[3]);

	/*
	 File format:
	 0		12	"JWE/BSDIFF40"
	 12		8	X	sizeof control block
	 20		8	Y	sizeof diff block
	 28		8		sizeof newfile
	 36		X	uzlib(control block)
	 36+X	Y	uzlib(diff block)
	 36+X+Y	?	uzlib(extra block)
	 with control block a set of triples (x,y,z) meaning "add x bytes
	 from oldfile to x bytes from the diff block; copy y bytes from the
	 extra block; seek forwards in oldfile by z bytes".
	 */

	/* Read bsdiff header */
	if (read(fd_patch, header, 36) < 36) 
		err(1, "read(%s)", argv[3]);
	
	/* Check for appropriate magic */
	if (memcmp(header, "JWE/BSDIFF40", 12) != 0)
		errx(1, "Corrupt patch\n");

	/* Read lengths from header */
	uzctrllen = offtin(header + 12);
	uzdatalen = offtin(header + 20);
	newsize = offtin(header + 28);

	if ((uzctrllen < 0) || (uzdatalen < 0) || (newsize < 0))
		errx(1, "Corrupt patch\n");

	/* Close patch file and re-open it with uzlib at the right places */
	if (close(fd_patch))
		err(1, "close(%s)", argv[3]);

	if (((uzfctrl = open(argv[3], O_RDONLY)) < 0)
			|| (lseek(uzfctrl, 36, SEEK_SET) != 36)
			|| (uzReadOpen(&tctrl, uzfctrl) != TINF_OK))
		err(1, "%s", argv[3]);

	if (((uzfdata = open(argv[3], O_RDONLY)) < 0) 
			|| (lseek(uzfdata, 36 + uzctrllen, SEEK_SET) != 36 + uzctrllen)
			|| (uzReadOpen(&tdata, uzfdata) != TINF_OK))
		err(1, "%s", argv[3]);

	if (((uzfextra = open(argv[3], O_RDONLY)) < 0) 
			|| (lseek(uzfextra, 36 + uzctrllen + uzdatalen, SEEK_SET) != 36 + uzctrllen + uzdatalen)
			|| (uzReadOpen(&textra, uzfextra) != TINF_OK))
		err(1, "%s", argv[3]);

	if (((fd_old = open(argv[1], O_RDONLY)) < 0)
			|| ((oldsize = lseek(fd_old, 0, SEEK_END)) == -1)
			|| (lseek(fd_old, 0, SEEK_SET) != 0))
		err(1, "%s", argv[1]);

	if ((fd_new = open(argv[2], O_CREAT | O_RDWR, 0666)) < 0)
		err(1, "%s", argv[2]);

	int64_t max_length = 0;
	oldpos = 0;
	newpos = 0;
	
	while (newpos < newsize) {
		
		/* Read control data */
		if (uzRead(&tctrl, uzfctrl, buf, 24) != 24)
			errx(1, "Corrupt patch: 1\n");
		for (i = 0; i < 3; i++)	{
			ctrl[i] = offtin(&buf[i<<3]);
		}

		/* Sanity-check */
		if (newpos + ctrl[0] > newsize)
			errx(1, "Corrupt patch: 2\n");

		while (ctrl[0]) {
			max_length = MIN(ctrl[0], RAM_SIZE);

			/* Read old data */
			read(fd_old, old, max_length);

			/* Read diff string */
			lenread = uzRead(&tdata, uzfdata, new, max_length);
			
			if (lenread != max_length)
				errx(1, "Corrupt patch: 3\n");

			/* Add old data to diff string */
			for (i = 0; i < max_length; i++)
				if ((oldpos + i >= 0) && (oldpos + i < oldsize))
					new[i] += old[i];

			/* Adjust pointers */
			newpos += max_length;
			oldpos += max_length;

			/* Adjust ctrl */
			ctrl[0] -= max_length;

			/* Write to new */
			write(fd_new, new, max_length);
		}

		/* Sanity-check */
		if (newpos + ctrl[1] > newsize)
			errx(1, "Corrupt patch: 4\n");

		while (ctrl[1]) {

			max_length = MIN(ctrl[1], RAM_SIZE);

			/* Read extra string */
			lenread = uzRead(&textra, uzfextra, new, max_length);
			if (lenread < max_length)
				errx(1, "Corrupt patch: 5\n");

			/* Adjust pointers */
			newpos += max_length;

			/* Adjust ctrl */
			ctrl[1] -= max_length;

			/* Write to new */
			write(fd_new, new, max_length);
		}
				
		oldpos += ctrl[2];

		/* Adjust file pointer */
		int off = lseek(fd_old, ctrl[2], SEEK_CUR);
		
	};

	close(fd_new);
	close(fd_old);

	if (close(uzfctrl) || close(uzfdata) || close(uzfextra))
		err(1, "close(%s)", argv[3]);

	return 0;
}
