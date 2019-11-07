/*Arculator 2.0 by Sarah Walker
  OpenAL interface*/
#include <string.h>  
#include <stdio.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include "arc.h"
#include "disc.h"
#include "sound.h"
#include "soundopenal.h"

static ALuint buffers[4]; // front and back buffers
static ALuint source[2];     // audio source
static ALuint buffersdd[4]; // front and back buffers
static ALenum format;     // internal format

#define FREQ 62500
//#define BUFLEN (3125<<2)
#define BUFLEN (2500<<2)

static void check()
{
        ALenum error;
        if ((error = alGetError()) != AL_NO_ERROR)
        {
                //printf("AL Error : %08X\n", error);
                //printf("Description : %s\n",alGetErrorString(error));
        }
/*        if ((error = alutGetError()) != ALUT_ERROR_NO_ERROR)
        {
                printf("ALut Error : %08X\n", error);
                printf("Description : %s\n",alutGetErrorString(error));
        }*/
}

ALvoid  alutInit(ALint *argc,ALbyte **argv) 
{
	ALCcontext *Context;
	ALCdevice *Device;
	
	//Open device
 	Device=alcOpenDevice((void *)"");
	//Create context(s)
	Context=alcCreateContext(Device,NULL);
	//Set active context
	alcMakeContextCurrent(Context);
	//Register extensions
}

ALvoid  alutExit() 
{
	ALCcontext *Context;
	ALCdevice *Device;

	//Unregister extensions

	//Get active context
	Context=alcGetCurrentContext();
	//Get device for active context
	Device=alcGetContextsDevice(Context);
	//Disable context
	alcMakeContextCurrent(NULL);
	//Release context(s)
	alcDestroyContext(Context);
	//Close device
	alcCloseDevice(Device);
}

void al_init_main(int argc, char *argv[])
{
        alutInit(0, NULL);//&argc, argv);
        check();
//        atexit(closeal);
//        printf("AlutInit\n");
}

void al_close()
{
        alutExit();
}

static int16_t tempbuf[BUFLEN>>1];
static int16_t tempbufdd[4410*2];

void al_init()
{
        int c;
        format = AL_FORMAT_STEREO16;
        check();

        alGenBuffers(4, buffers);
        check();
        
        alGenSources(2, source);
        check();
        
        alSource3f(source[0], AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source[0], AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source[0], AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source[0], AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source[0], AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        memset(tempbuf, 0, BUFLEN);
        
        for (c = 0; c < 4; c++)
            alBufferData(buffers[c], AL_FORMAT_STEREO16, tempbuf, 2400*4, 48000);
        alSourceQueueBuffers(source[0], 4, buffers);
        check();
        alSourcePlay(source[0]);
        check();
//        printf("InitAL\n");

        alGenBuffers(4, buffersdd);
        check();

        alSource3f(source[1], AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source[1], AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source[1], AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source[1], AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source[1], AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        memset(tempbufdd, 0, 4410 * 4);

        for (c = 0; c < 4; c++)
            alBufferData(buffersdd[c], AL_FORMAT_STEREO16, tempbufdd, 4410*4, 44100);
        alSourceQueueBuffers(source[1], 4, buffersdd);
        check();
        alSourcePlay(source[1]);
        check();
//        printf("InitAL\n");
}

static int16_t zbuf[16384];

void al_givebuffer(int16_t *buf)
{
        int processed;
        int state;
        
        alGetSourcei(source[0], AL_SOURCE_STATE, &state);

        if (state == 0x1014)
        {
                alSourcePlay(source[0]);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source[0], AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;
                double gain = pow(10.0, (double)sound_gain / 20.0);

                alListenerf(AL_GAIN, gain);

                alSourceUnqueueBuffers(source[0], 1, &buffer);
//                printf("U ");
                check();

//                for (c = 0; c < (BUFLEN >> 1); c++) zbuf[c] = buf[c >> 1];
                
                alBufferData(buffer, AL_FORMAT_STEREO16, buf, 2400*4, 48000);
//                printf("B ");
                check();

                alSourceQueueBuffers(source[0], 1, &buffer);
//                printf("Q ");
                check();
                
        }
}

void al_givebufferdd(int16_t *buf)
{
        int processed;
        int state;
        int c;

        if (disc_noise_gain == DISC_NOISE_DISABLED)
                return;
                
        alGetSourcei(source[1], AL_SOURCE_STATE, &state);

        if (state == 0x1014)
        {
                alSourcePlay(source[1]);
//                printf("Resetting sounddd\n");
        }
        alGetSourcei(source[1], AL_BUFFERS_PROCESSED, &processed);
//rpclog("Get source\n");
        check();
//rpclog("Got source\n");
        if (processed>=1)
        {
                ALuint buffer;
                int gain = (int)(pow(10.0, (double)disc_noise_gain / 20.0) * 65536.0);

//rpclog("Unqueue\n");
                alSourceUnqueueBuffers(source[1], 1, &buffer);
                check();

                for (c = 0; c < (4410 * 2); c++)
                        zbuf[c] = (buf[c >> 1] * gain) >> 16;

//rpclog("BufferData\n");
                alBufferData(buffer, AL_FORMAT_STEREO16, zbuf, 4410*4, 44100);
                check();

//rpclog("Queue\n");
                alSourceQueueBuffers(source[1], 1, &buffer);
                check();
        }
        
//        rpclog("DDnoise3\n");
}
