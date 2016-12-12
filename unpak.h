#ifndef UNPAK_H
#define	UNPAK_H

#include <stdbool.h>
#include <minizip/unzip.h>

#ifdef _WIN32
#define MAX_OSPATH			256		// max length of a filesystem pathname (same as MAX_PATH)
#else
#define MAX_OSPATH			4096	// max length of a filesystem pathname
#endif

void *Z_Malloc (int size, bool crash);
void Z_Free (void *ptr);
unsigned Com_HashKey (const char *string);

typedef struct
{
	char			name[MAX_OSPATH];
	uLong			attr;
	unz_file_pos	pos;
	uLong			size;
	unsigned		hash;	// hash of name
} fileinfo_t;

typedef struct
{
	char			pak_name[MAX_OSPATH];
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
int		PackFileSize (zipfile_t *pf, char *fname, unsigned hash);

#endif
