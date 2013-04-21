/*Arculator 0.8 by Tom Walker
  ArculFS - Access files from the windows file system
  Relies on ArcFS module
  Read only at the moment*/

#include <unistd.h>
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "arc.h"

#define CFLAG 0x20000000
#define VFLAG 0x10000000
#define AL_ID(a,b,c,d)     (((a)<<24) | ((b)<<16) | ((c)<<8) | (d))
FILE *arculf;
char arcfsdir[512];
FILE *arculfiles[64];
int arculfileid[64];
unsigned long loadaddrs[64],execaddrs[64];

int readflash[4];

char printoutput[4096]="ABCD";
int arculfsprinting;

void closearculfs()
{
        int c;
        for (c=0;c<64;c++)
        {
                if (arculfiles[c])
                {
                        fclose(arculfiles[c]);
                        arculfiles[c]=NULL;
                }
        }
}
        
void initarculfs()
{
        int c;
        sprintf(arcfsdir,"%sfs",exname);
        for (c=0;c<64;c++)
            arculfiles[c]=NULL;
        atexit(closearculfs);
        arculfsprinting=-1;
}

int lcase(int in)
{
        if (in<=64 || in>=91) return in;
        return in+32;
}

/*Tries to find a match for a filename in a path*/
int matchfilename(char *path, char *fn, char *dest)
{
        int c;
        char temps[512];
        struct al_ffblk ffblk;
        int success=0;
        sprintf(temps,"%s\\*",path);
        rpclog("Trying to match %s in %s\n",fn,temps);
//        fputs(temps2,arculf);
        if (al_findfirst(temps, &ffblk, -1))
        {
                rpclog("Failed\n");
                return 0;
        }
        do
        {
                c=0;
                while (lcase(fn[c])==lcase(ffblk.name[c]) && fn[c] && c<10) c++;
                rpclog("Testing %s broke at %i\n",ffblk.name,c);
//                fputs(temps2,arculf);
                if (((ffblk.name[c]==0 || ffblk.name[c]=='.' || ffblk.name[c]==',') && c>1 && !fn[c]) || c==10)
                {
                        success=1;
                        strcpy(dest,ffblk.name);
                }
        }
        while (!success && !al_findnext(&ffblk));
        al_findclose(&ffblk);
        if (success) rpclog("Found match in %s\n",dest);
        else         rpclog("Failed to match\n");
//        fputs(temps,arculf);
        return success;
}
        
/*Removes filename from s and puts it in s2*/
void stripfilename(char *s, char *s2)
{
        int c=0,lastsep=0,d=0;
        char s3[2]="\\";
//        sprintf(temps,"Stripping %s\n",s);
//        fputs(temps,arculf);
//        putc(s3[0],arculf);
        while (s[c])
        {
//                putc(s[c],arculf);
                if (s[c]==s3[0]) lastsep=c;
                c++;
        }
        if (!lastsep)
        {
                s2[0]=0;
                return;
        }
        c=lastsep+1;
        while (s[c])
              s2[d++]=s[c++];
        s[lastsep]=0;
        s2[d]=0;
//        sprintf(temps,"Stripped to %s and %s\n",s,s2);
//        fputs(temps,arculf);
}

unsigned long loadaddr,execaddr;
/*returns file type*/
int parsefilename(char *s, char *s2)
{
        int c=0,d=0,e,type;
        char t[4];
        while (s[c] && s[c]!='.' && s[c]!=',')
              s2[d++]=s[c++];
        s2[d]=0;
        if (!s[c]) /*Generic data file*/
        {
                sscanf(t,"%x",&type);
//                sprintf(temps,"File %s type FFF\n",s2);
//                fputs(temps,arculf);
                loadaddr=0xFFFFFD00;
                execaddr=0;
                return 0xFFD;
        }
        if (s[c]==',') /*hex value contains filetype*/
        {
                e=c;
                if (s[c+4]==0 || s[c+4]=='.') /*hex value is filetype*/
                {
                        isfiletype:
                        t[0]=s[c+1];
                        t[1]=s[c+2];
                        t[2]=s[c+3];
                        t[3]=0;
                        sscanf(t,"%x",&type);
//                        sprintf(temps,"File %s type %03X\n",s2,type);
//                        fputs(temps,arculf);
                        loadaddr=0xFFF00000|(type<<8);
                        execaddr=0;
                        return type;
                }
                /*hex value is load/exec addresses*/
                d=0;
                c++;
                while (s[c]!='-' && s[c])
                {
                        t[d]=s[c];
                        c++;
                        d++;
//                        sprintf(temps,"Next char %c\n",s[c]);
//                        fputs(temps,arculf);
                }
                if (!s[c])
                {
                        c=e;
                        goto isfiletype;
                }
                t[d]=0;
                sscanf(t,"%x",&loadaddr);
                d=0;
                c++;
                while (s[c]!='-' && s[c]!=0)
                {
                        t[d]=s[c];
                        c++;
                        d++;
//                        sprintf(temps,"Next2 char %c\n",s[c]);
//                        fputs(temps,arculf);
                }
                t[d]=0;
                sscanf(t,"%x",&execaddr);
//                sprintf(temps,"File %s load %08X exec %08X\n",s2,loadaddr,execaddr);
//                fputs(temps,arculf);
                return 0xffd;
        }
                loadaddr=0xFFFFFF00;
                execaddr=0;
        return 0xFFF;
}

/*assume absolute pathname*/
void parsepathname(char *s, char *s2)
{
        int c=0,e;
        char temp[128];
//        fputs("Parsing pathname : ",arculf);
//        fputs(s,arculf);
//        fputs("\n",arculf);
        sprintf(s2,"%s",arcfsdir);
        while (s[c] && s[c]!='$') c++;
        if (!s[c])
        {
                rpclog("Bad pathname %i %s %s\n",c,s,s2);
                exit(-1);
        }
        if (!s[c+1])
        {
//                fputs("Final pathname : ",arculf);
//                fputs(s2,arculf);
//                fputs("\n",arculf);
                return;
        }
        c+=2;
        while (1 && s[c])
        {
                strcat(s2,"\\");
                e=0;
                while (s[c] && s[c]!='.')
                {
                        temp[e++]=s[c++];
                }
                temp[e]=0;
                strcat(s2,temp);
                if (!s[c]) break;
                c++;
        }
//        fputs("Final pathname : ",arculf);
//        fputs(s2,arculf);
//        fputs("\n",arculf);
}

void renamefile(char *path, char *o, char *n)
{
        FILE *f,*ff;
        char s[512];
        unsigned char temp;
        if (!stricmp(o,n)) return;
        sprintf(s,"%s/%s",path,o);
        f=fopen(s,"rb");
        if (!f)
        {
                rpclog("Couldn't open %s for read!\n",s);
                exit(-1);
        }
        sprintf(s,"%s/%s",path,n);
        ff=fopen(s,"wb");
        if (!ff)
        {
                rpclog("Couldn't open %s for write!\n",s);
                fclose(f);
                exit(-1);
        }
        while (!feof(f))
        {
                temp=getc(f);
                if (feof(f)) break;
                putc(temp,ff);
        }
        fclose(ff);
        fclose(f);
        sprintf(s,"%s/%s",path,o);
        remove(s);
}

void arculfs(int call)
{
        int c,d,e,ff=0,start,filetype;
        FILE *f;
        unsigned char lastbyte;
        char s[512],s2[512],s3[512],s4[512];
        unsigned char *sp;
        char path[512];
        struct al_ffblk ffblk;
        path[0]=0;
//        if (!arculf) arculf=fopen("arculfs.txt","wt");
        rpclog("ARCULFS call %i %08X %08X %08X %08X %07X\n",call,armregs[0],armregs[1],armregs[2],armregs[3],PC);
//        fputs(s,arculf);
        switch (call)
        {
                case 0: /*FSEntry_Open*/
                switch (armregs[0])
                {
                        case 0: /*Open for read*/
                        rpclog("Open file : ");
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
                                c++;
                        }
                        s[d]=0;
                        rpclog("\n");
                        parsepathname(s,s2);
                        filetype=0xFFF;
                        if (!file_exists(s2,-1,&e))
                        {
                                stripfilename(s2,s);
                                if (!matchfilename(s2,s,path))
                                {
                                        error("File not found in FS_Entry_Open");
  //                                      fputs("File not found FSEntry_Open\n",arculf);
                                        exit(-1);
                                }
//                                fputs("Found path ",arculf);
                                sprintf(s,"%s\\%s",s2,path);
//                                fputs(s,arculf);
//                                fputs("\n",arculf);
                                if (!file_exists(s,-1,&e))
                                {
                                        error("File not found FSEntry_Open\n");
                                        exit(-1);
                                }
                                strcpy(s2,s);
                                filetype=parsefilename(path,s);
                        }
//                        fputs("Opening file : ",arculf);
//                        fputs(s2,arculf);
  //                      fputs("\n",arculf);
                        c=0;
                        while (arculfiles[c] && c<64) c++;
                        if (c==64)
                        {
                                error("ArculFS out of file handles\n");
                                exit(-1);
                        }
                        rpclog("Opened with file handle %i\n",c);
                        arculfileid[c]=armregs[3];
                        arculfiles[c]=fopen(s2,"rb");
                        if (!arculfiles[c])
                        {
                                error("ArculFS failed to open file\n");
//                                fputs("Failed to open file\n",arculf);
                                exit(-1);
                        }
                        armregs[1]=c+1;
                        armregs[2]=64;
                        armregs[4]=64;
                        armregs[0]=1<<30;
                        if (e&FA_DIREC)     armregs[0]|=(1<<29);
                        if (!(e&FA_RDONLY)) armregs[0]|=(1<<31);
                        fseek(arculfiles[c],-1,SEEK_END);
                        armregs[3]=ftell(arculfiles[c])+1;
                        fseek(arculfiles[c],0,SEEK_SET);
                        armregs[15]&=~VFLAG;
//                        sprintf(s,"File opened : %08X %08X %08X %08X %08X\n",armregs[0],armregs[1],armregs[2],armregs[3],armregs[4]);
//                        fputs(s,arculf);
                        loadaddrs[c]=loadaddr;
                        execaddrs[c]=execaddr;                        
                        break;
                                
                        case 2: /*Open for update*/
//                        fputs("Open update file : ",arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
//                        fputs("\n",arculf);
                        parsepathname(s,s2);
                        filetype=0xFFF;
                        if (!file_exists(s2,-1,&e))
                        {
                                stripfilename(s2,s);
                                if (!matchfilename(s2,s,path))
                                {
                                        error("File not found in FSEntry_Open\n");
//                                        fputs("File not found FSEntry_Open\n",arculf);
                                        exit(-1);
                                }
//                                fputs("Found path ",arculf);
                                sprintf(s,"%s\\%s",s2,path);
//                                fputs(s,arculf);
//                                fputs("\n",arculf);
                                if (!file_exists(s,-1,&e))
                                {
                                        error("File not found in FSEntry_Open\n");
                                        exit(-1);
                                }
                                strcpy(s2,s);
                                filetype=parsefilename(path,s);
                        }
//                        fputs("Opening file : ",arculf);
//                        fputs(s2,arculf);
//                        fputs("\n",arculf);
                        c=0;
                        while (arculfiles[c] && c<64) c++;
                        if (c==64)
                        {
                                error("ArculFS out of file handles\n");
//                                fputs("Out of file handles\n",arculf);
                                exit(-1);
                        }
                        arculfileid[c]=armregs[3];
                        arculfiles[c]=fopen(s2,"r+b");
                        if (!arculfiles[c])
                        {
                                error("ArculFS failed to open file\n");
//                                fputs("Failed to open file\n",arculf);
                                exit(-1);
                        }
                        armregs[1]=c+1;
                        armregs[2]=64;
                        armregs[4]=64;
                        armregs[0]=1<<30;
                        if (e&FA_DIREC)     armregs[0]|=(1<<29);
                        if (!(e&FA_RDONLY)) armregs[0]|=(1<<31);
                        fseek(arculfiles[c],-1,SEEK_END);
                        armregs[3]=ftell(arculfiles[c])+1;
                        fseek(arculfiles[c],0,SEEK_SET);
                        armregs[15]&=~VFLAG;
//                        sprintf(s,"File opened : %08X %08X %08X %08X %08X\n",armregs[0],armregs[1],armregs[2],armregs[3],armregs[4]);
//                        fputs(s,arculf);
                        loadaddrs[c]=loadaddr;
                        execaddrs[c]=execaddr;                        
                        break;
                                
                        default:
                        sprintf(s,"Bad FSEntry_Open call %i %08X %08X %08X %08X\n",call,armregs[0],armregs[1],armregs[2],armregs[3]);
                        MessageBox(NULL,s,s,MB_OK);
                        dumpregs();
                        exit(-1);
                }
                break;

                case 1: /*FSEntry_GetBytes*/
                readflash[0]=1;
                if (!arculfiles[armregs[1]-1])
                {
                        error("FSEntry_GetBytes bad file number\n");
//                        fputs("Bad file number\n",arculf);
                        exit(-1);
                }
                c=armregs[1]-1;
                fseek(arculfiles[c],armregs[4],SEEK_SET);
                rpclog("Get bytes from file %i len %i bytes to %08X\n",c,armregs[3],armregs[2]);
//                sprintf(s,"Seeked file %i to %08X\n",c,armregs[4]);
//                fputs(s,arculf);
                for (d=0;d<armregs[3];d++)
                {
                        lastbyte=getc(arculfiles[c]);
                        if (feof(arculfiles[c]))
                        {
                                armregs[3]-=d;
                                armregs[15]|=CFLAG;
                                return;
                        }
                        writememb(d+armregs[2],lastbyte);
//                        sprintf(s,"Written %02X to %08X %i\n",lastbyte,d+armregs[2],d);
//                        fputs(s,arculf);
                }
                armregs[15]&=~VFLAG;
                break;

                case 2: /*FSEntry_PutBytes*/
                readflash[0]=2;
                if (!arculfiles[armregs[1]-1])
                {
                        error("FSEntry_PutBytes bad file number\n");
//                        fputs("Bad file number\n",arculf);
                        exit(-1);
                }
                c=armregs[1]-1;
                fseek(arculfiles[c],armregs[4],SEEK_SET);
//                sprintf(s,"Seeked file %i to %08X\n",c,armregs[4]);
//                fputs(s,arculf);
                for (d=0;d<armregs[3];d++)
                {
                        lastbyte=readmemb(d+armregs[2]);
                        putc(lastbyte,arculfiles[c]);
//                        sprintf(s,"Written %02X from %08X %i\n",lastbyte,d+armregs[2],d);
//                        fputs(s,arculf);
                }
                armregs[15]&=~VFLAG;
                break;
                
                case 3: /*FSEntry_Args*/
                switch (armregs[0])
                {
                        case 1: /*Write sequential pointer*/
//                        fputs("Write sequential pointer\n",arculf);
                        if (!arculfiles[armregs[1]-1])
                        {
                                error("FSEntry_Args bad file number\n");
//                                fputs("Bad file number\n",arculf);
                                exit(-1);
                        }
                        c=armregs[1]-1;
                        fseek(arculfiles[c],armregs[2],SEEK_SET);
//                        sprintf(s,"Seeked to %i\n",armregs[2]);
//                        fputs(s,arculf);
                        armregs[15]&=~VFLAG;
                        break;

                        case 3: /*Write file extent*/
                        armregs[15]&=~VFLAG;
                        break;
                                                
                        case 5: /*EOF check*/
                        if (!arculfiles[armregs[1]-1])
                        {
                                error("FSEntry_Args bad file number\n");
//                                fputs("Bad file number\n",arculf);
                                exit(-1);
                        }
                        c=armregs[1]-1;
                        if (feof(arculfiles[c])) armregs[2]=-1;
                        else                     armregs[2]=0;
                        armregs[15]&=~VFLAG;
                        break;
                        
                        case 6: /*Flush buffers*/
                        armregs[2]=armregs[3]=0;
                        armregs[15]&=~VFLAG;
                        break;
                        
                        case 7: /*Ensure file size*/
                        if (!arculfiles[armregs[1]-1])
                        {
                                error("FSEntry_Args bad file number\n");
//                                fputs("Bad file number\n",arculf);
                                exit(-1);
                        }
                        
                        c=armregs[1]-1;
                        d=ftell(arculfiles[c]);
                        fseek(arculfiles[c],-1,SEEK_END);
                        armregs[2]=ftell(arculfiles[c]);
                        armregs[15]&=~VFLAG;
                        fseek(arculfiles[c],d,SEEK_SET);
                        break;

                        case 9: /*Read load/exec addresses*/                        
                        if (!arculfiles[armregs[1]-1])
                        {
                                error("FSEntry_Args bad file number\n");
//                                fputs("Bad file number\n",arculf);
                                exit(-1);
                        }
                        c=armregs[1]-1;
                        armregs[2]=loadaddrs[c];
                        armregs[3]=execaddrs[c];
                        armregs[15]&=~VFLAG;                        
                        break;
                        
                        default:
                        sprintf(s,"Bad FSEntry_Args call %i %08X %08X %08X %08X\n",call,armregs[0],armregs[1],armregs[2],armregs[3]);
                        MessageBox(NULL,s,s,MB_OK);
                        dumpregs();
                        exit(-1);
                }
                break;
                
                case 4: /*FSEntry_Close*/
                if (!arculfiles[armregs[1]-1])
                {
                        error("FSEntry_Close bad file number\n");
//                        fputs("Bad file number\n",arculf);
                        exit(-1);
                }
                c=armregs[1]-1;
                fclose(arculfiles[c]);
                arculfiles[c]=NULL;
//                sprintf(s,"Closed file %i\n",c);
//                fputs(s,arculf);
                armregs[15]&=~VFLAG;
                break;
                
                case 5: /*FSEntry_File*/
                switch (armregs[0])
                {
                        case 0: /*Save file*/
                        rpclog("Save file : ");
//                        fputs("Save file : ",arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
                        c=armregs[2];
//                        fputs("\n",arculf);
                        parsepathname(s,s2);
                        rpclog("\n%s\n%s\n",s,s2);
//                        fputs(s,arculf);
//                        fputs(s2,arculf);
                        if ((armregs[2]&0xFFF00000)==0xFFF00000)
                           sprintf(s3,"%s,%x",s2,(armregs[2]>>8)&0xfff);
                        else
                           sprintf(s3,"%s,%x-%x",s2,armregs[2],armregs[3]);
                        rpclog("%s\n",s3);
                        f=fopen(s3,"wb");
//                        sprintf(s,"Opened file %s for save\n",s3);
//                        fputs(s,arculf);
                        if (!f)
                        {
                                error("FSEntry_File file error\n");
//                                fputs("File error\n",arculf);
                                exit(-1);
                        }
                        rpclog("Writing\n");
                        for (d=armregs[4];d<armregs[5];d++)
                        {
                                lastbyte=readmemb(d);
                                putc(lastbyte,f);
                        }
                        fclose(f);
                        armregs[15]&=~VFLAG;
                        rpclog("Written\n");
//                        sprintf(s,"Save file %08X %08X %08X %08X %08X\n",armregs[2],armregs[3],armregs[4],armregs[5],armregs[6]);
//                        fputs(s,arculf);
                        return;
                        case 1: /*Write catalogue information*/
                        rpclog("Write file info : ");
//                        fputs("Save file : ",arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
                        c=armregs[2];
//                        fputs("\n",arculf);
                        parsepathname(s,s2);
                        rpclog("\n%s\n%s\n",s,s2);
                        //matchfilename
                        rpclog("Load %08X Exec %08X Attr %08X\n",armregs[2],armregs[3],armregs[5]);
                        stripfilename(s2,s);
                        strcpy(s4,s);
                        if (!matchfilename(s2,s,path))
                        {
                                error("File not found in FSEntry_Open\n");
                                exit(-1);
                        }
                        sp=get_filename(path);
                        sprintf(s3,"%s\\%s",s2,sp);
                        file_exists(s3,-1,&d);
                        rpclog("File %s %08X %08X\n",s3,d,FA_DIREC);
                        if (d&FA_DIREC) /*Don't change type of directory*/
                        {
                                armregs[15]&=~VFLAG;
                                return;
                        }
                        rpclog("Found existing file %s\\%s\n",s2,sp);
                        if ((armregs[2]&0xFFF00000)==0xFFF00000)
                           sprintf(s3,"%s,%x",s4,(armregs[2]>>8)&0xfff);
                        else
                           sprintf(s3,"%s,%x-%x",s4,armregs[2],armregs[3]);
                        rpclog("New filename %s\\%s\n",s2,s3);
                        renamefile(s2,sp,s3);
                        armregs[15]&=~VFLAG;
                        return;
                        case 5: /*Read catalogue information*/
//                        fputs("Read catalogue information : ",arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
//                        fputs("\n",arculf);
                        if (lastbyte=='$') /*Root directory*/
                        {
                                armregs[0]=2;
                        }
                        else
                        {
                                rpclog("Path %s\n",path);
                                parsepathname(s,s2);
                                filetype=0xFFF;
                                if (!file_exists(s2,-1,&e))
                                {
                                        stripfilename(s2,s);
                                        if (!matchfilename(s2,s,path))
                                        {
                                                armregs[0]=0;
                                                return;
                                        }
//                                        fputs("Found path ",arculf);
                                        sprintf(s,"%s\\%s",s2,path);
//                                        fputs(s,arculf);
//                                        fputs("\n",arculf);
                                        if (!file_exists(s,-1,&e))
                                        {
                                                armregs[0]=0;
                                                return;
                                        }
                                        filetype=parsefilename(path,s);
                                }
                                rpclog("Path %s\n",path);
                                if (path[0]) sprintf(s3,"%s/%s",s2,path);
                                else         sprintf(s3,"%s",s2);
//                                sprintf(s,"File type %03X\ne=%08X\n",filetype,e);
//                                fputs(s,arculf);
                                if (e&FA_DIREC)
                                {
//                                        fputs("Directory\n",arculf);
                                        armregs[0]=2;
                                        return;
                                }
//                                sprintf(s,"s2 %s\t path %s\n",s3,path);
//                                fputs(s,arculf);
                                armregs[0]=1;
                                armregs[2]=loadaddr;//0xFFF00000|(filetype<<8);
                                armregs[3]=execaddr;
                                armregs[4]=file_size(s3);
                                armregs[5]=0x11; /*Read access*/
                                if (!(e&FA_RDONLY))
                                   armregs[5]=0x33; /*R/W access*/
                                armregs[15]&=~VFLAG;
                                rpclog("Cat info - %s %s %s %i %08X %08X %08X\n",s,s2,s3,armregs[4],armregs[2],armregs[3],armregs[5]);
//                                sprintf(s,"R2 %08X R3 %08X R4 %08X\n",armregs[2],armregs[3],armregs[4]);
//                                fputs(s,arculf);
//                                return;
                        }
                        armregs[15]&=~VFLAG;
                        break;
                        case 7: /*Create file*/
                        rpclog("Create file : ");
                        for (c=0;c<6;c++)
                            rpclog("%08X ",armregs[c]);
                        rpclog("\n");
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
                                c++;
                        }
                        s[d]=0;
                        c=armregs[2];
                        rpclog("\n");
                        parsepathname(s,s2);
                        if ((armregs[2]&0xFFF00000)==0xFFF00000)
                           sprintf(s3,"%s,%x",s2,(armregs[2]>>8)&0xfff);
                        else
                           sprintf(s3,"%s,%x-%x",s2,armregs[2],armregs[3]);
                        f=fopen(s3,"wb");
                        if (!f)
                        {
                                rpclog("Couldn't create %s!\n",s3);
                        }
//                        sprintf(s,"Created file %s %08X %08X\n",s3,armregs[4],armregs[5]);
//                        fputs(s,arculf);
                        for (d=armregs[4];d<armregs[5];d++)
                            putc(0,f);
                        fclose(f);
                        break;
                        case 8: /*Create directory*/
                        rpclog("Createdirectory file : ");
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
                                c++;
                        }
                        s[d]=0;
                        c=armregs[2];
                        rpclog("\n",arculf);
                        parsepathname(s,s2);
                        rpclog("s %s\ns2 %s\n",s,s2);
                        if (file_exists(s2,-1,NULL))
                        {
                                rpclog("Directory already exists\n");
                                armregs[15]&=~VFLAG;
                                return;
                        }
                        if (mkdir(s2)) armregs[15]|=VFLAG;
                        else           armregs[15]&=~VFLAG;
                        return;
                        case 0xFF: /*Load file*/
                        rpclog("Load file : ");
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
                                rpclog("%c",lastbyte);
                                c++;
                        }
                        s[d]=0;
                        c=armregs[2];
                        rpclog("\n",arculf);
                        parsepathname(s,s2);
                        if (!file_exists(s2,-1,&e))
                        {
                                        stripfilename(s2,s);
                                        if (!matchfilename(s2,s,path))
                                        {
                                error("FSEntry_File file not found\n");
//                                fputs("File not found\n",arculf);
                                exit(-1);
                                                armregs[0]=0;
                                                return;
                                        }
//                                        fputs("Found path ",arculf);
                                        sprintf(s,"%s\\%s",s2,path);
//                                        fputs(s,arculf);
//                                        fputs("\n",arculf);
                                        if (!file_exists(s,-1,&e))
                                        {
                                error("FSEntry_File file not found\n");
//                                fputs("File not found\n",arculf);
                                exit(-1);
                                                armregs[0]=0;
                                                return;
                                        }
                                        f=fopen(s,"rb");
                                armregs[4]=file_size(s);
                                        filetype=parsefilename(path,s);
                        }
                        else
                        {
                                armregs[4]=file_size(s2);
                                f=fopen(s2,"rb");
                        }
                        armregs[2]=loadaddr;//0xFFFFFA00;
                        armregs[3]=execaddr;//0;
//                        armregs[4]=file_size(s2);
                        if (!(e&FA_RDONLY))
                           armregs[5]=0x33; /*R/W access*/
                        rpclog("Len %i loaded to %08X\n",armregs[4],c);
//                        if (c==0xC03C) output=1;
                        if (!f)
                        {
                                error("FSEntry_File file error\n");
//                                fputs("File error\n",arculf);
                                exit(-1);
                        }
                        for (d=0;d<armregs[4];d++)
                        {
                                lastbyte=getc(f);
                                writememb(d+c,lastbyte);
                        }
                        fclose(f);
                        armregs[15]&=~VFLAG;
//                        sprintf(s,"Load file %08X %08X %08X %08X %08X\n",armregs[2],armregs[3],armregs[4],armregs[5],armregs[6]);
//                        fputs(s,arculf);
                        return;
                        
                        default:
                        sprintf(s,"Bad FSEntry_File call %i %08X %08X %08X %08X\n",call,armregs[0],armregs[1],armregs[2],armregs[3]);
                        MessageBox(NULL,s,s,MB_OK);
                        dumpregs();
                        exit(-1);
                }
                break;
                case 6: /*FSEntry_Func*/
                switch (armregs[0])
                {
                        case 0: /*Set current directory*/
//                        rpclog("Set current directory %08X : ",armregs[1]);
                        c=0;//armregs[1];
                        while (readmemb(c+armregs[1]) && c<16)
                        {
//                                rpclog("%c",readmemb(c+armregs[1]));
//                                putc(readmemb(c),arculf);
                                c++;
                        }
//                        rpclog("\n");
//                        fputs("\n",arculf);
                        armregs[15]&=~VFLAG;
                        break;
/*                        case 2:
                        arculfsprinting=0;
                        armregs[15]|=VFLAG;
                        break;*/
                        case 0xB: /*Read name and boot (*OPT 4) option of disc*/
//                        fputs("Set name/opt\n",arculf);
                        c=armregs[2];
                        writememb(c,3);
                        writememb(c+1,'H');
                        writememb(c+2,'D');
                        writememb(c+3,'4');
                        writememb(c+4,0);
                        writememb(c+5,0);
                        armregs[15]&=~VFLAG;
                        break;

                        case 0xE: /*Read directory entries*/
//                        fputs("Read directory entries\n",arculf);
//                        sprintf(s,"Maximum entries : %i  Length of buffer : %i\n",armregs[3],armregs[5]);
//                        fputs(s,arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
                        parsepathname(s,s2);
                        sprintf(path,"%s\\*",s2);
//                        fputs(path,arculf);
//                        fputs("\n",arculf);
                        if (al_findfirst(path, &ffblk, -1) != 0) /*No files in directory*/
                        {
//                                fputs("No entries\n",arculf);
                                armregs[3]=0;
                                armregs[4]=0xFFFFFFFF;
                                return;
                        }
                        start=armregs[4];
                        for (c=0;c<start+2;c++)
                            al_findnext(&ffblk);
                        c=armregs[2];
                        armregs[3]=0;
                        do
                        {
                                if (ffblk.name[0]=='.') continue;
                                filetype=parsefilename(ffblk.name,s);
                                d=0;
                                while (s[d] && d<10)
                                      d++;
                                if ((ff+d)>=armregs[5])
                                {
                                        armregs[4]=start+armregs[3];
                                        return;
                                }
//                                fputs("Writing entry ",arculf);
//                                fputs(s,arculf);
//                                fputs("\n",arculf);
//                                writememl(c,loadaddr);//0xFFFFFA00);
//                                writememl(c+4,execaddr);//0);
//                                writememl(c+8,ffblk.size);
//                                writememl(c+12,0x33);
//                                if (ffblk.attrib&FA_DIREC) { writememl(c+16,2); }
//                                else                       { writememl(c+16,1); }
                                d=0;
                                while (s[d] && d<10)
                                {
                                        writememb(c+d,s[d]);
                                        d++;
                                }
                                writememb(c+d,0);
                                d++;
//                                d+=20;
/*                                while (d&3)
                                {
                                        writememb(c+d,0);
                                        d++;
                                }*/
                                c+=d;
                                ff+=d;
                                armregs[3]++;
                        } while (al_findnext(&ffblk) == 0);
                        al_findclose(&ffblk);
                        armregs[15]&=~VFLAG;
                        armregs[4]=0xFFFFFFFF;
                        break;

                        case 0xF: /*Read directory entries and information*/
//                        fputs("Read directory entries\n",arculf);
//                        sprintf(s,"Maximum entries : %i  Length of buffer : %i\n",armregs[3],armregs[5]);
//                        fputs(s,arculf);
                        c=armregs[1];
                        d=0;
                        while (readmemb(c))
                        {
                                lastbyte=readmemb(c);
                                s[d++]=lastbyte;
//                                putc(lastbyte,arculf);
                                c++;
                        }
                        s[d]=0;
                        parsepathname(s,s2);
                        sprintf(path,"%s\\*",s2);
//                        fputs(path,arculf);
//                        fputs("\n",arculf);
                        if (al_findfirst(path, &ffblk, -1) != 0) /*No files in directory*/
                        {
//                                fputs("No entries\n",arculf);
                                armregs[3]=0;
                                armregs[4]=0xFFFFFFFF;
                                return;
                        }
                        start=armregs[4];
                        for (c=0;c<start+2;c++)
                            al_findnext(&ffblk);
                        c=armregs[2];
                        armregs[3]=0;
                        do
                        {
                                if (ffblk.name[0]=='.') continue;
                                filetype=parsefilename(ffblk.name,s);
                                d=0;
                                while (s[d] && d<10)
                                      d++;
                                if ((ff+d)>=armregs[5])
                                {
                                        armregs[4]=start+armregs[3];
                                        return;
                                }
//                                fputs("Writing entry ",arculf);
//                                fputs(s,arculf);
//                                fputs("\n",arculf);
                                writememl(c,loadaddr);//0xFFFFFA00);
                                writememl(c+4,execaddr);//0);
                                writememl(c+8,ffblk.size);
                                writememl(c+12,0x33);
                                if (ffblk.attrib&FA_DIREC) { writememl(c+16,2); }
                                else                       { writememl(c+16,1); }
                                d=0;
                                while (s[d] && d<10)
                                {
                                        writememb(c+d+20,s[d]);
                                        d++;
                                }
                                writememb(c+d+20,0);
                                d++;
                                d+=20;
                                while (d&3)
                                {
                                        writememb(c+d,0);
                                        d++;
                                }
                                for (e=0;e<d;e+=4)
                                {
//                                        sprintf(s,"%08X ",readmeml(e+c));
//                                        fputs(s,arculf);
                                }
//                                fputs("\n",arculf);
//                                for (e=0;e<d;e++)
//                                    putc(readmemb(e+c),arculf);
//                                fputs("\n",arculf);
                                c+=d;
                                ff+=d;
/*                                sprintf(s,"%08X %08X %08X %08X %08X %08X %08X %08X\n",readmeml(c),readmeml(c+4),readmeml(c+8),readmeml(c+12),readmeml(c+16),readmeml(c+20),readmeml(c+24),readmeml(c+28));
                                fputs(s,arculf);
                                for (d=0;d<32;d++)
                                    putc(readmemb(d+c),arculf);
                                fputs("\n",arculf);
                                c+=32;*/
                                armregs[3]++;
                        } while (al_findnext(&ffblk) == 0);
                        al_findclose(&ffblk);
                        armregs[15]&=~VFLAG;
                        armregs[4]=0xFFFFFFFF;
                        break;
                        case 0x10: /*Shutdown*/
                        armregs[15]&=~VFLAG;
                        closearculfs();
                        break;
                        default:
                        sprintf(s,"Bad FSEntry_Func call %i %08X %08X %08X %08X\n",call,armregs[0],armregs[1],armregs[2],armregs[3]);
                        MessageBox(NULL,s,s,MB_OK);
                        dumpregs();
                        exit(-1);
                }
                break;
                
                case 8: /*Print routine - get next character from buffer*/
                if (arculfsprinting==-1)
                {
                        armregs[15]&=~VFLAG;
                        break;
                }
                armregs[0]=printoutput[arculfsprinting++];
                if (!armregs[0])
                {
                        arculfsprinting=-1;
                        armregs[15]&=~VFLAG;
                        break;
                }
                armregs[15]|=VFLAG;
                break;

                default:
                sprintf(s,"Bad ArculFS call %i %08X %08X %08X %08X\n",call,armregs[0],armregs[1],armregs[2],armregs[3]);
                MessageBox(NULL,s,s,MB_OK);
                dumpregs();
                exit(-1);
        }
}
