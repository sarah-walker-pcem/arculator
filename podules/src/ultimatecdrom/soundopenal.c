#include <string.h>  
#include <stdio.h>
#include <stdint.h>
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include "sound.h"

static ALuint buffers[4]; // front and back buffers
static ALuint source;     // audio source
static ALenum format;     // internal format

#define CD_FREQ 44100
#define CD_BUFLEN (CD_FREQ / 10)

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

void sound_close()
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

void sound_init()
{
        int c;
        int16_t cd_buf[CD_BUFLEN*2];

        if (!alcGetCurrentContext())
        {
        	ALCcontext *Context;
        	ALCdevice *Device;

        	//Open device
         	Device=alcOpenDevice((void *)"");
        	//Create context(s)
        	Context=alcCreateContext(Device,NULL);
        	//Set active context
        	alcMakeContextCurrent(Context);
        }

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

        memset(cd_buf, 0, sizeof(cd_buf));
        
        for (c = 0; c < 4; c++)
                alBufferData(buffers[c], AL_FORMAT_STEREO16, cd_buf, CD_BUFLEN*2*2, CD_FREQ);
        alSourceQueueBuffers(source, 4, buffers);
        check();
        alSourcePlay(source);
        check();
//        printf("InitAL\n");

}

void sound_givebuffer(int16_t *buf, int len)
{
        int processed;
        int state;
        
        alGetSourcei(source, AL_SOURCE_STATE, &state);

        if (state == 0x1014)
        {
                alSourcePlay(source);
//                printf("Resetting sound\n");
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

                alBufferData(buffer, AL_FORMAT_STEREO16, buf, len*2*2, CD_FREQ);
//                printf("B ");
                check();

                alSourceQueueBuffers(source, 1, &buffer);
//                printf("Q ");
                check();
                
        }
}
