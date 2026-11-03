#ifndef MATH_H
#define MATH_H

/* tables.dat is 11904 bytes, read in six slices by 0x49518:
   4096, 4096, 640, 1024, 1024, 1024. */

#define TABLES_DAT_SIZE 11904

int loadtables(void);
const unsigned char *tables_dat(void);

/* 2048 signed values from the front of tables.dat. sin(90 deg) is 16383.
   NULL until loadtables succeeds. */
const short *sintable(void);

extern int sqrttable[2048];
void initmath(void);
int tablesqrt(int v);
int ksqrt(int v);
extern int randomseed;
int krand(void);
int wrand(void);
int getangle(int xvect, int yvect);

#endif
