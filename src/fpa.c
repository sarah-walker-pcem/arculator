/*RPCemu v0.6 by Tom Walker
  FPA emulation
  Not enabled by default due to bugs*/

#include <math.h>
#include "arc.h"

int cycles;
int timetolive;

//#define UNDEFINED  11
//#define undefined() exception(UNDEFINED,8,4)
double fparegs[8] = {0.0}; /*No C variable type for 80-bit floating point, so use 64*/
uint32_t fpsr = 0, fpcr = 0;

void dumpfpa(void)
{
        rpclog("F0=%f F1=%f F2=%f F3=%f ",fparegs[0],fparegs[1],fparegs[2],fparegs[3]);
        rpclog("F4=%f F5=%f F6=%f F7=%f\n",fparegs[4],fparegs[5],fparegs[6],fparegs[7]);
//        rpclog("FPSR=%08X FPCR=%08X\n",fpsr,fpcr);
}

void resetfpa()
{
        uint32_t temp[3];
        float *tfs;
        double tf;
        tfs=(float *)temp;
        *tfs=0.12f;
        tf=(double)(*tfs);
        rpclog("Double size %i Float size %i %f %f\n",sizeof(double),sizeof(float),*tfs,tf);
//        fpsr=0;
        fpsr=0x81000000; /*FPA system*/
        fpcr=0;
        atexit(dumpfpa);
}

#define FD ((opcode>>12)&7)
#define FN ((opcode>>16)&7)
#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define GETADDR(r) ((r==15)?(armregs[15]&0x3FFFFFC):armregs[r])

#define NFLAG 0x80000000
#define ZFLAG 0x40000000
#define CFLAG 0x20000000
#define VFLAG 0x10000000

void setsubf(double op1, double op2)
{
        armregs[15]&=0xFFFFFFF;
        if (op1==op2) armregs[15]|=ZFLAG;
        if (op1< op2) armregs[15]|=NFLAG;
        if (op1>=op2) armregs[15]|=CFLAG;
//        if ((op1^op2)&(op1^res)&0x80000000) armregs[cpsr]|=VFLAG;
}
int times8000;
double fconstants[8]={0.0,1.0,2.0,3.0,4.0,5.0,0.5,10.0};

volatile union
{
        float f;
        uint32_t i;
} f32;
volatile union
{
        double f;
        struct
        {
                uint32_t l,h;
        } i;
} f64;


double convert80to64(uint32_t *temp)
{
        int tempi,len;
        if ((temp[0] & ~0xffff8000) == 0x00007fff && !temp[1] && !temp[2]) /*Infinity*/
        {
                f64.i.l = 0;
                f64.i.h = 0x7ff00000 | (temp[0] & 0x80000000);
                return f64.f;
        }
        
        f64.i.l=temp[2]>>11;
        f64.i.l|=(temp[1]<<21);
        f64.i.h=(temp[1]&~0x80000000)>>11;
        tempi=(temp[0]&0x7FFF)-16383;
        len=((tempi>0)?tempi:-tempi)&0x3FF;
        tempi=((tempi>0)?len:-len)+1023;
        f64.i.h|=(tempi<<20);
        f64.i.h|=(temp[0]&0x80000000);
        return f64.f;
}

int __x,__y;
void convert64to80(uint32_t *temp, double tf)
{
        int tempi,len;
        f64.f=tf;
        __x=f64.i.h;
        __y=f64.i.l;
        
        if ((f64.i.h & ~0x80000000) == 0x7ff00000 && !f64.i.l)
        {
                temp[0] = (f64.i.h & 0x80000000) | 0x7fff;
                temp[1] = temp[2] = 0;
                return;
        }
//        double *tf2=(double *)&temp[4];
//        *tf2=tf;
        temp[0]=f64.i.h&0x80000000;
        tempi=((f64.i.h>>20)&0x7FF)-1023+16383;
        temp[0]|=(tempi&0x7FFF);
        temp[1]=(f64.i.h&0xFFFFF)<<11;
        temp[1]|=((f64.i.l>>21)&0x7FF);
        temp[2]=f64.i.l<<11;
        if (temp[0]&0x7FFF) temp[1]|=0x80000000;
//        rpclog("64 %08X %08X  ",f64.i.l,f64.i.h);
//        rpclog(" %08X %08X %08X\n",temp[0],temp[1],temp[2]);
}

#define undeffpa                fpcr|= 0x00000800; \
                                fpcr&=~0x00FFF07F; \
                                fpcr|=(opcode&0x00FFF07F); \
                                return 1;


int64_t fpa_round(double x, uint32_t opcode)
{
        switch (opcode&0x60)
        {
                case 0x00: /*Nearest*/
                return (int64_t)(x+0.5);
                case 0x20: /*+inf*/
                return (int64_t)ceil(x);
                case 0x40: /*-inf*/
                return (int64_t)floor(x);
                case 0x60: /*Zero*/
                return (int64_t)x;
        }
}

static char fpa_temps[10];

double fpa_dis_getrmval(int rm)
{
        if (rm & 8)
        {
                switch (rm)
                {
                        case 0x8: return 0.0;
                        case 0x9: return 1.0;
                        case 0xa: return 2.0;
                        case 0xb: return 3.0;
                        case 0xc: return 4.0;
                        case 0xd: return 5.0;
                        case 0xe: return 0.5;
                        case 0xf: return 10.0;
                }
        }
        
        return fparegs[rm & 7];
}

char *fpa_dis_getrm(int rm)
{
        if (rm & 8)
        {
                switch (rm)
                {
                        case 0x8: sprintf(fpa_temps, "0.0"); break;
                        case 0x9: sprintf(fpa_temps, "1.0"); break;
                        case 0xa: sprintf(fpa_temps, "2.0"); break;
                        case 0xb: sprintf(fpa_temps, "3.0"); break;
                        case 0xc: sprintf(fpa_temps, "4.0"); break;
                        case 0xd: sprintf(fpa_temps, "5.0"); break;
                        case 0xe: sprintf(fpa_temps, "0.5"); break;
                        case 0xf: sprintf(fpa_temps, "10.0"); break;
                }
        }
        else
           sprintf(fpa_temps, "F%i", rm & 7);
        
        return fpa_temps;
}

void fpa_dasm(uint32_t opcode)
{
        int c;
        switch ((opcode>>24) & 0xf)
        {
                case 0xc: case 0xd:
                if (opcode & 0x100) /*LDF/STF*/
                {
                        if (((opcode >> 8) & 0xf) != 1)
                           rpclog("Malformed ");
                        if (opcode & 0x100000)
                           rpclog("LDF");
                        else
                           rpclog("STF");
                        switch (opcode & 0x408000)
                        {
                                case 0x000000: rpclog("S "); break;
                                case 0x008000: rpclog("D "); break;
                                case 0x400000: rpclog("L "); break;
                                case 0x408000: rpclog("P "); break;
                        }
                        
                        rpclog("F%i, ", FD);
                        
                        if (opcode & (1 << 24))
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i, #0x%02X]", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i, #-0x%02X]", RN, (opcode & 0xff) << 2);                        
                                if (opcode & (1 << 21))
                                   rpclog("!");
                        }
                        else
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i], #0x%02X", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i], #-0x%02X", RN, (opcode & 0xff) << 2);
                        }
                        if (!(opcode & 0x100000))
                           rpclog("(%f)", fparegs[FD]);
                        rpclog("\n");
                }
                else if (opcode&0x100000) /*LFM*/
                {
                        if (((opcode >> 8) & 0xf) != 2)
                           rpclog("Malformed ");
                        c = 0;
                        if (opcode &   0x8000) c++;
                        if (opcode & 0x400000) c += 2;
                        rpclog("LFM  F%i, %i, ", FD, c);
                        
                        if (opcode & (1 << 24))
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i, #0x%02X]", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i, #-0x%02X]", RN, (opcode & 0xff) << 2);                        
                                if (opcode & (1 << 21))
                                   rpclog("!");
                        }
                        else
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i], #0x%02X", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i], #-0x%02X", RN, (opcode & 0xff) << 2);
                        }
                        rpclog("\n");
                }
                else /*SFM*/
                {
                        if (((opcode >> 8) & 0xf) != 2)
                           rpclog("Malformed ");
                        c = 0;
                        if (opcode &   0x8000) c++;
                        if (opcode & 0x400000) c += 2;
                        rpclog("SFM  F%i, %i, ", FD, c);
                        
                        if (opcode & (1 << 24))
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i, #0x%02X]", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i, #-0x%02X]", RN, (opcode & 0xff) << 2);                        
                                if (opcode & (1 << 21))
                                   rpclog("!");
                        }
                        else
                        {
                                if (opcode&0x800000)
                                   rpclog("[R%i], #0x%02X", RN, (opcode & 0xff) << 2);
                                else
                                   rpclog("[R%i], #-0x%02X", RN, (opcode & 0xff) << 2);
                        }
                        rpclog("\n");
                }
                break;
                
                case 0xe: 
                if (opcode & 0x100)
                {
                        if (((opcode >> 8) & 0xf) != 1)
                           rpclog("Malformed ");                           
                        if (opcode & 0x10)
                        {
                                if (RD == 15 && opcode & (1 << 20))
                                {
                                        switch ((opcode >> 21) & 0x7)
                                        {
                                                case 4: rpclog("CMF F%i, %s  (%f, %f)\n", FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 5: rpclog("CMF F%i, %s  (%f, %f)\n", FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 6: rpclog("CMF F%i, %s  (%f, %f)\n", FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 7: rpclog("CMF F%i, %s  (%f, %f)\n", FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                default: rpclog("UND\n"); break;
                                        }
                                }
                                else
                                {
                                        switch ((opcode >> 20) & 0xf)
                                        {
                                                case 0x0: rpclog("FLT F%i, R%i\n", FN, RD); break;
                                                case 0x1: rpclog("FIX R%i, F%i\n", RD, RM); break;
                                                case 0x2: rpclog("WFS R%i\n", RD); break;
                                                case 0x3: rpclog("RFS R%i\n", RD); break;
                                                case 0x4: rpclog("WFC R%i\n", RD); break;
                                                case 0x5: rpclog("RFC R%i\n", RD); break;
                                                default: rpclog("UND\n"); break;
                                        }
                                }
                        }
                        else
                        {
                                if (opcode & (1 << 15))
                                {
                                        switch ((opcode >> 20) & 0xf)
                                        {
                                                case 0x0: rpclog("MVF F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x1: rpclog("MNF F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x2: rpclog("ABS F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x3: rpclog("RND F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x4: rpclog("SQT F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x5: rpclog("LOG F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x6: rpclog("LGN F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x7: rpclog("EXP F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x8: rpclog("SIN F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0x9: rpclog("COS F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xa: rpclog("TAN F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xb: rpclog("ASN F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xc: rpclog("ACS F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xd: rpclog("ATN F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xe: rpclog("URD F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                                case 0xf: rpclog("NRM F%i, %s\n", FD, fpa_dis_getrm(RM)); break;
                                        }
                                }
                                else
                                {
                                        switch ((opcode >> 20) & 0xf)
                                        {
                                                case 0x0: rpclog("ADF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x1: rpclog("MUF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x2: rpclog("SUF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x3: rpclog("RSF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x4: rpclog("DVF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x5: rpclog("RDF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x6: rpclog("POW F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x7: rpclog("RPW F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x8: rpclog("RMF F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0x9: rpclog("FML F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0xa: rpclog("FDV F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0xb: rpclog("FRD F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                case 0xc: rpclog("POL F%i, F%i, %s  (%f, %f)\n", FD, FN, fpa_dis_getrm(RM), fparegs[FN], fpa_dis_getrmval(RM)); break;
                                                default: rpclog("UND\n"); break;
                                        }
                                }
                        }
                }
                else
                   rpclog("UND\n");
                break;
        }
}

/*Instruction types :
  Opcodes Cx/Dx, CP1 - LDF/STF
  Opcodes Cx/Dx, CP2 - LFM/SFM
  Opcodes Ex, bit 4 clear - Data processing
  Opcodes Ex, bit 4 set   - Register transfer
  Opcodex Ex, bit 4 set, RD=15 - Compare*/
int fpaopcode(uint32_t opcode)
{
        uint32_t temp[6];
        double *tf,*tf2;
        float *tfs;
        double tempf;
        int len,tempi;
        uint32_t addr;
        if (romset<2 || romset>3) return 1;
        tf=(double *)temp;
        tf2=(double *)&temp[4];
        tfs=(float *)temp;
/*        if (PC < 0x1800000 || output)
        {
                dumpfpa();
                rpclog("FPA op %08X %08X - ",opcode,PC);
                fpa_dasm(opcode);
        }*/
        switch ((opcode>>24)&0xF)
        {
                case 0xC: case 0xD:
//                        rpclog("%08X %03X\n",opcode,opcode&0x100);
                if (opcode&0x100) /*LDF/STF*/
                {
//                        rpclog("1\n");
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*Single*/
                                f32.f=(float)fparegs[FD];
                                temp[0]=f32.i;
                                temp[1]=temp[2]=0;
                                len=1;
                                cycles-=2;
//                                if (!(opcode&0x100000)) rpclog("Storing %08X %08X %08X %08X %f %f\n",addr,temp[0],temp[1],temp[2],fparegs[FD],*tfs);
                                break;
                                case 0x008000: /*Double*/
                                f64.f=fparegs[FD];
                                temp[0]=f64.i.l;
                                temp[1]=f64.i.h;
                                temp[2]=0;
                                len=2;
                                cycles-=3;
                                break;
                                case 0x400000: /*Long*/
                                convert64to80(temp, fparegs[FD]);
                                len=3;
                                cycles-=4;
                                break;
                                default:
//rpclog("Bad LDF/STF size %08X %08X\n",opcode&0x408000,opcode);
//                                armregs[15]+=4;
//                                armirq|=4;
//                                rpclog("Undefined LDF/STF\n",PC);
                                fpcr|=0x800;
//                                undefined();
                                return 1;
                                error("Bad LDF/STF size %08X %08X\n",opcode&0x408000,opcode);
                                dumpregs();
                                exit(-1);
                        }
//                        if (opcode&0x100000) rpclog("LDF %i,%i %i %08X %08X %08X\n",RN,FD,len,temp[0],temp[1],temp[2]);
//                        else                 rpclog("STF %i,%i %i %08X %08X %08X\n",RN,FD,len,temp[0],temp[1],temp[2]);
//                        rpclog("Address %07X len %i\n",addr,len);
                        if (opcode&0x100000)
                        {
                                switch (len)
                                {
                                        case 1:
                                        temp[0]=readmeml(addr);
                                        break;
                                        case 2:
                                        temp[1]=readmeml(addr);
                                        temp[0]=readmeml(addr+4);
                                        break;
                                        case 3:
                                        temp[0]=readmeml(addr);
                                        temp[1]=readmeml(addr+4);
                                        temp[2]=readmeml(addr+8);
                                        break;
                                }
                                switch (opcode&0x408000)
                                {
                                        case 0x000000: /*Single*/
                                        f32.i=temp[0];
                                        fparegs[FD]=(double)f32.f;
//                                        rpclog("Loaded %f %f %i %08X %08X %08X %08X\n",*tfs,fparegs[FD],len,addr,temp[0],temp[1],temp[2]);
                                        break;
                                        case 0x008000: /*Double*/
                                        f64.i.l=temp[0];
                                        f64.i.h=temp[1];
                                        fparegs[FD]=f64.f;
//                                        rpclog("F%i = %f %08X %08X\n",FD,(double)fparegs[FD], temp[0], temp[1]);
                                        break;

                                        case 0x400000: /*Long*/
//                                        rpclog("Long load %08X %08X %08X\n", temp[0], temp[1], temp[2]);
                                        fparegs[FD] = convert80to64(temp);
                                        break;
                                }
                        }
                        else
                        {
//                                rpclog("Write %f to %08X %08X %i %i\n",fparegs[FD],addr,armregs[RN]);
                                switch (len)
                                {
                                        case 1:
                                        writememl(addr,temp[0]);
                                        break;
                                        case 2:
                                        writememl(addr,temp[1]);
                                        writememl(addr+4,temp[0]);
                                        break;
                                        case 3:
                                        writememl(addr,temp[0]);
                                        writememl(addr+4,temp[1]);
                                        writememl(addr+8,temp[2]);
                                        break;
                                }
                        }
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode&0x200000) armregs[RN]=addr;
                        return 0;
                }
                if (opcode&0x100000) /*LFM*/
                {
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
//                        rpclog("LFM from %08X %08X %07X\n",GETADDR(RN),addr,PC);
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*4 registers*/
                                temp[0]=readmeml(addr);
                                temp[1]=readmeml(addr+4);
                                temp[2]=readmeml(addr+8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+12);
                                temp[1]=readmeml(addr+16);
                                temp[2]=readmeml(addr+20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+24);
                                temp[1]=readmeml(addr+28);
                                temp[2]=readmeml(addr+32);
                                fparegs[(FD+2)&7]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+36);
                                temp[1]=readmeml(addr+40);
                                temp[2]=readmeml(addr+44);
                                fparegs[(FD+3)&7]=convert80to64(&temp[0]);
                                cycles-=13;
                                break;
                                case 0x408000: /*3 registers*/
                                temp[0]=readmeml(addr);
                                temp[1]=readmeml(addr+4);
                                temp[2]=readmeml(addr+8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+12);
                                temp[1]=readmeml(addr+16);
                                temp[2]=readmeml(addr+20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+24);
                                temp[1]=readmeml(addr+28);
                                temp[2]=readmeml(addr+32);
                                fparegs[(FD+2)&7]=convert80to64(&temp[0]);
                                cycles-=10;
                                break;
                                case 0x400000: /*2 registers*/
                                temp[0]=readmeml(addr);
                                temp[1]=readmeml(addr+4);
                                temp[2]=readmeml(addr+8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                temp[0]=readmeml(addr+12);
                                temp[1]=readmeml(addr+16);
                                temp[2]=readmeml(addr+20);
                                fparegs[(FD+1)&7]=convert80to64(&temp[0]);
                                cycles-=7;
                                break;
                                case 0x008000: /*1 register*/
                                temp[0]=readmeml(addr);
                                temp[1]=readmeml(addr+4);
                                temp[2]=readmeml(addr+8);
                                fparegs[FD]=convert80to64(&temp[0]);
                                cycles-=4;
                                break;

                                default:
                                rpclog("Bad number of registers to load %06X\n",opcode&0x408000);
                                dumpregs();
                                exit(-1);
                        }
//                        rpclog("Loaded %08X  %i  %f %f %f %f\n",opcode&0x408000,FD,fparegs[FD],fparegs[(FD+1)&7],fparegs[(FD+2)&7],fparegs[(FD+3)&7]);
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode&0x200000) armregs[RN]=addr;
                        return 0;
                }
                else /*SFM*/
                {
                        addr=GETADDR(RN);
                        if (opcode&0x1000000)
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
//                        rpclog("SFM from %08X %08X %07X\n",GETADDR(RN),addr,PC);
                        switch (opcode&0x408000)
                        {
                                case 0x000000: /*4 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                writememl(addr,temp[0]);
                                writememl(addr+4,temp[1]);
                                writememl(addr+8,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                writememl(addr+12,temp[0]);
                                writememl(addr+16,temp[1]);
                                writememl(addr+20,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+2)&7]);
                                writememl(addr+24,temp[0]);
                                writememl(addr+28,temp[1]);
                                writememl(addr+32,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+3)&7]);
                                writememl(addr+36,temp[0]);
                                writememl(addr+40,temp[1]);
                                writememl(addr+44,temp[2]);
                                cycles-=13;
                                break;
                                case 0x408000: /*3 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                writememl(addr,temp[0]);
                                writememl(addr+4,temp[1]);
                                writememl(addr+8,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                writememl(addr+12,temp[0]);
                                writememl(addr+16,temp[1]);
                                writememl(addr+20,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+2)&7]);
                                writememl(addr+24,temp[0]);
                                writememl(addr+28,temp[1]);
                                writememl(addr+32,temp[2]);
                                cycles-=10;
                                break;
                                case 0x400000: /*2 registers*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                writememl(addr,temp[0]);
                                writememl(addr+4,temp[1]);
                                writememl(addr+8,temp[2]);
                                convert64to80(&temp[0],fparegs[(FD+1)&7]);
                                writememl(addr+12,temp[0]);
                                writememl(addr+16,temp[1]);
                                writememl(addr+20,temp[2]);
                                cycles-=7;
                                break;
                                case 0x008000: /*1 register*/
                                temp[2]=0;
                                convert64to80(&temp[0],fparegs[FD]);
                                writememl(addr,temp[0]);
                                writememl(addr+4,temp[1]);
                                writememl(addr+8,temp[2]);
                                cycles-=4;
                                break;
                                
                                default:
                                rpclog("Bad number of registers to store %06X\n",opcode&0x408000);
                                dumpregs();
                                exit(-1);
                        }
                        if (!(opcode&0x1000000))
                        {
                                if (opcode&0x800000) addr+=((opcode&0xFF)<<2);
                                else                 addr-=((opcode&0xFF)<<2);
                        }
                        if (opcode&0x200000) armregs[RN]=addr;
                        return 0;
                }
                /*LFM/SFM*/
                error("SFM opcode %08X\n",opcode);
                dumpregs();
                exit(-1);
                return 1;
                case 0xE:
                if (opcode & 0x10)
                {
                        if (RD == 15 && opcode & 0x100000) /*Compare*/
                        {
                                switch ((opcode >> 21) & 7)
                                {
                                        case 4: /*CMF*/
                                        case 6: /*CMFE*/
                                        if (opcode & 8) 
                                                tempf = fconstants[opcode & 7];
                                        else          
                                                tempf = fparegs[opcode & 7];
                                        setsubf(fparegs[FN], tempf);
                                        cycles -= 5;
                                        return 0;
                                        case 5: /*CNF*/
                                        case 7: /*CNFE*/
                                        if (opcode & 8) 
                                                tempf = fconstants[opcode & 7];
                                        else          
                                                tempf = fparegs[opcode & 7];
                                        setsubf(fparegs[FN], -tempf);
                                        cycles -= 5;
                                        return 0;
                                }
                                undeffpa
                        }
                        /*Register transfer*/
                        switch ((opcode>>20)&0xF)
                        {
                                case 0: /*FLT*/
                                fparegs[FN]=(double)(int32_t)armregs[RD];
                                cycles-=6;
//                                rpclog("FLT F%i now %f from R%i %08X %i %07X\n",FN,fparegs[FN],RD,armregs[RD],armregs[RD],PC);
                                return 0;
                                case 1: /*FIX*/
                                armregs[RD]=(int32_t)fpa_round(fparegs[opcode&7],opcode);
                                cycles-=8;
//                                rpclog("FIX F%i (%f) to R%i (%08X %i)\n",FN,fparegs[FN],RD,armregs[RD],armregs[RD]);
                                return 0;
                                case 2: /*WFS*/
                                fpsr=(armregs[RD]&0xFFFFFF)|(fpsr&0xFF000000);
                                cycles-=3;
                                return 0;
                                case 3: /*RFS*/
//                                rpclog("Read FPSR - %08X\n",fpsr);
                                armregs[RD]=fpsr|0x400;
                                cycles-=3;
                                return 0;
                                case 4: /*WFC*/
                                fpcr=(fpcr&~0xD00)|(armregs[RD]&0xD00);
                                cycles-=3;
                                return 0;
                                case 5: /*RFC*/
//                                rpclog("Read FPCR - %08X\n",fpcr);
                                armregs[RD]=fpcr;
                                fpcr&=~0xD00;
                                cycles-=3;
                                return 0;
                        }
                        undeffpa
                }
                if (opcode&8) tempf=fconstants[opcode&7];
                else          tempf=fparegs[opcode&7];
                if ((opcode&0x8000) && ((opcode&0xF08000)>=0x508000) && ((opcode&0xF08000)<0xE08000))
                {
                        fpcr&=0xD00;
                        fpcr|=0x400; /*Arithmetic bounce*/
                        fpcr|=(opcode&0xFFF0FF); /*Opcode, destination, source 1, source 2, rounding*/
//                        undefined();
                        return 1;
                }
                switch (opcode&0xF08000)
                {
                        case 0x000000: /*ADF*/
                        fparegs[FD]=fparegs[FN]+tempf;
                        cycles-=2;
                        return 0;
                        case 0x100000: /*MUF*/
                        cycles-=3;
                        case 0x900000: /*FML*/
                        fparegs[FD]=fparegs[FN]*tempf;
                        cycles-=5;
                        return 0;
                        case 0x200000: /*SUF*/
                        fparegs[FD]=fparegs[FN]-tempf;
                        cycles-=2;
                        return 0;
                        case 0x300000: /*RSF*/
                        fparegs[FD]=tempf-fparegs[FN];
                        cycles-=2;
                        return 0;
                        case 0x400000: /*DVF*/
                        case 0xA00000: /*FDV*/
                        fparegs[FD]=fparegs[FN]/tempf;
                        cycles-=30;
                        return 0;
                        case 0x500000: /*RDV*/
                        case 0xB00000: /*FRD*/
                        fparegs[FD]=tempf/fparegs[FN];
                        cycles-=30;
                        return 0;
                        case 0x800000: /*RMF*/
                        fparegs[FD]=fmod(fparegs[FN],tempf);
                        cycles-=30;
                        return 0;
                        
                        case 0x008000: /*MVF*/
                        fparegs[FD]=tempf;
                        cycles--;
                        return 0;
                        case 0x108000: /*MNF*/
                        fparegs[FD]=-tempf;
                        cycles--;
                        return 0;
                        case 0x208000: /*ABS*/
                        fparegs[FD]=fabs(tempf);
                        cycles--;
                        return 0;
                        case 0x308000: /*RND*/
                        fparegs[FD]=(double)fpa_round(tempf,opcode);
                        cycles--;
                        return 0;
                        case 0x408000: /*SQT*/
                        fparegs[FD]=sqrt(tempf);
                        cycles-=5;
                        return 0;
                        
                        case 0xe08000: /*URD*/
                        fparegs[FD] = fpa_round(tempf, opcode);
                        cycles -= 2;
                        return 0;
                        case 0xf08000: /*NRM*/
                        fparegs[FD] = tempf;
                        cycles -= 2;
                        return 0;
                        #if 0
                        case 0x508000: /*LOG*/
                        undeffpa
                        fparegs[FD]=log10(tempf);
                        return 0;
                        case 0x608000: /*LGN*/
                        undeffpa
                        fparegs[FD]=log(tempf);
                        return 0;
                        case 0x708000: /*EXP*/
                        undeffpa
                        fparegs[FD]=exp(tempf);
                        return 0;
                        case 0x808000: /*SIN*/
                        undeffpa
                        fparegs[FD]=sin(tempf);
                        return 0;
                        case 0x908000: /*COS*/
                        undeffpa
                        fparegs[FD]=cos(tempf);
                        return 0;
                        case 0xA08000: /*TAN*/
                        undeffpa
                        fparegs[FD]=tan(tempf);
                        return 0;
                        case 0xB08000: /*ASN*/
                        undeffpa
                        fparegs[FD]=asin(tempf);
                        return 0;
                        case 0xC08000: /*ACS*/
                        undeffpa
                        fparegs[FD]=acos(tempf);
                        return 0;
                        case 0xD08000: /*ATN*/
                        undeffpa
                        fparegs[FD]=atan(tempf);
                        return 0;
                        #endif
                }
                        fpcr&=0xD00;
                        fpcr|=0x400; /*Arithmetic bounce*/
                        fpcr|=(opcode&0xFFF0FF); /*Opcode, destination, source 1, source 2, rounding*/
//                        undefined();
                        return 1;
        }
        return 0;
}
