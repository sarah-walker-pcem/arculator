#include <stdio.h>
#include <zlib.h>
#include "mfm.h"

#define printf rpclog

unsigned long gzgetil(gzFile *f)
{
        unsigned long temp=gzgetc(f);
        temp|=(gzgetc(f)<<8);
        temp|=(gzgetc(f)<<16);
        temp|=(gzgetc(f)<<24);
        return temp;
}

MFM *mfm_open(gzFile *f, int d)
{
        MFM *mfm;
        int c;
        int pos=8+(166*12);
        printf("MFM open\n");
        
        mfm=malloc(sizeof(MFM));

        mfm->f=f;
        
        gzseek(f,8,SEEK_SET);

	for (c=0;c<160;c++)
	{
                mfm->sdtrack[c].len=gzgetil(f);
	        mfm->sdtrack[c].rd=(mfm->sdtrack[c].len+7)>>3;
		mfm->sdtrack[c].pos=pos;
		pos+=mfm->sdtrack[c].rd;

	        mfm->track[c].len=gzgetil(f);
	        mfm->track[c].rd=(mfm->track[c].len+7)>>3;
		mfm->track[c].pos=pos;
		pos+=mfm->track[c].rd;

	        mfm->hdtrack[c].len=gzgetil(f);
	        mfm->hdtrack[c].rd=(mfm->hdtrack[c].len+7)>>3;
		mfm->hdtrack[c].pos=pos;
		pos+=mfm->hdtrack[c].rd;

		printf("Track %i - %i - %i %i %i\n",c,pos,mfm->track[c].len,mfm->sdtrack[c].len,mfm->hdtrack[c].len);
	}
	return mfm;
}

int mfm_get_last_track(MFM *mfm)
{
        return 166;
}

void mfm_close(MFM *mfm)
{
        if (mfm->f) gzclose(mfm->f);
}

int mfm_loadtrack(MFM *mfm, unsigned short *buffer, unsigned short *timing, int tracknum, int *tracklen, int *indexoffset, int *mr, int density)
{
        switch (density)
        {
                case 0:
                gzseek(mfm->f,mfm->sdtrack[tracknum].pos,SEEK_SET);
                gzread(mfm->f,buffer,mfm->sdtrack[tracknum].rd);
                *tracklen=mfm->sdtrack[tracknum].len;
                break;
                case 1:
                gzseek(mfm->f,mfm->track[tracknum].pos,SEEK_SET);
                gzread(mfm->f,buffer,mfm->track[tracknum].rd);
                *tracklen=mfm->track[tracknum].len;
                break;
                case 2:
                gzseek(mfm->f,mfm->hdtrack[tracknum].pos,SEEK_SET);
                gzread(mfm->f,buffer,mfm->hdtrack[tracknum].rd);
                *tracklen=mfm->hdtrack[tracknum].len;
                break;
        }
        *indexoffset=0;
        return *tracklen;
}
