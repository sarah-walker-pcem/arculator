/*Arculator 0.8 by Tom Walker
  82c711 emulation
  Possibly a bug in the FDC write command*/
#include <windows.h>
#include <stdio.h>
#include "arc.h"

static int times = 0;
int ddensity;
void nodiscerrornew();
int fdcsectorsize;
int fdcupdatedata;
int fdcside;
void fdcsenddata(unsigned char val);
int fdiin[4];
int fdccallback,discchanged[4],motoron,timetolive;
int configmode=0;
unsigned char configregs[16];
int configreg;

struct
{
        unsigned char dor;
        int reset;
        unsigned char status;
        int incommand;
        unsigned char command;
        unsigned char st0,st1,st2,st3;
        int commandpos;
        int track,sector,side;
        unsigned char data;
        int params,curparam;
        unsigned char parameters[10];
        unsigned char dmadat;
        int rate;
        int oldpos;
        unsigned char size,sidex;
} fdc;

//unsigned char disc[2][2][80][10][1024];
int discdensity[4];
int discsectors[4];
//int discchanged[2];

unsigned char scratch,linectrl;
void reset82c711()
{
        configregs[0xA]=0;
        configregs[0xD]=0x65;
        configregs[0xE]=2;
        configregs[0xF]=0;
        fdccallback=0;
}

int databytes;
int fdclast;
void readdata82c711(unsigned char dat, int last)
{
        fdclast=last;
//        rpclog("Callback 82c711 read %02X %i %i\n",dat,last,databytes);
        if (last)
        {
                if (!fdcupdatedata)
                {
//                        rpclog("Other end\n");
                        inreadop=0;
                        fdc.commandpos=1024;
                        fdccallback=300;
                        fdc.st0=0;
                        fdc.sector++;
                        return;
                }
                fdc.sector++;
                if (fdc.sector<=fdc.parameters[5])
                {
                        fdc.commandpos=0;
                        fdireadsector(fdc.sector,fdc.track,readdata82c711);
//                        rpclog("Reading next sector\n");
                }
                else
                {
//                        fdccallback=100>>1;
//                        fdc.commandpos=1024;
//                        rpclog("Last one!\n");
//                        return;
                }
        }
        if (!fdcupdatedata)
           return;
        databytes++;
        fdcsenddata(dat);
}

void writefdc(unsigned long addr, unsigned long val)
{
        if (configmode==2)
        {
                if (((addr>>2)&0x3FF)==0x3F0)
                {
                        configreg=val&15;
//                        printf("Register CR%01X selected\n",configreg);
                        return;
                }
                else
                {
                        configregs[configreg]=val;
//                        printf("CR%01X = %02X\n",configreg,val);
                        return;
                }
        }
        /*if (((addr>>2)&0x3FF)!=0x3F5) */rpclog("FDC write %03X %08X %08X %08X\n",(addr>>2)&0x3FF,addr,val,PC);
        switch ((addr>>2)&0x3FF)
        {
                case 0x3F2: /*DOR*/
                if (val == 0x0C && PC == 0x38E4024)// && fdc.st0 == 0x80 && fdc.rate == 2)
                {
                        times++;
                        if (times == 4)
                           output = 1;
                }

                if ((val&4) && !(fdc.dor&4)) /*Reset*/
                {
                        fdc.reset=1;
                        fdccallback=400;
                        rpclog("Start FDC reset\n");
                }
                if (!(val&4)) fdc.status=0x80;
                motoron=(val&0xF0)?1:0;
                fdc.dor=val;
                break;
                case 0x3F4:
//                rpclog("3F4 write %02X %07X\n",val,PC);
                break;
                case 0x3F5: /*Command*/
                fdcupdatedata=1;
                inreadop=readidcommand=0;
//                output=0;
//                timetolive=50;
                if (fdc.params)
                {
                        if (!fdc.curparam && fdc.command!=3) curdrive=val&1;
                        fdc.parameters[fdc.curparam++]=val;
//                        printf("Param : %i %i %02X\n",fdc.curparam,fdc.params,val);
                        if (fdc.curparam==fdc.params)
                        {
                                fdc.status&=~0x80;
                                switch (fdc.command)
                                {
                                        case 3: /*Specify*/
                                        fdccallback=100;
                                        break;
                                        case 4: /*Sense drive status*/
                                        fdccallback=100;
                                        fdc.st0=0;
//                                        times++;
//                                        if (times == 7)
//                                           output = 1;
                                        break;
                                        case 7: /*Recalibrate*/
//                                        printf("Recalibrate starting\n");
                                        case 0x0F: /*Seek*/
                                        if (fastdisc) fdccallback=100;
                                        else          fdccallback=1000;
                                        fdc.status|=1;
                                        break;
                                        case 0x13: /*Configure*/
                                        fdccallback=100;
                                        break;
                                        case 0x45: /*Write data - MFM*/
                                        fdc.commandpos=0;
                                        fdccallback=800;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        fdc.track=fdc.parameters[1];
                                        fdc.side=fdc.parameters[2];
                                        fdc.sector=fdc.parameters[3];
//                                        rpclog("Write data %i %i %i\n",fdc.side,fdc.track,fdc.sector);
                                        readflash[fdc.st0&1]=8;
                                        discchanged[fdc.st0&1]=1;
                                        break;
                                        case 0x06: /*Read data - FM*/
                                        case 0x46: /*Read data - MFM*/
                                        fdc.commandpos=0;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        fdc.track=fdc.parameters[1];
                                        fdc.side=fdc.parameters[2];
                                        fdc.sector=fdc.parameters[3];
                                        rpclog("Read sector %i %i %i\n", fdc.track, fdc.side, fdc.sector);
                                        if (output)
                                        {
                                                dumpregs();
                                                exit(-1);
                                        }
                                        if (!(discname[fdc.st0&1][0]))
                                        {
                                                nodiscerrornew();
                                                return;
                                        }
                                        ddensity=(fdc.command&0x40);
                                        fdc.command=0x46;
                                        if (fdiin[fdc.st0&1])
                                        {
                                                fdireadsector(fdc.sector,fdc.track,readdata82c711);
                                                fdcside=fdc.side;
                                        }
                                        else
                                        {
                                                if (fastdisc) fdccallback=100;
                                                else          fdccallback=800;
                                        }
                                        readflash[fdc.st0&1]=1;
                                        fdcsectorsize=3;
                                        fdcupdatedata=1;
                                        fdclast=0;
                                        break;
                                        case 0x4A: /*Read ID - MFM*/
                                        ddensity=1;
                                        fdc.commandpos=0;
                                        fdccallback=800;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        rpclog("Read ID %i\n",fdc.st0&1);
                                        if (!(discname[fdc.st0&1][0]))
                                        {
                                                rpclog("No disc error new!\n");
                                                nodiscerrornew();
                                                return;
                                        }
                                        if (fdiin[fdc.st0&1])
                                        {
                                                inreadop=1;
                                                readidcommand=1;
                                                fdccallback=0;
                                        }
                                        if (fdc.rate!=discdensity[fdc.st0&1]) { fdc.command=0xA; rpclog("Bad ID\n"); }
                                        break;
                                        case 0x0A: /*Read ID - FM*/
                                        ddensity=0;
                                        fdc.commandpos=0;
                                        fdccallback=800;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        if (!(discname[fdc.st0&1][0]))
                                        {
                                                nodiscerrornew();
                                                return;
                                        }
                                        break;
                                }
                        }
                        return;
                }
                if (fdc.incommand)
                {
                        printf("FDC already in command\n");
                        dumpregs();
                        exit(-1);
                }
                fdc.incommand=1;
                fdc.commandpos=0;
                fdc.command=val;
                rpclog("Command write %02X %i : rate %i\n",val,ins,fdc.rate);
//                printf("Rate %i %i\n",discdensity[0],fdc.rate);
                switch (fdc.command)
                {
                        case 3: /*Specify*/
                        fdc.params=2;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 4: /*Sense drive status*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 7: /*Recalibrate*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 8: /*Sense interrupt status*/
                        fdccallback=100;
                        fdc.status=0x10;
                        break;

                        case 0x0F: /*Seek*/
                        fdc.params=2;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x13: /*Configure*/
                        fdc.params=3;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x45: /*Write data - MFM*/
                        case 0x46: /*Read data - MFM*/
                        case 0x06: /*Read data - FM*/
                        fdc.params=8;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x0A: /*Read ID - FM*/
                        case 0x4A: /*Read ID - MFM*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        default:
                        error("Bad 82c711 FDC command %02X\n",val);
                        dumpregs();
                        exit(-1);
                }
                break;
                case 0x3F7:
//                output=0;
//                rpclog("3F7 write %02X %07X\n",val,PC);
                fdc.rate=val&3;
                hdensity=!(val&3);
        }
}

int configindex;
unsigned char configregs[16];

void write82c711(unsigned long addr, unsigned long val)
{
        unsigned long addr2;
        addr2=(addr>>2)&0x3FF;
//        rpclog("Write 82c711 %08X %03X %08X %07X %i\n",addr,addr2,val,PC,configmode);
        if (configmode!=2)
        {
                if ((addr2==0x3F0) && (val==0x55))
                {
                        configmode++;
                        return;
                }
                else
                   configmode=0;
        }
        if (configmode==2 && addr2==0x3F0 && val==0xAA)
        {
                configmode=0;
//                printf("Cleared config mode\n");
                return;
        }
        if (configmode==2 && addr2==0x3F0)
        {
                configindex=val;
                return;
        }
        if (configmode==2 && addr2==0x3F1)
        {
                configregs[configindex&15]=val;
                return;
        }
//        printf("Write 82c711 %08X %08X %03X %07X %08X\n",addr,val,addr2,PC,armregs[12]);
        if ((addr2>=0x3F0) && (addr2<=0x3F7)) writefdc(addr,val);
        if ((addr2==0x27A) && (val&0x10))
        {
//                printf("Printer interrupt %02X\n",iomd.maska);
//                iomd.stata|=1;
//                updateirqs();
        }
        if ((addr2==0x3F9) && (val&2))
        {
//                printf("Serial transmit empty interrupt\n");
//                iomd.fiq|=0x10;
//                updateirqs();
        }
        if (addr2==0x3FB) linectrl=val;
        if (addr2==0x3FE) scratch=val;
        if ((addr2>=0x1F0 && addr2<=0x1F7) || addr2==0x3F6)
        {
                writeide(addr2,val);
                return;
        }
}

unsigned char readfdc(unsigned long addr)
{
        if (configmode==2 && ((addr>>2)&0x3FF)==0x3F1)
        {
//                printf("Read CR%01X %02X\n",configreg,configregs[configreg]);
                return configregs[configreg];
        }
        rpclog("FDC read %03X %08X %08X  ",(addr>>2)&0x3FF,addr,PC);
        switch ((addr>>2)&0x3FF)
        {
                case 0x3F4:
                ioc.irqb&=~0x10;
                updateirqs();
                rpclog("%02X\n", fdc.status);
//                rpclog("Status : %02X %07X\n",fdc.status,PC);
                return fdc.status;
                case 0x3F5: /*Data*/
/*                if (fdc.command==4)
                {
                        timetolive=400;
                }*/
                fdc.status&=0x7F;
                if (!fdc.incommand) fdc.status=0x80;
                else                fdccallback=50;
//                printf("Read FDC data %02X\n",fdc.data);
                rpclog("%02X\n", fdc.data);
                return fdc.data;
//                case 0x3F7: return 0x80;
        }
        rpclog("\n");
        return 0;
}

void fdcsend(unsigned char val)
{
//        printf("New FDC data %02X %02X %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos);
        fdc.data=val;
        fdc.status=0xD0;
        ioc.irqb|=0x10;
        updateirqs();
}

void fdcsend2(unsigned char val)
{
//        printf("NO INT - New FDC data %02X %02X %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos);
        fdc.data=val;
        fdc.status=0xD0;
}

void fdcsenddata(unsigned char val)
{
//        printf("New FDC DMA data %02X %02X %i %i  %i %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos,fdc.side,fdc.track,fdc.sector);
        fdc.dmadat=val;
        iocfiq(1);
//        timetolive=50;
}

unsigned char read82c711(unsigned long addr)
{
        unsigned long addr2;
        addr2=(addr>>2)&0x3FF;
//        rpclog("Read 82c711 %08X %03X %08X\n",addr,addr2,PC);
        if (configmode==2 && addr2==0x3F1)
        {
//                rpclog("Read config %i - %02X\n",configindex,configregs[configindex&15]);
                return configregs[configindex&15];
        }
        
        if (addr2==0x3FF) return 0x55;
//        printf("Read 82c711 %08X %03X %08X\n",addr,addr2,PC);
        if ((addr2>=0x1F0 && addr2<=0x1F7) || addr2==0x3F6)
        {
                return readide(addr2);
        }
        if ((addr2>=0x3F0) && (addr2<=0x3F7)) return readfdc(addr);
        if (addr2==0x3FA)
        {
                iocfiqc(0x10);
                return 2;
        }
        if (addr2==0x3FB) return linectrl;
        if (addr2==0x3FE) return scratch;
/*        if (addr2==0x3F6)
        {
                ide.atastat+=0x40;
                return ide.atastat&0xC0;
        }*/
        return 0;
}

void callbackfdc()
{
        if (fdc.reset)
        {
//                rpclog("FDC reset\n");
                ioc.irqb|=0x10;
                updateirqs();
                fdc.reset=0;
                fdc.status=0x80;
                fdc.incommand=0;
                fdc.st0=0xC0;
                fdc.track=0;
                fdc.curparam=fdc.params=0;
                fdc.rate=2;
                return;
        }
//        rpclog("Callback command %02X %i\n",fdc.command,fdc.commandpos);
        switch (fdc.command)
        {
                case 3:
//                printf("Specify : %02X %02X\n",fdc.parameters[0],fdc.parameters[1]);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                break;
                case 4: /*Sense drive status*/
                fdc.st3=(fdc.parameters[0]&7)|0x28;
                if (!fdc.track) fdc.st3|=0x10;
                fdc.incommand=0;
                rpclog("Send ST3 %02X %02X\n",fdc.st3,fdc.parameters[0]);
//                timetolive=150;
                fdcsend(fdc.st3);
                fdc.params=fdc.curparam=0;
                break;
                case 7: /*Recalibrate*/
                fdc.track=0;
                if (fdiin[fdc.st0&1])
                   fdiseek(fdc.track);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                ioc.irqb|=0x10;
                updateirqs();
                fdc.st0=0x20;
//                printf("Recalibrate complete\n");
                break;
                case 8:
                fdc.commandpos++;
                if (fdc.commandpos==1)
                {
//                        printf("Send ST0\n");
                        fdcsend(fdc.st0);
                        fdccallback=100;
                }
                else
                {
//                        printf("Send track\n");
                        fdc.incommand=0;
                        fdcsend(fdc.track);
                }
                break;
                case 0x0F: /*Seek*/
//                printf("Seek to %i\n",fdc.parameters[1]);
                fdc.track=fdc.parameters[1];
                if (fdiin[fdc.st0&1])
                   fdiseek(fdc.track);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                ioc.irqb|=0x10;
                updateirqs();
                fdc.st0=0x20;
                break;
                case 0x13:
//                printf("Configure : %02X %02X %02X\n",fdc.parameters[0],fdc.parameters[1],fdc.parameters[2]);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                break;
                case 0x45: /*Write data - MFM*/
                if (fdc.commandpos==2048)
                {
                        disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.oldpos-1]=fdc.dmadat;
//                        rpclog("Write %i %02i %i %03X %02X\n",fdc.side,fdc.track,fdc.sector,fdc.oldpos-1,fdc.dmadat);
//                        rpclog("Operation terminated\n");
                        fdc.commandpos=1025;
                        fdccallback=300;
                        fdc.sector++;
                }
                else if (fdc.commandpos>=1025)
                {
//                        printf("Sending result\n");
//                        fdccallback=50;
                        switch (fdc.commandpos-1025)
                        {
                                case 0: fdcsend(fdc.st0); break;
                                case 1: fdcsend2(fdc.st1); break;
                                case 2: fdcsend2(fdc.st2); break;
                                case 3: fdcsend2(fdc.track); break;
                                case 4: fdcsend2((fdc.parameters[0]&4)?1:0); break;
                                case 5: fdcsend2(fdc.sector); break;
                                case 6:
                                fdcsend2(3);
                                fdc.incommand=0;
                                fdc.params=fdc.curparam=0;
                                fdccallback=0;
//                                printf("Ins %i\n",ins);
                                break;
                        }
                        fdc.commandpos++;
                }
                else
                {
                        if (fdc.commandpos)
                        {
                                disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.commandpos-1]=fdc.dmadat;
//                                rpclog("Write %i %02i %i %03X %02X\n",fdc.side,fdc.track,fdc.sector,fdc.commandpos-1,fdc.dmadat);
                        }
                        fdc.commandpos++;
                        if (fdc.commandpos==1025)
                        {
//                                rpclog("End of sector\n");
                                fdc.sector++;
                                if (fdc.sector<=fdc.parameters[5])
                                {
//                                        printf("Continuing to next sector\n");
                                        fdc.commandpos=0;
                                        fdccallback=200;
                                        return;
                                }
                                else
                                {
                                        fdccallback=200;
                                        return;
                                }
                        }
                        else
                        {
//                                printf("FIQ\n");
                                iocfiq(1);
                        }
                        fdccallback=0;
                }
                break;
                case 0x46: /*Read data - MFM*/
//                rpclog("Read data callback %i\n",fdc.commandpos);
                if (fdc.commandpos>=1024)
                {
//                        output=1;
//                        if (fdc.commandpos==1024) rpclog("Finish data %02X %02X %02X %02X %02X %02X\n",fdc.st0,fdc.st1,fdc.st2,fdc.track,(fdc.parameters[0]&4)?1:0,fdc.sector,3);
//                        printf("sending result %i\n",fdc.commandpos-1024);
//                        timetolive=500;
//                        fdccallback=20;
/*                        if (fdc.commandpos==1024)
                        {
                                rpclog("FDC results : %02X %02X %02X %02X %02X %02X %02X\n",fdc.st0,fdc.st1,fdc.st2,fdc.track,(fdc.parameters[0]&4)?1:0,fdc.sector,3);
                        }*/
                        switch (fdc.commandpos-1024)
                        {
                                case 0: fdcsend(fdc.st0); break;
                                case 1: fdcsend2(fdc.st1); break;
                                case 2: fdcsend2(fdc.st2); break;
                                case 3: fdcsend2(fdc.track); break;
                                case 4: fdcsend2((fdc.parameters[0]&4)?1:0); break;
                                case 5: fdcsend2(fdc.sector); break;
                                case 6:
//                                        rpclog("Real end of command!\n");
//                                        output=1;
                                fdcsend2(fdcsectorsize);
                                fdc.incommand=0;
                                fdc.params=fdc.curparam=0;
                                fdccallback=0;
//                                printf("Ins %i\n",ins);
                                break;
                        }
                        fdc.commandpos++;
                }
                else
                {
                        fdclast=0;
//                        printf("sending data\n");
                        if (fdcupdatedata) fdcsenddata(disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.commandpos]);
                        else               fdccallback=300;
                        fdc.commandpos++;
                        if (fdc.commandpos==1024)
                        {
                                fdclast=1;
//                                rpclog("Finished sector %i - target %i\n",fdc.sector,fdc.parameters[5]);
                                fdc.sector++;
                                if (fdc.sector<=fdc.parameters[5])
                                   fdc.commandpos=0;
/*                                else
                                {
                                        rpclog("End of read op\n");
                                }*/
/*                                else
                                {
//                                        printf("End of read op\n");
                                        fdc.sector=1;
                                }*/
                        }
                        if (fdcupdatedata) fdccallback=0;
                }
                break;
                case 0x4A: /*Read ID - MFM*/
                if (fdc.sector>=discsectors[fdc.st0&1] && !fdiin[fdc.st0&1]) fdc.sector=0;
                switch (fdc.commandpos)
                {
                        case 0: fdcsend(fdc.st0); break;
                        case 1: fdcsend2(fdc.st1); break;
                        case 2: fdcsend2(fdc.st2); break;
                        case 3: fdcsend2(fdc.track); break;
                        case 4: if (fdiin[fdc.st0&1]) fdcsend2(fdc.sidex); else fdcsend2((fdc.parameters[0]&4)?1:0); break;
                        case 5: fdcsend2(fdc.sector); break;
                        case 6: if (fdiin[fdc.st0&1]) fdcsend2(fdc.size); else fdcsend2(3); break;
                        default:
                        printf("Bad ReadID command pos %i\n",fdc.commandpos);
                        exit(-1);
                }
                fdc.commandpos++;
                if (fdc.commandpos==7)
                {
                        rpclog("Read ID sector %i\n",fdc.sector);
//                        printf("Sector %i : maxsector %i density %i\n",fdc.sector,discsectors[0],discdensity[0]);
                        fdc.incommand=0;
//                        printf("Read ID for track %i sector %i\n",fdc.track,fdc.sector);
                        if (!fdiin[fdc.st0&1])
                        {
                                fdc.sector++;
                                if (fdc.sector>=discsectors[fdc.st0&1]) fdc.sector=0;
                        }
                        fdc.params=fdc.curparam=0;
                }
//                else
//                   fdccallback=50;
                break;
                case 0x0A:
                ioc.irqb|=0x10;
                updateirqs();
                fdc.st0=0x40|(fdc.parameters[0]&7);
                fdc.st1=1;
                fdc.st2=1;
                fdc.incommand=0;
                fdc.params=fdc.curparam=0;
                break;
        }
}

int dmacount=0;
unsigned char readfdcdma(unsigned long addr)
{
        unsigned char temp;
//        rpclog("Read FDC DMA %08X %02X %i\n",addr,fdc.dmadat,fdc.commandpos);
/*        dmacount++;
        if (dmacount==12)
        {
                dumpregs();
                exit(-1);
        }*/
        iocfiqc(1);
        if (!fdiin[fdc.st0&1])
        {
                fdccallback=50;
                if (!fdc.commandpos) fdccallback=300;
        }
        temp=fdc.dmadat;
        if (addr==0x302A000)
        {
                if (/*inreadop && */!fdclast)
                {
                        fdcupdatedata=0;
//                        rpclog("Early end of DMA\n");
                        return temp;
                }
//                if (!fdc.commandpos)
//                   fdc.sector--;
                inreadop=0;
                fdc.commandpos=1024;
//                rpclog("End of DMA\n");
                fdccallback=300;
                fdc.st0=0;
        }
        else if (fastdisc && fdc.command==0x46 && !fdiin[fdc.st0&1])
        {
                fdccallback=0;
                callbackfdc();
        }
        return temp;
}

void writefdcdma(unsigned long addr, unsigned char val)
{
        iocfiqc(1);
        fdccallback=200;
        if (!fdc.commandpos) fdccallback=400;
        if (addr==0x302A000)
        {
//                printf("DMA terminated\n");
                fdc.oldpos=fdc.commandpos;
                fdc.commandpos=2048;
                fdccallback=300;
                fdc.st0=0;
        }
        fdc.dmadat=val;
//        printf("Write DMA dat %02X %08X\n",val,addr);
}

void sectornotfoundnew()
{
//        rpclog("Sector not found\n");
        ioc.irqb|=0x10;
        updateirqs();
        fdc.st0=0x40|(fdc.parameters[0]&7);
        fdc.st1=4; /*No data*/
        fdc.st2=0;
        fdc.incommand=0;
        fdc.params=fdc.curparam=0;
}

void headercrcerrornew()
{
//        rpclog("Header CRC error\n");
        ioc.irqb|=0x10;
        updateirqs();
        fdc.st0=0x40|(fdc.parameters[0]&7);
        fdc.st1=0x20; /*Data error*/
        fdc.st2=0;
        fdc.incommand=0;
        fdc.params=fdc.curparam=0;
}

void datacrcerrornew()
{
//        rpclog("Data CRC error\n");
        ioc.irqb|=0x10;
        updateirqs();
        fdc.st0=0x40|(fdc.parameters[0]&7);
        fdc.st1=0x20; /*Data error*/
        fdc.st2=0x20; /*Data error in data field*/
        fdc.incommand=0;
        fdc.params=fdc.curparam=0;
}

void nodiscerrornew()
{
//        rpclog("No disc\n");
        ioc.irqb|=0x10;
        updateirqs();
        fdc.st0=0xC0|(fdc.parameters[0]&7);
        fdc.st1=0x10; /*Data error*/
        fdc.st2=0x00; /*Data error in data field*/
        fdc.incommand=0;
        fdc.params=fdc.curparam=0;
}

void init82c711()
{
        fdiinit(datacrcerrornew,headercrcerrornew,sectornotfoundnew);
}

void readidresult(unsigned char *dat, int badcrc)
{
        readidcommand=0;
        fdccallback=10;
        fdc.track=dat[0];
        fdc.sidex=dat[1];
        fdc.sector=dat[2];
        fdc.size=dat[3];
        
        rpclog("ID result! %02X %02X %02X %02X\n",fdc.track,fdc.sidex,fdc.sector,fdc.size);
        
        if (badcrc)
        {
                fdc.st0|=0x40;
                fdc.st1|=0x20;
        }
        
        inreadop=0;
}
