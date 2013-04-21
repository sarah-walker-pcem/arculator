/*Arculator 0.8 by Tom Walker
  WD1772 FDC emulation
  Possibly a bug somewhere*/
#include <stdio.h>
#include "arc.h"
#include <allegro.h>
#include <winalleg.h>

int discdensity[4];
int discsectors[2];
int fdiin[4];
int fdcready=4;
int discchanged[4]={0,0,0,0};
int readflash[4];
char err2[256];
FILE *olog;
int readaddr=0;
int output,output2;
int fdcside;
int commandpos=0;
int fiq;
int track=0,sector=1;
unsigned char fdctrack=0,fdcsector=0,fdcdata,fdcstatus=0;
int discint=0;
unsigned short fdccommand;
int discformat[4];
int writeprot[4];

void loaddisc(char *fn, int dosdisc, int drive)
{
        FILE *ff=fopen(fn,"rb");
        int c,d,e,f;
        fdiin[drive]=0;
        if (!ff)
        {
                discchangeint(0,drive);
                return;
        }
        fseek(ff,-1,SEEK_END);
        if (ftell(ff)>1000000)
        {
                discdensity[drive]=0;
                discsectors[drive]=10;
        }
        else
        {
                discdensity[drive]=2;
                discsectors[drive]=5;
        }
        fseek(ff,0,SEEK_SET);
        rpclog("Load %s %i %i\n",fn,drive,dosdisc);
//        fseek(ff,-1,SEEK_END);
//        c=ftell(ff);
//        fseek(ff,0,SEEK_SET);
        for (d=0;d<80;d++)
        {
                for (c=0;c<2;c++)
                {
                        if (dosdisc)
                        {
                                for (e=0;e<9;e++)
                                {
                                        for (f=0;f<512;f++)
                                        {
                                                disc[drive][c][d][e][f]=getc(ff);
                                        }
                                }
                        }
                        else
                        {
/*                                if (c<(700*1024))
                                {
                                for (e=0;e<16;e++)
                                {
                                        for (f=0;f<256;f++)
                                        {
                                                disc[drive][c][d][e][f]=getc(ff);
                                        }
                                }
                                dosdisc=2;
                                }
                                else
                                {*/
                                for (e=0;e<discsectors[drive];e++)
                                {
                                        for (f=0;f<1024;f++)
                                        {
                                                disc[drive][c][d][e][f]=getc(ff);
                                        }
                                }
//                                dosdisc=0;
//                                }
                        }
                }
        }
        fclose(ff);
        ff=fopen(fn,"ab");
        if (!ff)
           writeprot[drive]=1;
        else
        {
                fclose(ff);
                writeprot[drive]=0;
        }
        discformat[drive]=dosdisc;
        track=sector=0;
//        fdcstatus=0;
}

void updatedisc(char *fn, int drive)
{
        FILE *ff;
        int c,d,e,f;
//        rpclog("Update disc %i %i %i %i %s\n",fdiin[drive],discchanged[drive],writeprot[drive],drive,fn);
/*        newdisc=fopen("new.adf","wb");
//        fwrite(newdiscd,2*80*5*1024,1,newdisc);
        for (d=0;d<80;d++)
        {
                for (c=0;c<2;c++)
                {
                        for (e=0;e<5;e++)
                        {
                                for (f=0;f<1024;f++)
                                {
                                        putc(newdiscd[c][d][e][f],newdisc);
                                }
                        }
                }
        }
        fclose(newdisc);*/
        if (fdiin[drive]) return;
        if (!discchanged[drive]) return;
        if (writeprot[drive])    return;
        ff=fopen(fn,"wb");
        if (!ff)
        {
//                error("Failed to open file %s for write\n",fn);
                return;
        }
//        rpclog("Updating %s with %i sectors\n",fn,discsectors[drive]);
        for (d=0;d<80;d++)
        {
                for (c=0;c<2;c++)
                {
                        if (discformat[drive])
                        {
                                for (e=0;e<9;e++)
                                {
                                        for (f=0;f<512;f++)
                                        {
                                                putc(disc[drive][c][d][e][f],ff);
                                        }
                                }
                        }
                        else
                        {
                                for (e=0;e<discsectors[drive];e++)
                                {
                                        for (f=0;f<1024;f++)
                                        {
                                                putc(disc[drive][c][d][e][f],ff);
                                        }
                                }
                        }
                }
        }
        fclose(ff);
        discchanged[drive]=0;
}

void callback()
{
        switch (fdccommand>>4)
        {
                case 0x0: /*Restore*/
                if (fdiin[curdrive])
                   fdiseek(0);
                track=fdctrack=0;
                iocfiq(2);
                discint=0;
                fdcstatus=0xA4;
                fdcready=4;
//                fdcstatus|=0x04;
//                fdcstatus&=~1;
//                printf("Restore callback\n");
                break;

                case 0x1: /*Seek*/
                if (fdiin[curdrive])
                   fdiseek(fdcdata);
                fdctrack=track=fdcdata;
                iocfiq(2);
                discint=0;
                fdcstatus=0xA0;
//                fdcstatus|=0xA0;
//                fdcstatus&=~5;
                if (!track) fdcstatus|=4;
                commandpos=0;
                fdcready=4;
//                printf("Seek callback %02X - seeked to %02X\n",fdccommand,track);
                break;

                case 0x5: /*Step in*/
                track++;
//                if (track==80) track=79;
                if (fdccommand&0x10) fdctrack=track;
                if (fdiin[curdrive])
                   fdiseek(track);
                iocfiq(2);
                discint=0;
//                fdcstatus&=~5;
//                fdcstatus|=0xA0;
                fdcstatus=0xA0;
                commandpos=0;
                fdcready=4;
                break;

                case 0x7: /*Step out*/
                track--;
                if (track==-1) track=0;
                if (fdccommand&0x10) fdctrack=track;
                if (fdiin[curdrive])
                   fdiseek(track);
                iocfiq(2);
                discint=0;
//                fdcstatus&=~5;
//                fdcstatus|=0xA0;
                fdcstatus=0xA0;
                commandpos=0;
                fdcready=4;
                break;

                case 0x8: /*Read sector*/
//                rpclog("Read sector %i %i %i\n",commandpos,fdiin[curdrive],commandpos);
//                fputs(s,olog);
//                rpclog("Read sector %i\n",commandpos);
                if (fdiin[curdrive] && commandpos!=1024) return;
//                rpclog("Finish read sector command\n");
//                fputs(s,olog);
/*                if (sector>9 || track>79 || commandpos>1024)
                {
                        sprintf(err2,"Bad read sector %i %i %i\n",track,sector,commandpos);
                        MessageBox(NULL,err2,"Arc",MB_OK);
                        dumpregs();
                        exit(-1);
                }*/
/*                if (!commandpos)
                {
                        track=fdctrack;
                        sector=fdcsector;
                        printf("Sector command end\n");
                }*/
//                if (discformat[curdrive]==2 && commandpos==256) c=1;
//                else                                            c=0;
                if (commandpos==((discformat[curdrive])?512:1024)/* || c*/)
                {
                        iocfiq(2);
                        fdcstatus&=~1;
                        fdcstatus=0x80;
                        fdcready=4;
//                        fputs("End of read command\n",olog);
                        discint=25000;
                        fdccommand=0xFF0;
                        inreadop=0;
                }
                else
                {
                        if (fdcstatus&2) rpclog("Overflow!\n");
//                        rpclog("Read dat! %i %i\n",commandpos,discformat[curdrive]);
                        if (discformat[curdrive]==1)
                           fdcdata=disc[curdrive][fdcside&1][track%80][(sector-1)%9][commandpos&511];
                        else
                           fdcdata=disc[curdrive][fdcside&1][track%80][sector%5][commandpos&1023];
//                        else if (discformat[curdrive]==2)
//                           fdcdata=disc[curdrive][fdcside&1][track%80][sector%16][commandpos&255];
                        commandpos++;
                        iocfiq(1);
                        fdcstatus=0x83;
//                        fdcstatus|=2;
                        discint=100>>1;
                        if ((fdccommand>>4)==8 && !fdiin[curdrive] && fastdisc && commandpos!=((discformat[curdrive])?512:1024)) discint=0;
//                        if (speed>1 && fastdisc) discint=250;
//                        fputs("Normal read command\n",olog);                        
                }
                break;

                case 0xA: /*Write sector*/
/*        fdcready=1;
        fdccommand=0;
        commandpos=0;
        discint=0;
        fdcstatus=0xC0;
        iocfiqc(1);
        iocfiq(2);
        return;                */
                        discchanged[curdrive]=1;
                if (writeprot[curdrive])
                {
                        commandpos=0;
                        iocfiq(2);
                        fdcstatus=0xC0;
                        discint=0;
                        return;
                }
                if (commandpos==-1)
                {
                        commandpos=0;
                        iocfiq(1);
                        fdcstatus=0x83;
                        discint=100;
                        return;
                }
                if (commandpos==((discformat[curdrive])?512:1024))
                {
                        iocfiq(2);
                        fdcstatus&=~1;
                        fdcstatus=0x80;
                        fdcready=4;
                }
                else
                {
//                        sprintf(err2,"%03X: %02X - %02X %i %i\n",commandpos,disc[fdcside&1][track%80][sector%5][commandpos&1023],fdcdata,track,sector);
//                        fputs(err2,olog);
                        if (discformat[curdrive])
                           disc[curdrive][fdcside&1][track%80][(sector-1)%9][commandpos&511]=fdcdata;
                        else
                           disc[curdrive][fdcside&1][track%80][sector%5][commandpos&1023]=fdcdata;
                        commandpos++;
                        if (commandpos!=((discformat[curdrive])?512:1024)) iocfiq(1);
                        fdcstatus=0x83;
//                        fdcstatus|=2;
                        discint=80;
//                        if (speed>1 && fastdisc) discint=250;
                }
                break;

                case 0xC: /*Read address*/
                discint=5000;
                iocfiq(1);
                fdcstatus=0x81;
                switch (commandpos--)
                {
                        case 6: fdcdata=track; break;
                        case 5: fdcdata=(fdcside)?1:0; break;
                        case 4:
                                fdcdata=sector;
                                sector++;
//                                if (discformat[curdrive]==2 && sector==16) c=1;
//                                else                                       c=0;
                                if (sector==((discformat[curdrive])?9:5))/* && discformat[curdrive]!=2) || c)*/
                                   sector=0;
                                if (discformat[curdrive]==1)
                                   fdcdata++;
                                break;
                        case 3: if (discformat[curdrive]==1) fdcdata=2; else fdcdata=3; /*if (discformat[curdrive]==2) fdcdata=1; */break;
                        case 2: fdcdata=0xA5; break;
                        case 1: fdcdata=0x5A; break;
                        case 0:
                        discint=0;
                        iocfiqc(1);
                        iocfiq(2);
                        fdcstatus=0;//0x80;
                        fdcsector=track;
                        readaddr++;
                        fdcready=4;
//                        if (readaddr==8)
//                           output2=1;
                }
//                printf("Read address %i %02X\n",commandpos,fdcdata);
                break;
                case 0xF: /*Write track*/
                if (writeprot[curdrive])
                {
                        commandpos=0;
                        iocfiq(2);
                        fdcstatus=0xC0;
                        discint=0;
                        return;
                }
                if (discformat[curdrive])
                   disc[curdrive][fdcside&1][track%80][(sector-1)%9][commandpos&511]=fdcdata;
                else
                   disc[curdrive][fdcside&1][track%80][sector%5][commandpos&1023]=fdcdata;
                commandpos++;
                if (commandpos==((discformat[curdrive])?512:1024))
                {
                        commandpos=0;
                        sector++;
                        if (sector==((discformat[curdrive])?9:5))
                        {
                                iocfiq(2);
                                fdcstatus&=~1;
                                fdcstatus=0x80;
                                fdcready=4;
                                return;
                        }
                }
                discint=80;
                break;

                case 0xFF:
                fdcstatus=0;
                iocfiq(2);
//                fputs("End of all commands\n",olog);
                break;
                case 0xFE:
                fdcready=1;
                fdccommand=0;
                commandpos=0;
                discint=0;
                fdcstatus=0x88;
                iocfiqc(1);
                iocfiq(2);
                break;
        }
}

void finishreadtrack()
{
        fdcstatus=0;
        iocfiq(2);
}
void readtrackdata(unsigned char val)
{
        iocfiq(1);
        fdcstatus=0x83;
        fdcdata=val;
}

/*Give up command - disc is ejected*/
void giveup1772()
{
        discint=0;
        fdcstatus=0;
}

int databytes=0;
void readdata1772(unsigned char dat, int last)
{
        if (last)
        {
                discint=500>>1;
                commandpos=1024;
//                rpclog("Last\n");
                inreadop=0;
//                return;
        }
        databytes++;
        rpclog("Data %04i %02X %i %i %i\n",databytes,dat,discformat[curdrive],curdrive,last);
//        sprintf(err2,"Data bytes %i\n",databytes);
//        fputs(err2,olog);
        if (fdcstatus&2) rpclog("Overflow!\n");
        fdcdata=dat;
        iocfiq(1);
        if (fdcstatus&2) fdcstatus=0x87;
        else             fdcstatus=0x83;
//        newdiscd[fdcside&1][track][sector][newdiscpos++]=dat;
}

void write1772(unsigned addr, unsigned data)
{
        hdensity=0;
//        if (!olog) olog=fopen("olog.txt","wt");
//        sprintf(bigs,"1772 write %01X %02X %07X\n",addr,data,PC);
//        fputs(bigs,olog);
//        printf("1770 write %08X %04X\n",addr,data);
        switch (addr&0xC)
        {
                case 0: /*Command reg*/
//                rpclog("Write command %02X %i\n",data,fdcstatus);
//                if (!olog) olog=fopen("olog.txt","wt");
//                sprintf(bigs,"1772 command %02X\n",data);
//                fputs(bigs,olog);
                if (fdcstatus&1)
                {
                        rpclog("ALREADY IN COMMAND!\n");
                        return;
                }
/*                {
                        sprintf(err2,"Rejected 1772 command %02X\nLast command %02X status %02X pos %i",data,fdccommand,fdcstatus,commandpos);
                        MessageBox(NULL,err2,"Arc",MB_OK);
                        dumpregs();
                        exit(-1);
                }*/
                fdcready=0;
//                log("1770 command %02X\n",data>>16);
                fdccommand=data;
                fdcstatus=0x81;
                switch ((data>>4)&0xF)
                {
                        case 0x0: /*Restore*/
                        discint=(50000*fdctrack)/4;
                        if (!discint) discint=50000;
//                        fdcstatus=1;//(fdcstatus|1)&~0x1A;
                        break;

                        case 0x1: /*Seek*/
//                        sprintf(bigs,"Seek to track %i\n",fdcdata);
//                        fputs(bigs,olog);
                        iocfiqc(2);
                        discint=(50000*ABS(fdcdata-fdctrack))/4;
                        if (!discint) discint=50000;
//                        printf("Seek time is %i\n",discint);
//                        fdcstatus=1;//(fdcstatus|1)&~0x1A;
                        break;

                        case 0x5: /*Step in*/
                        discint=50000/4;//1000>>2;
//                        fdcstatus=1;//(fdcstatus|1)&~0x1A;
                        break;

                        case 0x7: /*Step out*/
                        discint=50000/4;//1000>>2;
//                        fdcstatus=1;//(fdcstatus|1)&~0x1A;
                        break;

                        case 0x8: /*Read sector*/
                        readflash[curdrive]=1;
//                        discint=500>>2;
                        commandpos=0;
//                        fdcstatus&=~4;
//                        fdcstatus|=1;
                        track=fdctrack;
                        sector=fdcsector;
                        if (fdiin[curdrive])
                           fdireadsector(sector,track,readdata1772);
                        else
                           discint=500>>2;
                        rpclog("Read head %i track %i sector %i\n",fdcside,track,sector);
//                        fputs(bigs,olog);
//                        if (fdccommand&4) discint+=15000;
                        break;

                        case 0xA: /*Write sector*/
                        readflash[curdrive]=1;
                        discint=1000>>2;
                        commandpos=-1;
//                        fdcstatus&=~4;
//                        fdcstatus|=1;
                        track=fdctrack;
                        sector=fdcsector;
                        break;

                        case 0xC: /*Read address*/
                        commandpos=6;
                        discint=5000>>1;
//                        fdcstatus=(fdcstatus|1)&~0x7E;
//                        fdcstatus&=~4;
//                        fdcstatus|=1;
//                        log("read %i %i %i\n",track,fdcside,sector);
                        break;

                        case 0xD: /*Force interrupt*/
                        if (fdccommand&8) iocfiq(2);
                        else              iocfiqc(2);
                        discint=0;
                        fdcstatus=0x80;
                        fdcready=4;
                        break;
                        
                        case 0xE: /*Read track*/
                        readflash[curdrive]=1;
                        commandpos=0;
                        inreadop=1;
                        readtrack=1;
                        break;
                        
                        case 0xF: /*Write track*/
                        readflash[curdrive]=1;
                        discint=1000>>2;
                        commandpos=0;
//                        fdcstatus&=~4;
//                        fdcstatus|=1;
                        track=fdctrack;
                        sector=0;
                        break;

                        default:
                        allegro_exit();
                        sprintf(err2,"Bad WD1772 FDC command %01X\n",(data>>4)&0xF);
                        MessageBox(NULL,err2,"Arc",MB_OK);
                        exit(-1);
                }
                break;
                case 4: /*Track reg*/
                fdctrack=data;
//                printf("Track reg = %02X %i\n",data,data);
                break;
                case 8: /*Sector reg*/
                fdcsector=data;
//                printf("Sector reg = %02X %i\n",data,data);
                break;
                case 0xC: /*Data reg*/
                fdcstatus&=~2;
//                sprintf(s,"FDC data write %02X\n",data);
//                fputs(s,olog);
                fdcdata=data;
//                rpclog("FDC data write %02X\n",data);
                iocfiqc(1);
                break;
        }
//        log("1770 write addr %08X data %02X\n",addr,data);
}

unsigned char read1772(unsigned addr)
{
        unsigned char temp;
//        if (!olog) olog=fopen("olog.txt","wt");
//        printf("1770 read %08X\n",addr);
        switch (addr&0xC)
        {
                case 0: /*Status reg*/
                iocfiqc(2);
//                rpclog("Returning status %02X %07X %08X\n",fdcstatus,PC);
//                output=1;
//                fputs(s,olog);
                return fdcstatus;
                case 4: /*Track reg*/
//                printf("Reading fdctrack\n");
                return fdctrack;
                case 8: /*Sector reg*/
//                printf("Reading fdcsector\n");
                return fdcsector;
                case 0xC: /*Data reg*/
                fdcstatus&=~2;
//                rpclog("Read databyte %i %02X %07X\n",databytes,fdcdata,PC);
//                printf("Reading fdcdata\n");
//                sprintf(s,"Returning data %02X %07X %08X\n",fdcdata,PC);
//                fputs(s,olog);
                iocfiqc(1);
                temp=fdcdata;
                if ((fdccommand>>4)==8 && !fdiin[curdrive] && fastdisc && commandpos!=((discformat[curdrive])?512:1024)) callback();
                return temp;
        }
        return 0xef; /*Should never reach here*/
}

void sectornotfound()
{
        rpclog("Sector not found\n");
//        if (!olog) olog=fopen("olog.txt","wt");
        fdcready=1;
        fdccommand=0;
        commandpos=0;
        discint=0;
        fdcstatus=0x90;
        iocfiqc(1);
        iocfiq(2);
//        fputs("Sector not found\n",olog);
}

void headercrcerror()
{
        rpclog("Header CRC error\n");
        fdcready=1;
        fdccommand=0;
        commandpos=0;
        discint=0;
        fdcstatus=0x98;
        iocfiqc(1);
        iocfiq(2);
}

void datacrcerror()
{
        discint=200>>2;
        inreadop=0;
        fdccommand=0xFE0;
        rpclog("Data CRC error\n");
        return;
}

void init1772()
{
        fdiinit(datacrcerror,headercrcerror,sectornotfound);
}
