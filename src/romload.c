/*Arculator 2.0 by Sarah Walker
  'Flexible' ROM loader*/
#ifdef WIN32
#include <io.h>
#else
#include <dirent.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "arc.h"
#include "config.h"

char romfns[17][256];
int firstromload=1;
char olddir[512];

int loadertictac()
{
        int c,d;
        char s[10];
        FILE *f[4];
        int addr=0;
        uint8_t *romb = (uint8_t *)rom;
        for (c=0;c<16;c+=4)
        {
                for (d=0;d<4;d++)
                {
                        sprintf(s,"%02i",(c|d)+1);
//                        rpclog("Opening %s\n",s);
                        f[d]=fopen(s,"rb");
                        if (!f[d])
                        {
//                                rpclog("File missing!\n");
                                return -1;
                        }
                }
                for (d=0;d<0x40000;d+=4)
                {
                        romb[d+addr]=getc(f[0]);
                        romb[d+addr+1]=getc(f[1]);
                        romb[d+addr+2]=getc(f[2]);
                        romb[d+addr+3]=getc(f[3]);
                }
                for (d=0;d<4;d++) fclose(f[d]);
                addr+=0x40000;
        }
        chdir(olddir);
        return 0;
}

int loadpoizone()
{
        int c,d;
        char s[10];
        FILE *f[4];
        int addr=0;
        uint8_t *romb = (uint8_t *)rom;
        return -1;
        for (c=0;c<24;c+=4)
        {
                if (c==12 || c==16)
                {
                        addr+=0x40000;
                        continue;
                }
                for (d=0;d<4;d++)
                {
                        sprintf(s,"p_son%02i.bin",(c|d)+1);
//                        rpclog("Opening %s\n",s);
                        f[d]=fopen(s,"rb");
                        if (!f[d])
                        {
//                                rpclog("File missing!\n");
                                return -1;
                        }
                }
                for (d=0;d<0x40000;d+=4)
                {
                        romb[d+addr]=getc(f[0]);
                        romb[d+addr+1]=getc(f[1]);
                        romb[d+addr+2]=getc(f[2]);
                        romb[d+addr+3]=getc(f[3]);
                }
                for (d=0;d<4;d++) fclose(f[d]);
                addr+=0x40000;
        }
        chdir(olddir);
        return 0;
}

int ucase(char c)
{
        if (c>='a' && c<='z') c-=32;
        return c;
}

int loadrom()
{
        FILE *f;
        int finished=0;
        int file=0;
        int c,d,e;
        int len,pos=0;
        int find_file;
//        char s[256];
        char fn[512];
        char s[512];
        char *ext;
#ifdef WIN32
        struct _finddata_t finddata;
#else
        DIR *dirp;
        struct dirent *dp;
#endif
        uint8_t *romb = (uint8_t *)rom;
//        rpclog("Loading ROM set %i\n",romset);
        if (firstromload)
        {
                getcwd(olddir,511);
                firstromload=0;
        }
        else
        {
                chdir(olddir);
        }
        snprintf(s, sizeof(s), "roms/%s", config_get_romset_name(romset));
        append_filename(fn, exname, s, sizeof(fn));

        rpclog("Loading ROM set %d from %s\n",romset, fn);
        if (chdir(fn) != 0)
        {
                perror(fn);
                return -1;
        }

#ifdef WIN32
        find_file = _findfirst("*.*", &finddata);
        if (find_file == -1)
        {
                chdir(olddir);
//                rpclog("No files found!\n");
                return -1;
        }
        while (!finished && file<16)
        {
                ext = (char *)get_extension(finddata.name);
                if (stricmp(ext,"txt") && strcmp(finddata.name, ".") && strcmp(finddata.name, ".."))
                {
//                        rpclog("Found %s\n",ff.name);
                        strcpy(romfns[file],finddata.name);
                        file++;
                }
//                else
//                   rpclog("Skipping %s\n",ff.name);
                finished = _findnext(find_file, &finddata);
        }
        _findclose(find_file);                        
#else
        dirp = opendir(".");
        if (!dirp)
        {
                perror("opendir: ");
#ifndef RELEASE_BUILD
                fatal("Can't open rom dir %s\n", fn);
#endif
        }
        else
        {
                while (((dp = readdir(dirp)) != NULL) && file<16)
                {
                        if (dp->d_type != DT_REG && dp->d_type != DT_LNK)
                                continue;
                        if (dp->d_name[0] != '.')
                        {
                                ext=get_extension(dp->d_name);
                                if (strcasecmp(ext,"txt"))
                                {
                                        rpclog("Found %s\n", dp->d_name);
                                        strcpy(romfns[file], dp->d_name);
                                        file++;
                                }
                        }
//                        else
//                                rpclog("Skipping %s\n",ff.name);
                }
                (void)closedir(dirp);
        }
#endif
        if (file==0)
        {
                chdir(olddir);
//                rpclog("No files found!\n");
                return -1;
        }
        for (c=0;c<file;c++)
        {
                for (d=0;d<file;d++)
                {
                        if (c>d)
                        {
                                e=0;
                                while (ucase(romfns[c][e])==ucase(romfns[d][e]) && romfns[c][e])
                                      e++;
                                if (ucase(romfns[c][e])<ucase(romfns[d][e]))
                                {
                                        memcpy(romfns[16],romfns[c],256);
                                        memcpy(romfns[c],romfns[d],256);
                                        memcpy(romfns[d],romfns[16],256);
                                }
                        }
                }
        }
        for (c=0;c<file;c++)
        {
                f=fopen(romfns[c],"rb");
                fseek(f,-1,SEEK_END);
                len=ftell(f)+1;
                fseek(f,0,SEEK_SET);
//                rpclog("Loading %s %08X %08X\n",romfns[c],len,pos);
                fread(&romb[pos],len,1,f);
                fclose(f);
                pos+=len;
        }
        chdir(olddir);
//        rpclog("Successfully loaded!\n");
        return 0;
}

int romset_available_mask = 0;

/*Establish which ROMs are available*/
int rom_establish_availability()
{
        int old_romset = romset;

        rom = malloc(4 * 1024 * 1024);

        for (romset = 0; romset < ROM_MAX; romset++)
        {
                if (!loadrom())
                        romset_available_mask |= (1 << romset);
        }

        free(rom);
        rom = NULL;
        
        romset = old_romset;

        return (!romset_available_mask);
}
