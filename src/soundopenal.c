#include <stdio.h>
#include <AL/al.h>
#include <al/alut.h>
#include "arc.h"

FILE *allog;

ALuint buffers[4]; // front and back buffers
ALuint source;     // audio source
ALenum format;     // internal format

#define FREQ (125000>>1)
#define BUFLEN (2500<<1)

void closeal();

void check()
{
        ALenum error;
        if ((error = alGetError()) != AL_NO_ERROR)
        {
                rpclog("AL Error : %08X\n", error);
              //  rpclog("Description : %s\n",alGetErrorString(error));
        }
/*        if ((error = alutGetError()) != ALUT_ERROR_NO_ERROR)
        {
                rpclog("AL Error : %08X\n", error);
                rpclog("Description : %s\n",alutGetErrorString(error));
        }*/
}

void initalmain(int argc, char *argv[])
{
        alutInit(&argc, argv);
        check();
        atexit(closeal);
        printf("AlutInit\n");
}

void closeal()
{
        alutExit();
}

void inital()
{
        int c;
        short buf[BUFLEN*2];
        format = AL_FORMAT_STEREO16;
        check();

        alGenBuffers(4, buffers);
        check();
        
        alGenSources(1, &source);
        check();
        
        alSource3f(source, AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source, AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source, AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source, AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source, AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        memset(buf,0,BUFLEN*4);
        
        for (c=0;c<4;c++)
            alBufferData(buffers[c], AL_FORMAT_STEREO16, buf, BUFLEN*2, FREQ);
        alSourceQueueBuffers(source, 4, buffers);
        check();
        alSourcePlay(source);
        check();
        printf("InitAL\n");
}

void givealbuffer(short *buf)
{
        int processed;
        int state;
        int c;
        
        alGetSourcei(source, AL_SOURCE_STATE, &state);

        if (state==0x1014)
        {
                alSourcePlay(source);
                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;

                alSourceUnqueueBuffers(source, 1, &buffer);
//                printf("U ");
                check();

                for (c=0;c<BUFLEN*2;c++) buf[c]^=0x8000;
                alBufferData(buffer, AL_FORMAT_STEREO16, buf, BUFLEN*2, FREQ);
//                printf("B ");
                check();

                alSourceQueueBuffers(source, 1, &buffer);
//                printf("Q ");
                check();
                
//                printf("\n");

//                if (!allog) allog=fopen("al.pcm","wb");
//                fwrite(buf,BUFLEN*2,1,allog);
        }
}
