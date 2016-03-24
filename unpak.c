/*
	unpak.c			Copyright (C) 1998-2000 Vitaly Ovtchinnikov
										and Damir Sagidullin
*/

#include "unpak.h"

int		PackFileOpen (zipfile_t *pf)
{
	int		x, max;
	unz_file_info	file_info;

	pf->uf = unzOpen (pf->pak_name);
	if (!pf->uf)
		return 0;
	unzGetGlobalInfo (pf->uf, &pf->gi);
	pf->fi = (fileinfo_t *)Z_Malloc (sizeof(fileinfo_t)*pf->gi.number_entry, false);
	if (!pf->fi)
	{
		unzClose (pf->uf);
		return 0;
	}

	pf->hash = Com_HashKey(pf->pak_name);
	max = pf->gi.number_entry;

	for (x=0; x<max; x++)
	{
		unzGetCurrentFileInfo (pf->uf,&file_info, pf->fi[x].name,sizeof(pf->fi[x].name),NULL,0,NULL,0);
		pf->fi[x].attr = file_info.external_fa;
		pf->fi[x].offset = file_info.offset;
		pf->fi[x].size = file_info.uncompressed_size;
		pf->fi[x].c_offset = file_info.c_offset;
		pf->fi[x].hash = Com_HashKey(pf->fi[x].name);
		unzGoToNextFile (pf->uf);
	}
	return 1;
}

void	PackFileClose (zipfile_t *pf)
{
	if (pf->fi)
	{
		Z_Free (pf->fi);
		pf->fi = NULL;
	}
	if (pf->uf)
	{
		unzClose (pf->uf);
		pf->uf = NULL;
	}
}

/*
char b_chrt(char sym)
{
	if((sym>0x40)&&(sym<0x5b)) return sym+0x20;
	if(sym==0x2f) return 0x5c;
	return sym;
}


bool b_stricmp(char *str1, char *str2)
{
	int i=0;
	while (1)
	{
		char ch1=b_chrt(str1[i]);
		char ch2=b_chrt(str2[i]);
		if((ch1==0)&&(ch2==0)) return false;
		if(ch1!=ch2) return true;
		i++;
	}
}
*/

int		PackFileGet (zipfile_t *pf, char *fname, char **buf, unsigned hash)
{
	int			num;
	DWORD		err;
	for (num = 0; num<(int)pf->gi.number_entry; num++)
		if (hash == pf->fi[num].hash)
			if (!b_stricmp(fname, pf->fi[num].name))
				break;

	if (num >= (int)pf->gi.number_entry)
		return -1;

	*buf = (char *)Z_Malloc (pf->fi[num].size, true);

	unzLocateFileMy (pf->uf, num, pf->fi[num].c_offset);

	unzOpenCurrentFile (pf->uf);
	err = unzReadCurrentFile (pf->uf,*buf,pf->fi[num].size);
	unzCloseCurrentFile (pf->uf);

	if (err!=pf->fi[num].size)
	{
		Z_Free (*buf);
		*buf = NULL;
		return -2;
	}

	return pf->fi[num].size;
}

int		PackFileGetFilesNumber (zipfile_t *pf)
{
	return pf->gi.number_entry;
}
/*
char	*PackFileGetFileName (zipfile_t *pf, int num)
{
	if (num >= (int)pf->gi.number_entry)
		return NULL;
	return pf->fi[num].name;
}
*/
int	PackFileSize (zipfile_t *pf, char *fname, unsigned hash)
{
	int num;
	for (num = 0; num<(int)pf->gi.number_entry; num++)
		if (hash == pf->fi[num].hash)
			if (!b_stricmp(fname, pf->fi[num].name))
				return pf->fi[num].size;
	return -1;
}
