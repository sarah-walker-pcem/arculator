/*Arculator 0.8 by Tom Walker
  FDI interface*/

int inscount;
int timetolive;
#include <stdio.h>
#include <zlib.h>
#include "arc.h"
#include "fditoraw.h"

int fdcsectorsize;
unsigned short fdibuffer;
int ddidbitsleft=0;
int pollbytesleft=0,pollbitsleft=0;
int readidpoll=0,readdatapoll=0;
unsigned char fdisector[6];
int sectorsize;
unsigned short crc;
unsigned char sectorcrc[2];

int ddensity;
int fdiin[4];
int fditype[4];
int fdcside;
char err2[256];
FILE *olog;
FDI *fdi[4];
FILE *fdifiles[4];
gzFile *fdifilesz[4];
int fdiopen[4]={0,0,0,0};
unsigned char mfmbufhd[4][2][0x80000];
unsigned char mfmbuf[4][2][0x40000];
unsigned char mfmbufsd[4][2][0x20000];
unsigned short tracktiming[0x20000];
int fdilenhd[4][2],indexhhd[4][2];
int fdilen[4][2],indexh[4][2];
int fdilensd[4][2],indexhsd[4][2];
int fdilasttrack[4];
int fdipos;
int fdinextsector=0;
int fdisect,fditrack;
void (*fdiwrite)(unsigned char dat, int last);
void (*datacrcerrorfdi)();
void (*headercrcerrorfdi)();
void (*sectornotfoundfdi)();

FILE *fdidmp;

int indexpasses=0;

int lastfditrack=-1;

int discformat[4];
int writeprot[4];
int discsectors[4];
int discdensity[4];

unsigned short CRCTable[256];

void setupcrc(unsigned short poly, unsigned short rvalue)
{
	int c = 256, bc;
	unsigned short crctemp;

	while(c--)
	{
		crctemp = c << 8;
		bc = 8;

		while(bc--)
		{
			if(crctemp&0x8000)
			{
				crctemp = (crctemp << 1) ^ poly;
			}
			else
			{
				crctemp <<= 1;
			}
		}

		CRCTable[c] = crctemp;
	}
}


void calccrc(unsigned char byte)
{
	crc = (crc << 8) ^ CRCTable[(crc >> 8)^byte];
/*        int i;
	for (i = 0; i < 8; i++) {
		if (crc & 0x8000) {
			crc <<= 1;
			if (!(byte & 0x80)) crc ^= 0x1021;
		} else {
			crc <<= 1;
			if (byte & 0x80) crc ^= 0x1021;
		}
		byte <<= 1;
	}*/
}

void fdiinit(void (*func)(), void (*func2)(), void (*func3)())
{
        datacrcerrorfdi=func;
        headercrcerrorfdi=func2;
        sectornotfoundfdi=func3;
}

void fdiload(char *fn, int drv, int type)
{
        FILE *f;
        gzFile *gf;
//        if (!fdidmp) fdidmp=fopen("fdi.dmp","wb");
        drv&=3;
        if (fdiopen[drv])
        {
                if (fditype[drv]) gzclose(fdifilesz[drv]);
                else              fclose(fdifiles[drv]);
        }
        if (type) fdifilesz[drv]=gf=gzopen(fn,"rb");
        else      fdifiles[drv]=f=fopen(fn,"rb");
        fdiin[drv]=1;
        fditype[drv]=type;
        if (type) fdi[drv]=mfm_open(gf);
        else      fdi[drv]=fditoraw_open(f);
        if (!fdi[drv])
        {
                error("FDI not opened!");
                exit(-1);
        }
        if (type) fdilasttrack[drv]=(mfm_get_last_track(fdi[drv])/2)-1;
        else      fdilasttrack[drv]=(fdi2raw_get_last_track(fdi[drv])/2)-1;
        memset(mfmbuf[drv][0],0,0x40000);
        memset(mfmbuf[drv][1],0,0x40000);
        fdiopen[drv]=1;
        lastfditrack=-1;
        discformat[drv]=0;
        writeprot[drv]=1;
        discdensity[drv]=2;
        discsectors[drv]=5;
        setupcrc(0x1021, 0xcdb4);
        fdihd[drv]=fdi[drv]->hd;
        fdiseek(0);
//        fputs("Changed disc\n",olog);
}

void fdiseek(int track)
{
        int tracklen,indexoffset;
        int temp,c;
        unsigned short tempd[0x80000];
        unsigned char *mfmbufb;
        rpclog("Seek to track %i %i\n",track,curdrive);
        if (track==lastfditrack) return;
        lastfditrack=track;
        if (track>82) track=fdilasttrack[curdrive];
//        if (!olog) olog=fopen("olog.txt","wt");

        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,2);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,2);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x80000) tracklen=0x80000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbufhd[curdrive][0],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbufhd[curdrive][0][c]=mfmbufb[c];
        }
        fdilenhd[curdrive][0]=tracklen;
        indexhhd[curdrive][0]=0;//indexoffset;
        /*FDI, buffer, timing (ignore), tracknum, &tracklen, &indexoffset*/
        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,2);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,2);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x80000) tracklen=0x80000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbufhd[curdrive][1],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbufhd[curdrive][1][c]=mfmbufb[c];
        }
        fdilenhd[curdrive][1]=tracklen;
        indexhhd[curdrive][1]=0;//indexoffset;

        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,1);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,1);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x40000) tracklen=0x40000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbuf[curdrive][0],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbuf[curdrive][0][c]=mfmbufb[c];
        }
        fdilen[curdrive][0]=tracklen;
        indexh[curdrive][0]=0;//indexoffset;
        /*FDI, buffer, timing (ignore), tracknum, &tracklen, &indexoffset*/
        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,1);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,1);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x40000) tracklen=0x40000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbuf[curdrive][1],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbuf[curdrive][1][c]=mfmbufb[c];
        }
        fdilen[curdrive][1]=tracklen;
        indexh[curdrive][1]=0;//indexoffset;

        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,0);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,track<<1,&tracklen,&indexoffset,&mr,0);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x20000) tracklen=0x20000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbufsd[curdrive][0],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbufsd[curdrive][0][c]=mfmbufb[c];
        }
        fdilensd[curdrive][0]=tracklen;
        indexhsd[curdrive][0]=0;//indexoffset;
        if (fditype[curdrive]) temp=mfm_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,0);
        else                   temp=fdi2raw_loadtrack(fdi[curdrive],tempd,tracktiming,(track<<1)+1,&tracklen,&indexoffset,&mr,0);
        mfmbufb=(unsigned char *)tempd;
        if (tracklen>0x20000) tracklen=0x20000;
        if (indexoffset>tracklen) indexoffset=1;
        if (!temp) memset(mfmbufsd[curdrive][1],0,tracklen);
        else
        {
                for (c=0;c<tracklen;c++) mfmbufsd[curdrive][1][c]=mfmbufb[c];
        }
        fdilensd[curdrive][1]=tracklen;
        indexhsd[curdrive][1]=0;//indexoffset;
        
        fdipos=30000;
//        sprintf(err2,"Read track %i - %i %i  %i %i %i\n",track,fdilen[curdrive][0],indexh[curdrive][0],tracklen,indexoffset,fdilasttrack[curdrive]);
//        fputs(err2,olog);
}

int dumpthissector=0;
void fdireadsector(int sector, int track, void (*func)(unsigned char dat, int last))
{
//        sector=65364;
//        if (!olog) olog=fopen("olog.txt","wt");
//        sprintf(s,"read sector %i %i\n",sector,track);
//        fputs(s,olog);
        inreadop=1;
        fdinextsector=0;
        fdisect=sector;
        fditrack=track;
        fdiwrite=func;
        indexpasses=0;
        pollbitsleft=readdatapoll=0;
        rpclog("FDI read sector %i %i %i  %i %i  %i\n",sector,track,fdcside&1,ddensity,hdensity,inscount);
        dumpthissector=0;//(sector==242) && (track==79) && (ddensity==0);
}

void fdiclose()
{
        int c;
        for (c=0;c<4;c++)
        {
                if (fdiopen[c])
                {
                        if (fditype[c]) gzclose(fdifiles[c]);
                        else            fclose(fdifiles[c]);
                }
        }
}

unsigned char decodefm(unsigned short dat)
{
        unsigned char temp;
        temp=0;
        if (dat&0x0001) temp|=1;
        if (dat&0x0004) temp|=2;
        if (dat&0x0010) temp|=4;
        if (dat&0x0040) temp|=8;
        if (dat&0x0100) temp|=16;
        if (dat&0x0400) temp|=32;
        if (dat&0x1000) temp|=64;
        if (dat&0x4000) temp|=128;
        return temp;
}

unsigned short lastfdidat[2];
void fdinextbit()
{
        int tempi;
        int c,d;
//        int limit=(ddensity && hdensity)?32:16;
        for (d=0;d<16;d++)
        {
//        if (!olog) olog=fopen("olog.txt","wt");
        if ((fdipos>=fdilen[curdrive&3][fdcside&1] && ddensity && !hdensity) ||
            (fdipos>=fdilenhd[curdrive&3][fdcside&1] && ddensity && hdensity) ||
            (fdipos>=fdilensd[curdrive&3][fdcside&1] && !ddensity && !hdensity))
        {
//                rpclog("Disc real looping %i %i %i %i %i\n",fdilen[curdrive&3][fdcside&1],fdilenhd[curdrive&3][fdcside&1],ddensity,hdensity,fdipos);
//rpclog("Disc looping %i %i\n",inscount,fdipos);
                fdipos=0;
        }
        if (inreadop)
        {
                if (hdensity)
                   tempi=mfmbufhd[curdrive&3][fdcside&1][(fdipos>>3)&0x1FFFF]&(1<<(7-(fdipos&7)));
                else if (ddensity)
                   tempi=mfmbuf[curdrive&3][fdcside&1][(fdipos>>3)&0xFFFF]&(1<<(7-(fdipos&7)));
                else
                   tempi=mfmbufsd[curdrive&3][fdcside&1][(fdipos>>3)&0xFFFF]&(1<<(7-(fdipos&7)));
                fdibuffer<<=1;
                fdibuffer|=(tempi?1:0);
//                rpclog("%i %04X %02X %i  %02X  %i %i\n",fdipos,fdibuffer,mfmbuf[curdrive&3][fdcside&1][(fdipos>>3)&0xFFFF],ddensity,decodefm(fdibuffer),hdensity,ddensity);
//                if (!(fdibuffer&15)) rpclog("Bad data low!\n");
//                if ((fdibuffer&3)==3) rpclog("Bad data high!\n");
//                rpclog("%04X\n",fdibuffer);
                if (pollbitsleft)
                {
//                        if (!fdibuffer&15) rpclog("Invalid MFM! 0\n");
//                        if ((fdibuffer&3)==3) rpclog("Invalid MFM! 1\n");
                        pollbitsleft--;
                        if (!pollbitsleft)
                        {
                                if (readtrack==2)
                                {
                                        readtrackdata(decodefm(fdibuffer));
                                }
                                pollbytesleft--;
                                if (pollbytesleft) pollbitsleft=16; /*Set up another word if we need it*/
                                if (readidpoll)
                                {
                                        fdisector[5-pollbytesleft]=decodefm(fdibuffer);
                                        if (!pollbytesleft)
                                        {
                                                rpclog("Found header : %02X %02X %02X %02X %02X %02X\n",fdisector[0],fdisector[1],fdisector[2],fdisector[3],fdisector[4],fdisector[5]);
                                                if ((fdisector[0]==fditrack && fdisector[2]==fdisect) || readidcommand)
                                                {
                                                if (ddensity) crc=0xcdb4;
                                                else          crc=0xffff;
                                                calccrc(0xFE);
                                                for (c=0;c<4;c++) calccrc(fdisector[c]);
                                                if ((crc>>8)!=fdisector[4] || (crc&0xFF)!=fdisector[5])
                                                {
                                                        rpclog("Header CRC error : %02X %02X %02X %02X\n",crc>>8,crc&0xFF,fdisector[4],fdisector[5]);
//                                                        output=1;
//                                                        timetolive=200000;
//                                                        dumpregs();
//                                                        exit(-1);
                                                        inreadop=0;
                                                        if (readidcommand) readidresult(fdisector,1);
                                                        else               headercrcerrorfdi();
                                                        return;
                                                }
                                                if (readidcommand) readidresult(fdisector,0);
//                                                fputs(s,olog);
                                                else if (fdisector[0]==fditrack && fdisector[2]==fdisect)
                                                {
                                                        fdinextsector=1;
                                                        readidpoll=0;
                                                        sectorsize=(1<<(fdisector[3]+7))+2;
                                                        fdcsectorsize=fdisector[3];
                                                        rpclog("Read this sector!!! %i\nSize %i bytes   %i %i %i %i\n",fdipos,sectorsize,fdisector[0],fditrack,fdisector[2],fdisect);
//                                                        if (fdisector[0]==0x40 && fdisector[1]==1 && fdisector[2]==0) dumpthissector=1;
//                                                        else  dumpthissector=0;
//                                                        fputs(s,olog);
                                                }
                                                }
                                        }
                                }
                                if (readdatapoll)
                                {
                                        if (dumpthissector) rpclog("Data %02X %02X %02X %04X %02X %i %i\n",decodefm(lastfdidat[1]),sectorcrc[0],sectorcrc[1],fdibuffer,decodefm(fdibuffer),pollbytesleft,fdipos);
                                        if (fditrack==79 && fdisect==4 && fdcside&1)
                                        {
//                                                rpclog("Data %02X %i\n",decodefm(lastfdidat[1]),pollbytesleft);
                                        }
//                                        rpclog("Read data poll %i\n",pollbytesleft);
                                        if (pollbytesleft>1)
                                        {
                                                calccrc(decodefm(fdibuffer));
                                        }
                                        else
                                           sectorcrc[1-pollbytesleft]=decodefm(fdibuffer);
                                        if (!pollbytesleft)
                                        {
                                                if ((crc>>8)!=sectorcrc[0] || (crc&0xFF)!=sectorcrc[1])// || (fditrack==79 && fdisect==4 && fdcside&1))
                                                {
                                                        rpclog("Data CRC error : %02X %02X %02X %02X %i %04X %02X%02X %i\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1],fdipos,crc,sectorcrc[0],sectorcrc[1],fdilen[curdrive&3][fdcside&1]);
//                                                        fputs(s,olog);
                                                        inreadop=0;
                                                        fdiwrite(decodefm(lastfdidat[1]),1);
                                                        datacrcerrorfdi();
                                                        readdatapoll=0;
                                                        return;
                                                }
                                                rpclog("End of FDI read %02X %02X %02X %02X\n",crc>>8,crc&0xFF,sectorcrc[0],sectorcrc[1]);
                                                fdiwrite(decodefm(lastfdidat[1]),1);
                                        }
                                        else if (lastfdidat[1]!=0)
                                                   fdiwrite(decodefm(lastfdidat[1]),0);
                                        lastfdidat[1]=lastfdidat[0];
                                        lastfdidat[0]=fdibuffer;
//                                        fputc(decodefm(fdibuffer),fdidmp);
                                        if (!pollbytesleft)
                                           readdatapoll=0;
                                }
                        }
                }
                else if (fdibuffer==0x4489 && ddensity)
                {
                        rpclog("Found sync %i\n",readdatapoll);
                        ddidbitsleft=17;
                }
                else if (fdibuffer==0x8944 && ddensity)
                {
//                        rpclog("Found wrong sync %i\n",readdatapoll);
                }
                else if (fdibuffer==0xF57E && !ddensity)
                {
                        rpclog("SD sector header\n");
                        pollbytesleft=6;
                        pollbitsleft=16;
                        readidpoll=1;
                }
                else if ((fdibuffer==0xF56F || fdibuffer==0xF56A) && !ddensity)
                {
                        rpclog("SD data header\n");
                        if (fdinextsector)
                        {
                                rpclog("Reading this sector\n");
                                pollbytesleft=sectorsize;
                                pollbitsleft=16;
                                readdatapoll=1;
                                fdinextsector=0;
                                crc=0xffff;
                                if (fdibuffer==0xF56A) calccrc(0xF8);
                                else                   calccrc(0xFB);
                                lastfdidat[0]=lastfdidat[1]=0;
                        }
                }
                if (ddidbitsleft)
                {
                        ddidbitsleft--;
                        if (!ddidbitsleft)
                        {
                                rpclog("ID bits over %04X %02X %i\n",fdibuffer,decodefm(fdibuffer),fdipos);
                                if (decodefm(fdibuffer)==0xFE)
                                {
                                        rpclog("Sector header\n");
                                        pollbytesleft=6;
                                        pollbitsleft=16;
                                        readidpoll=1;
                                        readflash[0]=1;
                                }
                                else if (decodefm(fdibuffer)==0xFB)
                                {
//                                        dataheader=1;
                                        rpclog("Data header\n");
                                        if (fdinextsector)
                                        {
//                                                rpclog("Reading this sector %i\n",fdipos);
                                                pollbytesleft=sectorsize;
                                                pollbitsleft=16;
                                                readdatapoll=1;
                                                fdinextsector=0;
                                                crc=0xcdb4;
                                                if (fdibuffer==0xF56A) calccrc(0xF8);
                                                else                   calccrc(0xFB);
                                                lastfdidat[0]=lastfdidat[1]=0;
//                                                rpclog("Pollbytesleft %i\n",sectorsize);
                                        }
                                }
                        }
                }
        }
        if (((fdipos==indexhhd[curdrive&3][fdcside&1] && ddensity && hdensity) ||(fdipos==indexh[curdrive&3][fdcside&1] && ddensity && !hdensity) || (fdipos==indexhsd[curdrive&3][fdcside&1] && !ddensity))&& inreadop)
        {
//                rpclog("Disc loop %i %i %i\n",ddensity,indexpasses);
                indexpasses++;
                if (indexpasses==6 && !readidcommand)
                {
                        inreadop=0;
                        rpclog("Sector not found\n");
                        sectornotfoundfdi();
                }
                if (readtrack==1)
                {
                        readtrack=2;
                        pollbitsleft=16;
                        pollbytesleft=65536;
                }
                else if (readtrack==2)
                {
                        readtrack=0;
                        finishreadtrack();
                }
        }
        fdipos++;
//        rpclog("%i\n",fdipos);
}
}
