/*Arculator 0.8 by Tom Walker
  MEMC1a emulation*/

int flybacklines;
#include <stdio.h>
#include "arc.h"
#include "ioc.h"
#include "mem.h"
#include "memc.h"
#include "vidc.h"

#include <allegro.h>

int memc_videodma_enable;
int memc_refreshon;
int memc_is_memc1 = 1;

int sound_poll_time;

uint32_t memctrl;
int16_t soundbuf[8][50000],soundbuft[50000];
int sinprog=0;
int sdmaena=0;
int bigcyc=0;
char err2[256];
FILE *olog;
int logsound;
int osmode;
int pagesize;
int memcpages[0x2000];
int samppos=0,sampbuf=0;
AUDIOSTREAM *as;
int spdcount;
FILE *soundf;
FILE *slogfile;
int stereoimages[8];

#define BUFFERSIZE 2500

signed short lastbuffer[4]={0,0,0,0};

signed short getsample(signed short temp)
{
//        float tempf;
//        tempf=((float)lastbuffer[0]*((float)9/(float)16))+((float)lastbuffer[1]*((float)5/(float)16))+((float)lastbuffer[2]*((float)1/(float)16));
//        temp+=(tempf/2);
//        lastbuffer[2]=lastbuffer[1];
//        lastbuffer[1]=lastbuffer[0];
//        lastbuffer[0]=temp;
        return temp;
}

/*FILTER!!!*/
#define Ntap 63
    float FIRCoef[Ntap] = {
        0.00049307601000720064,
        -0.00026602169660020815,
        -0.00120050989046158600,
        -0.00228631193414917910,
        -0.00348477946397240250,
        -0.00474272020822737170,
        -0.00599328661908643790,
        -0.00715775789254538470,
        -0.00814819610881924170,
        -0.00887090290544868650,
        -0.00923054960662926350,
        -0.00913480450678616320,
        -0.00849923937693952170,
        -0.00725226623840924570,
        -0.00533983750474521440,
        -0.00272963948601896020,
        0.00058547812455314846,
        0.00458506585897087370,
        0.00921981731333185350,
        0.01441162175884779700,
        0.02005499166045760300,
        0.02601985855835959100,
        0.03215565684603676600,
        0.03829654310012360600,
        0.04426753305726668200,
        0.04989128308656525000,
        0.05499520153416351300,
        0.05941855031940927000,
        0.06301919039834402700,
        0.06567963687368816300,
        0.06731212019220633700,
        0.06786239749301446700,
        0.06731212019220633700,
        0.06567963687368816300,
        0.06301919039834402700,
        0.05941855031940927000,
        0.05499520153416351300,
        0.04989128308656525000,
        0.04426753305726668200,
        0.03829654310012360600,
        0.03215565684603676600,
        0.02601985855835959100,
        0.02005499166045760300,
        0.01441162175884779700,
        0.00921981731333185350,
        0.00458506585897087370,
        0.00058547812455314846,
        -0.00272963948601896020,
        -0.00533983750474521440,
        -0.00725226623840924570,
        -0.00849923937693952170,
        -0.00913480450678616320,
        -0.00923054960662926350,
        -0.00887090290544868650,
        -0.00814819610881924170,
        -0.00715775789254538470,
        -0.00599328661908643790,
        -0.00474272020822737170,
        -0.00348477946397240250,
        -0.00228631193414917910,
        -0.00120050989046158600,
        -0.00026602169660020815,
        0.00049307601000720064
    };
/*    float FIRCoef[Ntap] = {
        0.00142508326436622960,
        0.00176958882960530690,
        0.00201309825128511820,
        0.00208488421852299350,
        0.00191626660827256150,
        0.00144914599138006550,
        0.00064500523302992395,
        -0.00050649121386512036,
        -0.00198023984906743260,
        -0.00371070123537260150,
        -0.00558996552727555620,
        -0.00746947270642584190,
        -0.00916569092879623670,
        -0.01046971072394972100,
        -0.01116034032490727400,
        -0.01101992292889520700,
        -0.00985177647713568120,
        -0.00749791475126982980,
        -0.00385557365186484090,
        0.00110894200412945320,
        0.00735044970549485370,
        0.01474022064621744900,
        0.02306718398524032500,
        0.03204528283590706700,
        0.04132676066826663400,
        0.05052065403774214100,
        0.05921530774040443400,
        0.06700335456824997600,
        0.07350734990665816500,
        0.07840414713810910100,
        0.08144615572497027100,
        0.08247783792194682300,
        0.08144615572497027100,
        0.07840414713810910100,
        0.07350734990665816500,
        0.06700335456824997600,
        0.05921530774040443400,
        0.05052065403774214100,
        0.04132676066826663400,
        0.03204528283590706700,
        0.02306718398524032500,
        0.01474022064621744900,
        0.00735044970549485370,
        0.00110894200412945320,
        -0.00385557365186484090,
        -0.00749791475126982980,
        -0.00985177647713568120,
        -0.01101992292889520700,
        -0.01116034032490727400,
        -0.01046971072394972100,
        -0.00916569092879623670,
        -0.00746947270642584190,
        -0.00558996552727555620,
        -0.00371070123537260150,
        -0.00198023984906743260,
        -0.00050649121386512036,
        0.00064500523302992395,
        0.00144914599138006550,
        0.00191626660827256150,
        0.00208488421852299350,
        0.00201309825128511820,
        0.00176958882960530690,
        0.00142508326436622960
    };*/

//#define Ntap 15

/*    float FIRCoef[Ntap] = {
        0.06005361801289381400,
        0.06280232617950112800,
        0.06519313060567338800,
        0.06719442728252876500,
        0.06877956224199008300,
        0.06992729978216376300,
        0.07062219710122440200,
        0.07085487758804942500,
        0.07062219710122440200,
        0.06992729978216376300,
        0.06877956224199008300,
        0.06719442728252876500,
        0.06519313060567338800,
        0.06280232617950112800,
        0.06005361801289381400
    };*/
/*    float FIRCoef[Ntap] = {
        0.04194225072015333300,
        0.05127307110291310500,
        0.06009721651438715300,
        0.06800125272220478000,
        0.07460211373224015500,
        0.07957184022147292800,
        0.08265915422285705100,
        0.08370620152754298900,
        0.08265915422285705100,
        0.07957184022147292800,
        0.07460211373224015500,
        0.06800125272220478000,
        0.06009721651438715300,
        0.05127307110291310500,
        0.04194225072015333300
    };*/
/*    float FIRCoef[Ntap] = {
        -0.01233332425795921100,
        0.00340588334617153650,
        0.02778411845224726700,
        0.05835742676803153800,
        0.09074867910115647100,
        0.11950287000245449000,
        0.13933130611845909000,
        0.14640608093887761000,
        0.13933130611845909000,
        0.11950287000245449000,
        0.09074867910115647100,
        0.05835742676803153800,
        0.02778411845224726700,
        0.00340588334617153650,
        -0.01233332425795921100
    };*/

float fir(float NewSample) {
    static float x[Ntap]; //input samples
    float y=0;            //output sample
    int n;

    //shift the old samples
    for(n=Ntap-1; n>0; n--)
       x[n] = x[n-1];

    //Calculate the new output
    x[0] = NewSample;
    for(n=0; n<Ntap; n++)
        y += FIRCoef[n] * x[n];

    return y;
}

float fir2(float NewSample) {
    static float x[Ntap]; //input samples
    float y=0;            //output sample
    int n;

    //shift the old samples
    for(n=Ntap-1; n>0; n--)
       x[n] = x[n-1];

    //Calculate the new output
    x[0] = NewSample;
    for(n=0; n<Ntap; n++)
        y += FIRCoef[n] * x[n];

    return y;
}

int sampqueue=0,sampbufr=0;
void mixsound()
{
        int c;
        uint16_t *p;
        uint16_t p2[(25000)>>1];
        short temp;
        float tempf;
//        rpclog("Mixsound %i %i\n",soundena,sampqueue);
        if (!soundena) return;
        if (!sampqueue) return;
        p=0;
        for (c=0;c</*((25000>>1)>>1)*/BUFFERSIZE;c++)
        {
                temp=(getsample(soundbuf[sampbufr][c<<3])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+2])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+4])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+6])/16);
//                tempf=fir((float)temp);
//                temp=(signed short)tempf;
//                tempf=((float)lastbuffer[0]*((float)13/(float)16))+((float)lastbuffer[1]*((float)1/(float)16));
//                temp=tempf*8;
//temp=0;
                lastbuffer[1]=lastbuffer[0];
                lastbuffer[0]=temp;
//                temp=0;
                p2[c<<1]=(temp*2);//^0x8000;
                temp=(getsample(soundbuf[sampbufr][(c<<3)+1])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+3])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+5])/16);
                temp+=(getsample(soundbuf[sampbufr][(c<<3)+7])/16);
//                temp=0;
//                tempf=fir2((float)temp);
//                temp=(signed short)tempf;
//                tempf=((float)lastbuffer[2]*((float)13/(float)16))+((float)lastbuffer[3]*((float)1/(float)16));
//                temp=tempf*8;
//temp=0;
                lastbuffer[3]=lastbuffer[2];
                lastbuffer[2]=temp;
                p2[(c<<1)+1]=(temp*2);//^0x8000;
        }
        sampqueue--;
        sampbufr++;
        sampbufr&=7;
        al_givebuffer(p2);
        #if 0
        while (!p)
        {
                p=(uint16_t *)get_audio_stream_buffer(as);
        }
        for (c=0;c</*((25000>>1))*/BUFFERSIZE<<1;c++) p[c]=p2[c];
        free_audio_stream_buffer(as);
        #endif
}

signed short convbyte(uint8_t v)
{//                         7C       chord = 3     p = E/14
        signed short temp=1<<((v>>5)+4);
        temp+=(((v>>1)&0xF)<<(v>>5));
        if (v&1) temp=-temp;
        return temp;
/*        uint8_t temp=(1<<(v>>5))-1;             //  temp=7
        if (v&0x80) temp+=(((v>>1)&0xF)<<((v>>5)-4));  // +14<<
        else        temp+=(((v>>1)&0xF)>>(4-(v>>5)));
        temp>>=1;
        if (v&1) temp=(temp^0xFF)+1;
        return temp;*/
}

signed short samples[8];
int samplecount=0,sampledelay=0;
//float lastsamp;

uint16_t mixsample()
{
        signed short temp2=0;
        uint16_t *temp3;
        signed short temp=samples[0];//((signed short)((signed char)convbyte(samples[0])))*8;
//        float tempf;
        temp3=&temp2;
        temp2+=temp;

        soundbuft[samppos++]=(int)fir((float)samples[2]);//temp;
        soundbuft[samppos++]=(int)fir2((float)samples[3]);

        if (samppos == (BUFFERSIZE << 3))
        {
                if (sampqueue==8)
                {
                        sampqueue--;
                        sampbuf--;
                        sampbuf&=7;
                }
                memcpy(soundbuf[sampbuf], soundbuft, BUFFERSIZE<<4);
//                if (!soundf) soundf=fopen("sound.pcm","wb");
//                fwrite(soundbuf[sampbuf],1250<<2,1,soundf);
                samppos=0;
                sampbuf++;
                sampbuf&=7;
                sampqueue++;
                mixsound();
//                wakeupsoundthread();
        }
        return *temp3;
}


int soundtime;
float sampdiff;
uint32_t spos,soend,sendN,sstart2;
int nextvalid;
#define getdmaaddr(addr) (((addr>>2)&0x7FFF)<<2)
void writememc(uint32_t a)
{
//        rpclog("Write MEMC %08X\n",a);
//        if (!slogfile) slogfile=fopen("slog.txt","wt");
/*        if (a&0x7080)
        {
                sprintf(s,"Write %08X %04X\n",a,a&0x7080);
                fputs(s,slogfile);
        }*/
        switch ((a>>17)&7)
        {
                case 0: /*printf("MEMC write %08X - VINIT  = %05X\n",a,getdmaaddr(a)*4);*/ vinit=getdmaaddr(a); /*rpclog("Vinit write %08X %07X\n",vinit,PC);*/ return;
                case 1: /*printf("MEMC write %08X - VSTART = %05X\n",a,getdmaaddr(a)*4);*/ vstart=getdmaaddr(a); /*rpclog("Vstart write %08X %07X\n",vstart,PC);*/return;
                case 2: /*printf("MEMC write %08X - VEND   = %05X\n",a,getdmaaddr(a)*4);*/ vend=getdmaaddr(a); /*rpclog("Vend write %08X %07X\n",vend,PC);*/return;
                case 3: /*printf("MEMC write %08X - CINIT  = %05X\n",a,getdmaaddr(a));*/ cinit=getdmaaddr(a); /*printf("CINIT=%05X\n",cinit<<2);*/ return;
                case 4:
//                rpclog("%08i MEMC write %08X - SSTART = %05X %05X\n",bigcyc,a,getdmaaddr(a),spos);
//                if (!logsound) return;
                sstart=getdmaaddr(a); /*printf("SSTART=%05X\n",sstart<<2);*/
                if (!nextvalid) nextvalid=1;
                if (nextvalid==2) nextvalid=0;
//                if (nextvalid==1)
//                {
//                soundtime=soundper*(ssend-sstart);
                ioc_irqbc(IOC_IRQB_SOUND_BUFFER);
                        nextvalid=2;
//                }
//                spos=sstart;
//                sinprog=1;
//                for (c=sstart;c<ssend;c++) mixsamp(ram[c]);
                return;
                case 5:
//                rpclog("%08i MEMC write %08X - SEND   = %05X %05X\n",bigcyc,a,getdmaaddr(a),spos);
                sendN=getdmaaddr(a);

                if (nextvalid==1) nextvalid=2;
                if (nextvalid!=2) nextvalid=1;
//                fputs(s,slogfile);
//                soend=ssend;
                return;
                case 6:
//                rpclog("%08i MEMC write %08X - SPTR   = %05X %05X\n",bigcyc,a,getdmaaddr(a),spos);
                sptr=getdmaaddr(a); /*printf("SPTR=%05X\n",sptr); */
//                fputs(s,slogfile);
//                if (!logsound) return;
//                soundtime=27500;
                spos=sstart2=sstart<<2;
                ssend=sendN<<2;
                ioc_irqb(IOC_IRQB_SOUND_BUFFER);
                nextvalid=0;
//                sinprog=1;
                return;
                case 7: osmode=(a&0x1000)?1:0; /*MEMC ctrl*/
                sdmaena=(a&0x800)?1:0;
                pagesize=(a&0xC)>>2;
                resetpagesize(pagesize);
                memc_videodma_enable = a & 0x400;
                switch ((a >> 6) & 3) /*High ROM speed*/
                {
                        case 0: /*450ns*/
                        mem_setromspeed(4, 4);
                        break;
                        case 1: /*325ns*/
                        mem_setromspeed(3, 3);
                        break;
                        case 2: /*200ns*/
                        mem_setromspeed(2, 2);
                        break;
                        case 3: /*200ns with 60ns nibble mode*/
                        mem_setromspeed(2, 1);
                        break;
                }
                memc_refreshon = (((a >> 8) & 3) == 1);
                mem_dorefresh = memc_refreshon && !vidc_displayon;
                return;

                rpclog("MEMC ctrl write %08X %i\n",a,sdmaena);
//                rpclog("CTRL write pagesize %i %i\n",pagesize,ins);
                memctrl=a;
                return;
        }
}

int vollevels[2][2][8]=
{
        {
                {0,4,4,4,4,4,4,4},
                {0,4,4,4,4,4,4,4}
        },
        {
                {0,6,5,4,3,2,1,0},
                {0,0,1,2,3,4,5,6}
        }
};

void pollsound()
{
        mixsample();
        if (!sdmaena) return;
        sampledelay += 4 << 10;
        if (sampledelay >= soundper)
        {
                sampledelay -= soundper;
                samples[0/*samplecount*/]=(ram[(spos>>2)&0x1FFFF]>>((spos&3)<<3))&0xFF;
                samples[0]=convbyte(samples[0]);
                samples[2]=(signed short)(((int)samples[0]*vollevels[stereo][0][stereoimages[spos&7]]));
                samples[3]=(signed short)(((int)samples[0]*vollevels[stereo][1][stereoimages[spos&7]]));
//                tempf=(float)lastsamp*((float)13/(float)16);
//                samples[0]+=tempf;
//                lastsamp=samples[0];
                spos++;
                if (spos==(ssend+16))
                {
                        if (nextvalid==2)
                        {
                                spos=sstart2=sstart<<2;
                                ssend=sendN<<2;
                                nextvalid=0;
                        }
                        else
                           spos=sstart2;
                        ioc_irqb(IOC_IRQB_SOUND_BUFFER);
                        sinprog=0;
                }
        }
}

int output;

void writecam(uint32_t a)
{
        int page,access,logical,c;
//        rpclog("Write CAM %08X pagesize %i %i\n",a,pagesize,ins);
        switch (pagesize)
        {
//                #if 0
                case 1: /*8k*/
                page=((a>>1)&0x3f) | ((a&1)<<6);
                access=(a>>8)&3;
                logical=(a>>13)&0x3FF;
                logical|=(a&0xC00);
//                rpclog("Map page %02X to %03X\n",page,logical);
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x1FFF)==(page<<13))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=1;
                for (c=0;c<2;c++)
                {
                        memcpages[logical+c]=page<<13;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<11)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<11)+(c<<10)];
                }
                break;
//                #endif
                case 2: /*16k*/
                page=((a>>2)&0x1f) | ((a&3)<<5);
                access=(a>>8)&3;
                logical=(a>>14)&0x1FF;
                logical|=(a>>1)&0x600;
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x3FFF)==(page<<14))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=2;
                for (c=0;c<4;c++)
                {
                        memcpages[logical+c]=page<<14;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<12)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<12)+(c<<10)];
                }
                break;
                case 3: /*32k*/
                page=((a>>3)&0xf) | ((a&1)<<4) | ((a&2)<<5) | ((a&4)<<3);
                if (a&0x80) page|=0x80;
                if (a&0x1000) page|=0x100;
                access=(a>>8)&3;
                logical=(a>>15)&0xFF;
                logical|=(a>>2)&0x300;
//                printf("Mapping %08X to %08X\n",0x2000000+(page*32768),logical<<15);
                for (c=0;c<0x2000;c++)
                {
                        if ((memcpages[c]&~0x7FFF)==(page<<15))
                        {
                                memcpages[c]=~0;
                                memstat[c]=0;
                        }
                }
                logical<<=3;
                for (c=0;c<8;c++)
                {
                        memcpages[logical+c]=page<<15;
                        memstat[logical+c]=access+1;
                        mempoint[logical+c]=&ram[(page<<13)+(c<<10)];
                        mempointb[logical+c]=(uint8_t *)&ram[(page<<13)+(c<<10)];
                }
                break;
        }
//        memcpermissions[logical]=access;
}

void initmemc()
{
        int c;
        for (c=0;c<0x2000;c++) memstat[c]=0;
}
