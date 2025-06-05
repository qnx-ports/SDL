/*
    similar to sys/asound_common.h except for 2 unions in
    snd_pcm_channel_setup_t and snd_pcm_channel_params_t
    what is use of these unions?
*/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_strerror( int errnum );

typedef struct snd_ctl snd_ctl_t;
typedef struct snd_mixer snd_mixer_t;
typedef struct snd_pcm snd_pcm_t;
typedef struct snd_afm snd_afm_t;

#ifdef __cplusplus
}
#endif

typedef union snd_pcm_sync
{
	uint8_t		id[16];
	uint16_t	id16[8];
	uint32_t	id32[4];
	uint64_t	id64[2];
}		snd_pcm_sync_t;

struct snd_pcm_format
{
	uint32_t	interleave:1;
	int32_t		format;
	int32_t		rate;
	int32_t		voices;
	int32_t		special;
	uint8_t		reserved[124];			/* must be filled with zero */
};
typedef struct snd_pcm_format snd_pcm_format_t;

typedef struct snd_pcm_digital
{
	uint8_t		dig_status[24];			/* AES/EBU/IEC958 channel status bits */
	uint8_t		dig_subcode[147];		/* AES/EBU/IEC958 subcode bits */
	uint8_t		dig_valid:1;			/* must be non-zero to accept these values */
	uint8_t		dig_subframe[4];		/* AES/EBU/IEC958 subframe bits */
	uint8_t		reserved[128];			/* must be filled with zero */
}		snd_pcm_digital_t;

typedef struct snd_pcm_channel_params
{
    int32_t             channel;
    int32_t             mode;
    snd_pcm_sync_t      sync;               /* hardware synchronization ID */
    snd_pcm_format_t    format;
    snd_pcm_digital_t   digital;
    int32_t             start_mode;
    int32_t             stop_mode;
    int32_t             time:1, ust_time:1;
    uint32_t            why_failed;
    union
    {
        struct
        {
            int32_t     queue_size;
            int32_t     fill;
            int32_t     max_fill;
            uint8_t     reserved[124];    /* must be filled with zeroes */
        }       stream;
        struct
        {
            int32_t     frag_size;
            int32_t     frags_min;
            int32_t     frags_max;
            uint8_t     reserved[120];   /* must be filled with zeroes */
        }       block;
        uint8_t     reserved[128];       /* must be filled with zeroes */
    }       buf;
    char        sw_mixer_subchn_name[32];
    char        audio_type_name[32];
    uint8_t     reserved[64];            /* must be filled with zeroes */
} snd_pcm_channel_params_t;

typedef struct snd_pcm_channel_status
{
        int32_t                  channel;
        int32_t                  mode;
        int32_t                  status;
        uint32_t                 scount;
        struct timeval           stime;
        uint64_t                 ust_stime;
        int32_t                  frag;
        int32_t                  count;
        int32_t                  free;
        int32_t                  underrun;
        int32_t                  overrun;
        int32_t                  overrange;
        uint32_t                 subbuffered;
        uint32_t                 ducking_state;
#define SND_PCM_DUCKING_STATE_INACTIVE          (0x00)
#define SND_PCM_DUCKING_STATE_ACTIVE            (1<<0)
#define SND_PCM_DUCKING_STATE_HARD_SUSPENDED    (1<<1) 
#define SND_PCM_DUCKING_STATE_SOFT_SUSPENDED    (1<<2)
#define SND_PCM_DUCKING_STATE_PAUSED            (1<<3)
#define SND_PCM_DUCKING_STATE_FORCED_ACTIVE     (1<<4)        
        struct timeval           stop_time;
        uint8_t                  reserved[112];
} snd_pcm_channel_status_t;

typedef struct
{
	int32_t		type;
	char		name[36];
	int32_t		index;
	uint8_t	 	reserved[120];			/* must be filled with zero */
	int32_t		weight;					/* Reserved used for internal sorting oprations */
}		snd_mixer_eid_t;

typedef struct
{
	int32_t		type;
	char		name[32];
	int32_t		index;
	uint8_t		reserved[124];			/* must be filled with zero */
	int32_t		weight;					/* Reserved used for internal sorting operations */
}		snd_mixer_gid_t;

typedef struct snd_pcm_channel_setup
{
    int32_t             channel;
    int32_t             mode;
    snd_pcm_format_t    format;
    snd_pcm_digital_t   digital;
    union
    {
        struct
        {
            int32_t     queue_size;
            uint8_t     reserved[124]; /* must be filled with zeroes */
        }       stream;
        struct
        {
            int32_t     frag_size;
            int32_t     frags;
            int32_t     frags_min;
            int32_t     frags_max;
            uint32_t    max_frag_size;
            uint8_t     reserved[124]; /* must be filled with zeroes */
        }       block;
        uint8_t     reserved[128];     /* must be filled with zeroes */
    }       buf;
    int16_t         msbits_per_sample;
    int16_t         pad1;
    int32_t         mixer_device;
    snd_mixer_eid_t *mixer_eid;
    snd_mixer_gid_t *mixer_gid;
    uint8_t         mmap_valid:1;
    uint8_t         mmap_active:1;
    int32_t         mixer_card;
    uint8_t         reserved[104];     /* must be filled with zeroes */
}       snd_pcm_channel_setup_t;

#define		SND_PCM_CHANNEL_PLAYBACK		0
#define		SND_PCM_CHANNEL_CAPTURE			1

#define		SND_PCM_SFMT_U8						0
#define		SND_PCM_SFMT_S8						1
#define		SND_PCM_SFMT_U16_LE					2
#define		SND_PCM_SFMT_U16_BE					3
#define		SND_PCM_SFMT_S16_LE					4
#define		SND_PCM_SFMT_S16_BE					5
#define		SND_PCM_SFMT_U24_LE					6
#define		SND_PCM_SFMT_U24_BE					7
#define		SND_PCM_SFMT_S24_LE					8
#define		SND_PCM_SFMT_S24_BE					9
#define		SND_PCM_SFMT_U32_LE					10
#define		SND_PCM_SFMT_U32_BE					11
#define		SND_PCM_SFMT_S32_LE					12
#define		SND_PCM_SFMT_S32_BE					13
#define		SND_PCM_SFMT_FLOAT_LE				19
#define		SND_PCM_SFMT_FLOAT_BE				20

#define SND_PCM_OPEN_PLAYBACK		0x0001
#define SND_PCM_OPEN_CAPTURE		0x0002

#define		SND_PCM_STATUS_READY			1
#define		SND_PCM_STATUS_UNDERRUN			4

#define		SND_PCM_STOP_STOP				1
#define		SND_PCM_START_DATA					1
#define		SND_PCM_MODE_BLOCK					1 /* required */