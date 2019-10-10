#ifndef CDROM_IOCTL_H
#define CDROM_IOCTL_H

/*ATAPI stuff*/
typedef struct ATAPI
{
        int (*ready)(void);
        int (*medium_changed)(void);
        int (*readtoc)(uint8_t *b, uint8_t starttrack, int msf, int maxlen, int single);
        int (*readtoc_session)(uint8_t *b, int msf, int maxlen);
        int (*readtoc_raw)(uint8_t *b, int maxlen);
        uint8_t (*getcurrentsubchannel)(uint8_t *b, int msf);
        int (*readsector)(uint8_t *b, int sector, int count);
        void (*readsector_raw)(uint8_t *b, int sector);
        void (*playaudio)(uint32_t pos, uint32_t len, int ismsf);
        void (*seek)(uint32_t pos);
        void (*load)(void);
        void (*eject)(void);
        void (*pause)(void);
        void (*resume)(void);
        uint32_t (*size)(void);
        int (*status)(void);
        int (*is_track_audio)(uint32_t pos, int ismsf);
        void (*stop)(void);
        void (*exit)(void);
} ATAPI;

extern ATAPI *atapi;

#define CD_STATUS_EMPTY		0
#define CD_STATUS_DATA_ONLY	1
#define CD_STATUS_PLAYING	2
#define CD_STATUS_PAUSED	3
#define CD_STATUS_STOPPED	4


/* this header file lists the functions provided by
   various platform specific cdrom-ioctl files */

extern int ioctl_open(char d);
extern void ioctl_reset();
extern void ioctl_close();
void ioctl_set_drive(const char *path);
void ioctl_audio_callback(int16_t *output, int len);
void ioctl_audio_stop();

struct podule_config_selection_t;
struct podule_config_selection_t *cdrom_devices_config(void);

#endif /* ! CDROM_IOCTL_H */
