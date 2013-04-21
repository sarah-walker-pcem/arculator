/*Arculator 0.8 by Tom Walker
  IOC emulation*/
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "arc.h"

int motoron;
int irq;
int flyback,fdcready;
FILE *slogfile;
FILE *olog;
int i2cclock,i2cdata;
int keyway=0;
unsigned char tempkey,iockey,iockey2;
int keydelay=0,keydelay2;

void updateirqs()
{
        if (!fdctype || romset<2)
        {
                if (discchange[curdrive])
                {
                        ioc.irqb|=0x10;
//                        if (ioc.mskb&0x10) rpclog("Disc change interrupt\n");
                }
                else                      ioc.irqb&=~0x10;
        }
        if ((ioc.mska&ioc.irqa)||(ioc.mskb&ioc.irqb))   irq|=1;
        else                                            irq&=~1;
//        if (irq&1) rpclog("IRQ %02X %02X\n",(ioc.mska&ioc.irqa),(ioc.mskb&ioc.irqb));
}

void dumpiocregs()
{
        printf("IOC regs :\n");
        printf("STAT : %02X %02X %02X  MASK : %02X %02X %02X\n",ioc.irqa,ioc.irqb,ioc.fiq,ioc.mska,ioc.mskb,ioc.mskf);
}

char err2[256];
FILE *olog;

//int genpoll,genpol;
#define gencycdif 0
//#define gencycdif ((genpoll-genpol)<<1)
void writeioc(unsigned long addr, unsigned long v)
{
/*        if (addr>=0x3200080)
        {
                return;
                sprintf(err2,"Bad write %08X %08X\n",addr,v);
                MessageBox(NULL,err2,"Arc",MB_OK);
                dumpregs();
                exit(-1);
        }*/
//        rpclog("Write IOC %08X %08X\n",addr,v);
//        if (!olog) olog=fopen("olog.txt","wt");
        switch (addr&0x7C)
        {
                case 0x00: cmosi2cchange(v&2,v&1); ioc.ctrl=v&0xFC; return;
                case 0x04: ioc.irqb&=~0x40; updateirqs(); iockey2=v; keydelay=1536; keyway|=1; /*rpclog("Keywrite %02X\n",v); */return;
                case 0x14: ioc.irqa&=~v; updateirqs(); return;
                case 0x18: ioc.mska=v; updateirqs(); /*if (!olog) olog=fopen("armlog.txt","wt"); sprintf(err2,"MSKA %02X\n",ioc.mska); fputs(err2,olog);*/ return;
                case 0x24: /*printf("24 write %02X\n",v);*/ return; /*????*/
                case 0x28: ioc.mskb=v/*|(ioc.mskb&2)*/; updateirqs(); /*sprintf(err2,"MASKB now %02X %07X\n",v,PC); fputs(err2,olog);*/ return;
                case 0x34: /*sprintf(err2,"34 write %02X\n",v); fputs(err2,olog);*/ return; /*????*/
                case 0x38: /*printf(err2,"38 write %02X\n",v); fputs(err2,olog);*/ioc.mskf=v; iocfiq(0); return;
                case 0x40: ioc.timerl[0]=(ioc.timerl[0]&0x3FC00)|(v<<2); /*sprintf(err2,"T0l now %04X\n",ioc.timerl[0]>>2); fputs(err2,olog);*/ return;
                case 0x44: ioc.timerl[0]=(ioc.timerl[0]&0x3FC)|(v<<10); /*sprintf(err2,"T0h now %04X\n",ioc.timerl[0]>>2); fputs(err2,olog);*/ return;
                case 0x48:
                if (!speed)        ioc.timerc[0]=(ioc.timerl[0]+2)<<1;
                else if (speed==1) ioc.timerc[0]=(ioc.timerl[0]+2)*3;
                else if (speed==2) ioc.timerc[0]=(ioc.timerl[0]+2)*6;
                else               ioc.timerc[0]=(ioc.timerl[0]+2)<<3;
//                redogenpol(1);
                return;
                case 0x4C:
                if (!speed)        ioc.timerr[0]=(ioc.timerc[0]-gencycdif)>>3;
                else if (speed==1) ioc.timerr[0]=((ioc.timerc[0]-gencycdif)>>2)/3;
                else if (speed==2) ioc.timerr[0]=((ioc.timerc[0]-gencycdif)>>2)/6;
                else               ioc.timerr[0]=(ioc.timerc[0]-gencycdif)>>5;
//                rpclog("Latch timer %i %i %i %08X\n",ioc.timerr[0],ioc.timerc[0],gencycdif,PC);
                return;
                case 0x50: ioc.timerl[1]=(ioc.timerl[1]&0x3FC00)|(v<<2); /*sprintf(err2,"T1l now %04X %i %07X %i %i\n",ioc.timerl[1]>>2,getvidcline(),PC,getvidcwidth(),vidcgetcycs()); fputs(err2,olog); */return;
                case 0x54: ioc.timerl[1]=(ioc.timerl[1]&0x3FC)|(v<<10); /*sprintf(err2,"T1h now %04X %i %07X %i %i\n",ioc.timerl[1]>>2,getvidcline(),PC,getvidcwidth(),vidcgetcycs()); fputs(err2,olog); */return;
                case 0x58:
                if (!speed)        ioc.timerc[1]=(ioc.timerl[1]+2)<<1;
                else if (speed==1) ioc.timerc[1]=(ioc.timerl[1]+2)*3;
                else if (speed==2) ioc.timerc[1]=(ioc.timerl[1]+2)*6;
                else               ioc.timerc[1]=(ioc.timerl[1]+2)<<3;
//                redogenpol(1);
                return;
                case 0x5C:
                if (!speed)        ioc.timerr[1]=(ioc.timerc[1]-gencycdif)>>3;
                else if (speed==1) ioc.timerr[1]=((ioc.timerc[1]-gencycdif)>>2)/3;
                else if (speed==2) ioc.timerr[1]=((ioc.timerc[1]-gencycdif)>>2)/6;
                else               ioc.timerr[1]=(ioc.timerc[1]-gencycdif)>>5;
                return;
                case 0x60: ioc.timerl[2]=(ioc.timerl[2]&0xFF00)|v; return;
                case 0x64: ioc.timerl[2]=(ioc.timerl[2]&0xFF)|(v<<8); return;
                case 0x68: ioc.timerc[2]=ioc.timerl[2]; return;
                case 0x6C: ioc.timerr[2]=ioc.timerc[2]; return;
                case 0x70: ioc.timerl[3]=(ioc.timerl[3]&0xFF00)|v; return;
                case 0x74: ioc.timerl[3]=(ioc.timerl[3]&0xFF)|(v<<8); return;
                case 0x78: ioc.timerc[3]=ioc.timerl[3]; return;
                case 0x7C: ioc.timerr[3]=ioc.timerc[3]; return;
        }
/*        sprintf(err2,"Bad IOC write %07X %04X\n",addr,v);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

unsigned char readioc(unsigned long addr)
{
        unsigned char temp;
/*        if (addr>=0x3200080)
        {
                rpclog("Bad IOC read %08X\n",addr);
                return 0xFF;
//                sprintf(err2,"Bad read %08X\n",addr);
//                MessageBox(NULL,err2,"Arc",MB_OK);
//                dumpregs();
//                exit(-1);
        }*/
        addr&=0x7C;
//        rpclog("Read IOC %08X %08X %08X %08X %i\n",addr,PC,armregs[11],armregs[12],ioc.timerc[0]-gencycdif);
/*        if (!olog) olog=fopen("olog.txt","wt");
        if (addr==0x14 || addr==0x10 || addr==0x20 || addr==0x24)
        {
                sprintf(s,"Read %02X %07X\n",addr&0x7C,PC);
                fputs(s,olog);
        }*/
        switch (addr&0x7C)
        {
                case 0x00: rpclog("Read IOC control %02X\n", ((i2cclock)?2:0)|((i2cdata)?1:0)|fdcready|flyback); return ((i2cclock)?2:0)|((i2cdata)?1:0)|fdcready|flyback;
                case 0x04: ioc.irqb&=~0x80; updateirqs(); /*rpclog("Read IOCkey %02X\n",iockey);*/ return iockey;
                case 0x10: temp=ioc.irqa; ioc.irqa&=~0x10; return temp;
                case 0x14: return ioc.irqa&ioc.mska;
                case 0x18: return ioc.mska;
                case 0x20: return ioc.irqb;
                case 0x24: return ioc.irqb&ioc.mskb;
                case 0x28: return ioc.mskb;
                case 0x30: return ioc.fiq;
                case 0x34: return ioc.fiq&ioc.mskf;
                case 0x38: return ioc.mskf;
                case 0x40: return ioc.timerr[0];
                case 0x44: return ioc.timerr[0]>>8;
                case 0x50: return ioc.timerr[1];
                case 0x54: return ioc.timerr[1]>>8;
                case 0x60: return ioc.timerr[2];
                case 0x64: return ioc.timerr[2]>>8;
                case 0x70: return ioc.timerr[3];
                case 0x74: return ioc.timerr[3]>>8;
        }
        return 0;
/*        sprintf(err2,"Bad IOC read %07X\n",addr);
        MessageBox(NULL,err2,"Arc",MB_OK);
        dumpregs();
        exit(-1);*/
}

void iocfiq(unsigned char v)
{
        ioc.fiq|=v;
        if (ioc.fiq&ioc.mskf) irq|=2;
        else                  irq&=~2;
//        printf("FIQup %i %i\n",ioc.fiq,armfiq);
}

void iocfiqc(unsigned char v)
{
        ioc.fiq&=~v;
        if (ioc.fiq&ioc.mskf) irq|=2;
        else                  irq&=~2;
//        printf("FIQup %i %i\n",ioc.fiq,armfiq);
}

void updateioctimers()
{
        if (ioc.timerc[0]<0)
        {
                ioc.irqa|=0x20;
                if (!speed)        ioc.timerc[0]+=((ioc.timerl[0]-2)<<1);
                else if (speed==1) ioc.timerc[0]+=((ioc.timerl[0]-2)*3);
                else if (speed==2) ioc.timerc[0]+=((ioc.timerl[0]-2)*6);
                else               ioc.timerc[0]+=((ioc.timerl[0]-2)<<3);
                if (ioc.timerc[0]<0) ioc.timerc[0]=32767;
                updateirqs();
        }
        if (ioc.timerc[1]<0)
        {
                ioc.irqa|=0x40;
                if (!speed)        ioc.timerc[1]+=((ioc.timerl[1]-2)<<1);
                else if (speed==1) ioc.timerc[1]+=((ioc.timerl[1]-2)*3);
                else if (speed==2) ioc.timerc[1]+=((ioc.timerl[1]-2)*6);
                else               ioc.timerc[1]+=((ioc.timerl[1]-2)<<3);
                if (ioc.timerc[1]<0) ioc.timerc[1]=32767;
                updateirqs();
        }
}

void resetioc()
{
        ioc.irqa=ioc.mska=0;//x10;
        ioc.irqb=ioc.mskb=0;
        ioc.fiq= ioc.mskf=0;
//        ioc.irqa=0x10;
        ioc.irqb=2;
}

int disccint;
int lastdrive;
void discchangeint(int reload, int drive)
{
        discchange[drive]=1;
//        ioc.irqb|=0x10;
        updateirqs();
        if (reload) disccint=10000;
        lastdrive=drive;
        if (drive==curdrive) giveup1772();
}
void discchangecint()
{
        discchange[lastdrive]=0;
//        ioc.irqb&=~0x10;
        updateirqs();
        if (fdctype) motoron=25000;
}

void keycallback()
{
        if (keyway&1)
        {
//                printf("%02X sent to keyboard\n",iockey2);
                writekeyboard(iockey2);
                ioc.irqb|=0x40;
                updateirqs();
                keyway&=~1;
        }
}

void keycallback2()
{
//        rpclog("keycallback2 %i\n",keyway);
        if (keyway&2)
        {
                if (ioc.irqb&0x80)
                {
                        keydelay2+=256;
                        return;
                }
                iockey=tempkey;
                ioc.irqb|=0x80;
                updateirqs();
//                rpclog("KeyIRQ\n");
//                printf("Keyrx now %02X\n",iockey);
                keyway&=~2;
        }
}

void sendkey(unsigned char v)
{
//        rpclog("Send key %02X\n",v);
        tempkey=v;
        keydelay2=512;
        keyway|=2;
}

void initjoy()
{
        install_joystick(JOY_TYPE_AUTODETECT);
}

void polljoy()
{
        poll_joystick();
}

unsigned char readjoy(int addr)
{
        int c=(addr&4)?1:0;
        unsigned char temp=0x60;
        if (joy[c].stick[0].axis[1].d1) temp|=0x01;
        if (joy[c].stick[0].axis[1].d2) temp|=0x02;
        if (joy[c].stick[0].axis[0].d1) temp|=0x04;
        if (joy[c].stick[0].axis[0].d2) temp|=0x08;
        if (joy[c].button[0].b) temp|=0x10;
        return temp^0x1F;
}
