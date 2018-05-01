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

#include <sys/types.h>

#include "tinf.h"
#include "defl_static.h"
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(off_t *I,off_t *V,off_t start,off_t len,off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(off_t *I,off_t *V,uint8_t *old,off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static off_t matchlen(uint8_t *old,off_t oldsize,uint8_t *new,off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;

	return i;
}

static off_t search(off_t *I,uint8_t *old,off_t oldsize,
		uint8_t *new,off_t newsize,off_t st,off_t en,off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};


	/* Dummy return to make eclipse happy */
	return 0;
}

static void offtout(off_t x,uint8_t *buf)
{
	off_t y;

	if(x<0) y=-x; else y=x;

		buf[0]=y%256;y-=buf[0];
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;

	if(x<0) buf[7]|=0x80;
}

static void uzWriteOpen(FILE *sf, FILE *df)
{
	/* Write uzlib header */
	putc(0x1f, df);
    putc(0x8b, df);
    putc(0x08, df);
    putc(0x00, df); // FLG
    int mtime = 0;
    fwrite(&mtime, sizeof(mtime), 1, df);
    putc(0x04, df); // XFL
    putc(0x03, df); // OS

	fseeko(sf, 0, SEEK_SET);
}

static void uzWriteClose(FILE *sf, FILE *df)
{
	// TODO: add checksum
	// int sflen = ftello(sf);
	// char *source = (char*)malloc(sflen);
	// fseeko(sf, 0, SEEK_SET);
	// fread(source, 1, sflen, sf);
    // uint32_t crc = ~uzlib_crc32(source, sflen, ~0);
    // fwrite(&crc, sizeof(crc), 1, df);
	// free(source);
}

static size_t uzWrite(FILE *sf, FILE *df, uint8_t *buffer, size_t length)
{
	/* Store decompressed data for later, needed by crc32 to create ckecksum */
	fwrite(buffer, 1, length, sf);

	/* Compress data and write to the destination file */
	struct Outbuf out = {0};
	zlib_start_block(&out);
	uzlib_compress(&out, buffer, length);
    zlib_finish_block(&out);
	fwrite(out.outbuf, 1, out.outlen, df);
		
	return out.outlen;
}

int main(int argc,char *argv[])
{
	int fd;
	uint8_t *old,*new;
	off_t oldsize,newsize;
	off_t *I,*V;
	off_t scan,pos,len;
	off_t lastscan,lastpos,lastoffset;
	off_t oldscore,scsc;
	off_t s,Sf,lenf,Sb,lenb;
	off_t overlap,Ss,lens;
	off_t i;
	off_t cblen,dblen,eblen;
	uint8_t *cb,*db,*eb;
	uint8_t header[36];
	FILE *sf, *df;
		
	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);

	if(((I=malloc((oldsize+1)*sizeof(off_t)))==NULL) ||
		((V=malloc((oldsize+1)*sizeof(off_t)))==NULL)) err(1,NULL);

	qsufsort(I,V,old,oldsize);

	free(V);

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[2],O_RDONLY,0))<0) ||
		((newsize=lseek(fd,0,SEEK_END))==-1) ||
		((new=malloc(newsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,new,newsize)!=newsize) ||
		(close(fd)==-1)) err(1,"%s",argv[2]);

	if(((cb=malloc(newsize+1))==NULL) ||
		((db=malloc(newsize+1))==NULL) ||
		((eb=malloc(newsize+1))==NULL))
			err(1,NULL);

	cblen=0;
	dblen=0;
	eblen=0;
	
	/* Temporary file containing uncompressed data, 
		used by uzlib for creating crc32 checksum */
	if ((sf = fopen("crc32", "wb+")) == NULL)
		err(1, "%s", "crc32");

	/* Create the patch file (destination file) */
	if ((df = fopen(argv[3], "wb")) == NULL)
		err(1, "%s", argv[3]);

	/* Header is
		0	12	 "JWE/BSDIFF40"
		12	8	length of uzipped ctrl block
		20	8	length of uzipped diff block
		28	8	length of new file */
	/* File is
		0	36	Header
		36	??	uzlib ctrl block
		??	??	uzlib diff block
		??	??	uzlib extra block */

	memcpy(header,"JWE/BSDIFF40",12);
	offtout(0, header + 12);
	offtout(0, header + 20);
	offtout(newsize, header + 28);
	if (fwrite(header, 36, 1, df) != 1)
		err(1, "fwrite(%s)", argv[3]);

	/* Compute the differences, save ctrl as we go */
	
	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newsize;scan++) {
			len=search(I,old,oldsize,new+scan,newsize-scan,
					0,oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldsize) &&
				(old[scsc+lastoffset] == new[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldsize) &&
				(old[scan+lastoffset] == new[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldsize);) {
				if(old[lastpos+i]==new[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(old[pos-i]==new[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(new[lastscan+lenf-overlap+i]==
					   old[lastpos+lenf-overlap+i]) s++;
					if(new[scan-lenb+i]==
					   old[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				db[dblen+i]=new[lastscan+i]-old[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				eb[eblen+i]=new[lastscan+lenf+i];

			dblen+=lenf;
			eblen+=(scan-lenb)-(lastscan+lenf);

			offtout(lenf,&cb[cblen+0]);
			offtout((scan-lenb)-(lastscan+lenf),&cb[cblen+8]);
			offtout((pos-lenb)-(lastpos+lenf),&cb[cblen+16]);

			cblen+=24;

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};

	/* Write compressed ctrl data */
	uzWriteOpen(sf, df);
	uzWrite(sf, df, cb, cblen);
	uzWriteClose(sf, df);

	/* Compute size of compressed ctrl data */
	if ((len = ftello(df)) == -1)
		err(1, "ftello");
	offtout(len-36, header + 12);
	
	/* Write compressed diff data */
	uzWriteOpen(sf, df);
	uzWrite(sf, df, db, dblen);
	uzWriteClose(sf, df);

	/* Compute size of compressed diff data */
	if ((newsize = ftello(df)) == -1)
		err(1, "ftello");
	offtout(newsize - len, header + 20);

	/* Write compressed extra data */
	uzWriteOpen(sf, df);
	uzWrite(sf, df, eb, eblen);
	uzWriteClose(sf, df);
	
	/* Seek to the beginning, write the header, and close the file */
	if (fseeko(df, 0, SEEK_SET))
		err(1, "fseeko");
	if (fwrite(header, 36, 1, df) != 1)
		err(1, "fwrite(%s)", argv[3]);
	if (fclose(df))
		err(1, "fclose");
	if (fclose(sf))
		err(1, "fclose");

	if (remove("crc32"))
		err(1, "remove");

	/* Free the memory we used */
	free(cb);
	free(db);
	free(eb);
	free(I);
	free(old);
	free(new);

	return 0;
}
