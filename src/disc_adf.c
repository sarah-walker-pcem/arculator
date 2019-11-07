/*Arculator 2.0 by Sarah Walker
  ADFS disc support (really all double-density formats)*/
#include <stdio.h>
#include <string.h>
#include "arc.h"
#include "disc.h"
#include "disc_adf.h"

static struct
{
        FILE *f;
        uint8_t track_data[2][20*1024];
        
        int dblside;
        int sectors, size, track;
        int dblstep;
        int density;
        int maxsector;
} adf[4];

static int adf_sector,   adf_track,   adf_side,    adf_drive;
static int adf_inread,   adf_readpos, adf_inwrite, adf_inreadaddr;
static int adf_notfound;
static int adf_rsector=0;
static int adf_informat=0;

static int adf_pause = 0;
static int adf_index = 6250;

void adf_init()
{
        memset(adf, 0, sizeof(adf));
//        adl[0] = adl[1] = 0;
        adf_notfound = 0;
}

void adf_loadex(int drive, char *fn, int sectors, int size, int sides, int dblstep, int density)
{
        writeprot[drive] = 0;
        adf[drive].f = fopen(fn, "rb+");
        if (!adf[drive].f)
        {
                adf[drive].f = fopen(fn, "rb");
                if (!adf[drive].f) return;
                writeprot[drive] = 1;
        }
        fwriteprot[drive] = writeprot[drive];
        fseek(adf[drive].f, -1, SEEK_END);
        drives[drive].seek        = adf_seek;
        drives[drive].readsector  = adf_readsector;
        drives[drive].writesector = adf_writesector;
        drives[drive].readaddress = adf_readaddress;
        drives[drive].poll        = adf_poll;
        drives[drive].format      = adf_format;
        drives[drive].stop        = adf_stop;
        adf[drive].sectors = sectors;
        adf[drive].size = size;
        adf[drive].dblside = sides;
        adf[drive].dblstep = dblstep;
        adf[drive].density = density;
        adf[drive].maxsector = (ftell(adf[drive].f)+1 ) / size;
}

void adf_load(int drive, char *fn)
{
        adf_loadex(drive, fn, 16, 256, 0, 0, 1);
}

void adf_arcdd_load(int drive, char *fn)
{
        adf_loadex(drive, fn, 5, 1024, 1, 0, 1);
}

void adf_archd_load(int drive, char *fn)
{
        rpclog("HDload\n");
        adf_loadex(drive, fn, 10, 1024, 1, 0, 2);
}

void adl_load(int drive, char *fn)
{
        adf_loadex(drive, fn, 16, 256, 1, 0, 1);
}


void adf_close(int drive)
{
        if (adf[drive].f) fclose(adf[drive].f);
        adf[drive].f = NULL;
}

void adf_seek(int drive, int track)
{
        if (!adf[drive].f)
                return;
//        rpclog("Seek %i %i %i %i %i %i\n",drive,track,adfsectors[drive],adfsize[drive],adl[drive],adfsectors[drive]*adfsize[drive]);
        if (adf[drive].dblstep)
                track /= 2;
        adf[drive].track = track;
        if (adf[drive].dblside)
        {
                fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size * 2, SEEK_SET);
                fread(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
                fread(adf[drive].track_data[1], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
        }
        else
        {
                fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size, SEEK_SET);
                fread(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
        }
}
void adf_writeback(int drive, int track)
{
        if (!adf[drive].f)
                return;
                
        if (adf[drive].dblstep)
                track /= 2;
                
        if (adf[drive].dblside)
        {
                fseek(adf[drive].f, track * adf[drive].sectors * adf[drive].size * 2, SEEK_SET);
                fwrite(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
                fwrite(adf[drive].track_data[1], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
        }
        else
        {
                fseek(adf[drive].f, track *  adf[drive].sectors * adf[drive].size, SEEK_SET);
                fwrite(adf[drive].track_data[0], adf[drive].sectors * adf[drive].size, 1, adf[drive].f);
        }
}

void adf_readsector(int drive, int sector, int track, int side, int density)
{
        int sector_nr = sector + adf[drive].sectors * (track * (adf[drive].dblside ? 2 : 1) + (side ? 1 : 0));
        
        adf_sector = sector;
        adf_track  = track;
        adf_side   = side;
        adf_drive  = drive;
        if (adf[drive].size == 512)
                adf_sector--;
        rpclog("ADFS Read sector %i %i %i %i\n",drive,side,track,sector);

        if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
                (track != adf[drive].track) || (sector_nr >= adf[drive].maxsector) ||
                (adf_sector > adf[drive].sectors))
        {
//                printf("Not found! %08X (%i %i) %i (%i %i)\n",adff[drive],side,adl[drive],density,track,adftrackc[drive]);
                adf_notfound=500;
                return;
        }
//        printf("Found\n");
        adf_inread  = 1;
        adf_readpos = 0;
}

void adf_writesector(int drive, int sector, int track, int side, int density)
{
        int sector_nr = sector + adf[drive].sectors * (track * (adf[drive].dblside ? 2 : 1) + (side ? 1 : 0));

//        if (adfdblstep[drive]) track/=2;
        adf_sector = sector;
        adf_track  = track;
        adf_side   = side;
        adf_drive  = drive;
        if (adf[drive].size == 512)
                adf_sector--;
//        printf("ADFS Write sector %i %i %i %i\n",drive,side,track,sector);

        if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
                (track != adf[drive].track) || (sector_nr >= adf[drive].maxsector) ||
                (adf_sector > adf[drive].sectors))
        {
                adf_notfound = 500;
                return;
        }
        adf_inwrite = 1;
        adf_readpos = 0;
}

void adf_readaddress(int drive, int track, int side, int density)
{
        if (adf[drive].dblstep)
                track /= 2;

        adf_drive = drive;
        adf_track = track;
        adf_side  = side;
        rpclog("Read address %i %i %i  %i\n",drive,side,track, ins);

        if (!adf[drive].f || (side && !adf[drive].dblside) ||
                (density != adf[drive].density) || (track != adf[drive].track))
        {
                adf_notfound=500;
                return;
        }
        adf_inreadaddr = 1;
        adf_readpos    = 0;
        adf_pause = 100;//500;
}

void adf_format(int drive, int track, int side, int density)
{
        if (adf[drive].dblstep)
                track /= 2;
                
        adf_drive = drive;
        adf_track = track;
        adf_side  = side;

        if (!adf[drive].f || (side && !adf[drive].dblside) || (density != adf[drive].density) ||
                track != adf[drive].track)
        {
                adf_notfound = 500;
                return;
        }
        adf_sector  = 0;
        adf_readpos = 0;
        adf_informat  = 1;
}

void adf_stop()
{
        adf_pause = adf_notfound = adf_inread = adf_inwrite = adf_inreadaddr = adf_informat = 0;
}

void adf_poll()
{
        int c;

        adf_index--;
        if (!adf_index)
        {
                adf_index = 6250;
                fdc_indexpulse();
        }
        
        if (adf_pause)
        {
                adf_pause--;
                if (adf_pause)
                   return;
        }

        if (adf_notfound)
        {
                adf_notfound--;
                if (!adf_notfound)
                {
//                        rpclog("Not found!\n");
                        fdc_notfound();
                }
        }
        if (adf_inread && adf[adf_drive].f)
        {
//                rpclog("Read pos %i\n", adf_readpos);
//                if (!adfreadpos) rpclog("%i\n",adfsector*adfsize[adfdrive]);
                fdc_data(adf[adf_drive].track_data[adf_side][(adf_sector * adf[adf_drive].size) + adf_readpos]);
                adf_readpos++;
                if (adf_readpos == adf[adf_drive].size)
                {
//                        rpclog("Read %i bytes\n",adf_readpos);
                        adf_inread = 0;
                        fdc_finishread();
                }
        }
        if (adf_inwrite && adf[adf_drive].f)
        {
                if (writeprot[adf_drive])
                {
//                        rpclog("writeprotect\n");
                        fdc_writeprotect();
                        adf_inwrite = 0;
                        return;
                }
//                rpclog("Write data %i\n",adf_readpos);
                c = fdc_getdata(adf_readpos == (adf[adf_drive].size - 1));
                if (c == -1)
                {
//Carlo Concari: do not write if data not ready yet
                          return;
//                        printf("Data overflow!\n");
//                        exit(-1);
                }
                adf[adf_drive].track_data[adf_side][(adf_sector * adf[adf_drive].size) + adf_readpos] = c;
                adf_readpos++;
                if (adf_readpos == adf[adf_drive].size)
                {
//                        rpclog("write over\n");
                        adf_inwrite = 0;
                        fdc_finishread();
                        adf_writeback(adf_drive, adf_track);
                }
        }
        if (adf_inreadaddr && adf[adf_drive].f)
        {
//                rpclog("adf_inreadaddr %08X\n", fdc_sectorid);
                if (fdc_sectorid)
                {
                        fdc_sectorid(adf_track, adf_side, adf_rsector + ((adf[adf_drive].size == 512) ? 1 : 0), (adf[adf_drive].size == 256) ? 1 : ((adf[adf_drive].size == 512) ? 2 : 3), 0, 0);
                        adf_inreadaddr = 0;
                        adf_rsector++;
                        if (adf_rsector == adf[adf_drive].sectors)
                        {
                                adf_rsector=0;
                                rpclog("adf_rsector reset\n");
                        }
                }
                else
                {
                        switch (adf_readpos)
                        {
                                case 0: fdc_data(adf_track); break;
                                case 1: fdc_data(adf_side); break;
                                case 2: fdc_data(adf_rsector + ((adf[adf_drive].size == 512) ? 1 : 0)); break;
                                case 3: fdc_data((adf[adf_drive].size == 256) ? 1 : ((adf[adf_drive].size == 512) ? 2 : 3)); break;
                                case 4: fdc_data(0); break;
                                case 5: fdc_data(0); break;
                                case 6:
                                adf_inreadaddr = 0;
                                fdc_finishread();
                                rpclog("Read addr - %i %i %i %i 0 0 (%i %i %i)\n", adf_track, adf_side, adf_rsector + ((adf[adf_drive].size == 512) ? 1 : 0), (adf[adf_drive].size == 256) ? 1 : ((adf[adf_drive].size == 512) ? 2 : 3), adf[adf_drive].sectors, adf_drive, adf_rsector);
                                adf_rsector++;
                                if (adf_rsector == adf[adf_drive].sectors)
                                {
                                        adf_rsector=0;
                                        rpclog("adf_rsector reset\n");
                                }
                                break;
                        }
                }
                adf_readpos++;
        }
        if (adf_informat && adf[adf_drive].f)
        {
                if (writeprot[adf_drive])
                {
                        fdc_writeprotect();
                        adf_informat = 0;
                        return;
                }
                adf[adf_drive].track_data[adf_side][(adf_sector * adf[adf_drive].size) + adf_readpos] = 0;
                adf_readpos++;
                if (adf_readpos == adf[adf_drive].size)
                {
                        adf_readpos = 0;
                        adf_sector++;
                        if (adf_sector == adf[adf_drive].sectors)
                        {
                                adf_informat = 0;
                                fdc_finishread();
                                adf_writeback(adf_drive, adf_track);
                        }
                }
        }
}
