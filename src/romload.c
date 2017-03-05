/*Arculator v0.8 by Tom Walker
  'Flexible' ROM loader*/
#include <allegro.h>
#include <stdio.h>
#include "arc.h"

int romset;
char romfns[17][256];
int firstromload=1;
char olddir[512];

int loadertictac()
{
        int c,d;
        char s[10];
        FILE *f[4];
        int addr=0;
        uint8_t *romb=rom;
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
        uint8_t *romb=rom;
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
        struct al_ffblk ff;
//        char s[256];
        char fn[512];
        char *ext;
        uint8_t *romb=rom;
//        rpclog("Loading ROM set %i\n",romset);
        if (firstromload) getcwd(olddir,511);
        firstromload=0;
//        append_filename(fn,exname,"roms\\",511);
        switch (romset)
        {
                case 0: append_filename(fn,exname,"roms/arthur",511); break;
                case 1: append_filename(fn,exname,"roms/riscos2",511); break;
                case 2: case 3: append_filename(fn,exname,"roms/riscos3",511); break;
                case 4: append_filename(fn,exname,"roms/ertictac",511); chdir(fn); return loadertictac();
                case 5: append_filename(fn,exname,"roms/poizone",511); chdir(fn); return loadpoizone();
                case 6: append_filename(fn,exname,"roms/wtiger",511); break;
        }
        chdir(fn);
        finished=al_findfirst("*.*",&ff,0xFFFF&~FA_DIREC);
        if (finished)
        {
                chdir(olddir);
//                rpclog("No files found!\n");
                return -1;
        }
        while (!finished && file<16)
        {
                ext=get_extension(ff.name);
                if (stricmp(ext,"txt"))
                {
//                        rpclog("Found %s\n",ff.name);
                        strcpy(romfns[file],ff.name);
                        file++;
                }
//                else
//                   rpclog("Skipping %s\n",ff.name);
                finished = al_findnext(&ff);
        }
        al_findclose(&ff);
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
