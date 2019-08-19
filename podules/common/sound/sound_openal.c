#include <string.h>  
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include "sound_out.h"

typedef struct openal_t
{
        ALuint buffers[4]; // front and back buffers
        ALuint source;     // audio source

        int new_context; /*Did we create a new context on init?*/
        int freq;
        int buffer_size;
} openal_t;

static void check()
{
        ALenum error;
        if ((error = alGetError()) != AL_NO_ERROR)
        {
//                lark_log("AL Error : %08X\n", error);
//                lark_log("Description : %s\n",alGetErrorString(error));
        }
/*        if ((error = alutGetError()) != ALUT_ERROR_NO_ERROR)
        {
                printf("ALut Error : %08X\n", error);
                printf("Description : %s\n",alutGetErrorString(error));
        }*/
}

void sound_out_close(void *p)
{
        openal_t *openal = (openal_t *)p;
        
        alSourceStop(openal->source);
        alDeleteSources(1, &openal->source);
        alDeleteBuffers(4, openal->buffers);
        
        if (openal->new_context)
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
	
	free(openal);
}

void *sound_out_init(void *p, int freq, int buffer_size, void (*log)(const char *format, ...), const podule_callbacks_t *podule_callbacks, podule_t *podule)
{
        int c;
        int16_t *samp_buf;//[SAMP_BUFLEN*2];
        openal_t *openal = malloc(sizeof(openal_t));
        memset(openal, 0, sizeof(openal_t));

        openal->freq = freq;
        openal->buffer_size = buffer_size;
        
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
        	
        	openal->new_context = 1;
        }

        alGenBuffers(4, openal->buffers);
        check();
        
        alGenSources(1, &openal->source);
        check();
        
        alSource3f(openal->source, AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(openal->source, AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(openal->source, AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (openal->source, AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (openal->source, AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        samp_buf = malloc(openal->buffer_size*2*2);
        memset(samp_buf, 0, openal->buffer_size*2*2);
        
        for (c = 0; c < 4; c++)
                alBufferData(openal->buffers[c], AL_FORMAT_STEREO16, samp_buf, openal->buffer_size*2*2, openal->freq);
        alSourceQueueBuffers(openal->source, 4, openal->buffers);
        check();
        alSourcePlay(openal->source);
        check();

        return openal;
}

void sound_out_buffer(void *p, int16_t *buf, int len)
{
        openal_t *openal = (openal_t *)p;
        int processed;
        int state;
        
        alGetSourcei(openal->source, AL_SOURCE_STATE, &state);

        if (state == 0x1014)
        {
                alSourcePlay(openal->source);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(openal->source, AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;

                alSourceUnqueueBuffers(openal->source, 1, &buffer);
//                printf("U ");
                check();

                alBufferData(buffer, AL_FORMAT_STEREO16, buf, len*2*2, openal->freq);
//                printf("B ");
                check();

                alSourceQueueBuffers(openal->source, 1, &buffer);
//                printf("Q ");
                check();
        }
}
