#ifndef UNPAK_H
#define	UNPAK_H

#include "unzip.h"

extern void *Z_Malloc (int size, bool crash);
extern void Z_Free (void *ptr);

#include <stdio.h>
extern FILE *FS_Fopen(const char *name, const char *mode);

typedef struct
{
	char			name[256];
	unsigned		attr;
	unsigned		offset;
	unsigned		size;
	unsigned		c_offset;
	unsigned		hash;	// hash of name
} fileinfo_t;

unsigned Com_HashKey (const char *string);
typedef struct
{
	char			pak_name[256];
	unzFile			uf;
	unz_global_info	gi;
	fileinfo_t		*fi;
	unsigned		hash;	// hash of pak_name
} zipfile_t;

int		PackFileOpen (zipfile_t *pf);
void	PackFileClose (zipfile_t *pf);
int		PackFileGet (zipfile_t *pf, char *fname, char **buf, unsigned hash);
int		PackFileGetFilesNumber (zipfile_t *pf);
///char	*PackFileGetFileName (zipfile_t *pf, int num);
bool	b_stricmp(char *str1, char *str2);
char	b_chrt(char sym);
int	PackFileSize (zipfile_t *pf, char *fname, unsigned hash);

#endif
