/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/*
 * !!! FIXME: streamline this a little by removing all the
 * !!! FIXME:  if (capture) {} else {} sections that are identical
 * !!! FIXME:  except for one flag.
 */

/* !!! FIXME: can this target support hotplugging? */
/* !!! FIXME: ...does SDL2 even support QNX? */

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_QSA

#if __QNX__ < 800

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/select.h>
#include <sys/neutrino.h>

#ifdef DYNLOAD_QNX
#include "qsa_symbol_list.h"
#else
#include <sys/asoundlib.h>
#endif

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../../core/unix/SDL_poll.h"
#include "../SDL_audio_c.h"
#include "SDL_qsa_audio.h"

/* default channel communication parameters */
#define DEFAULT_CPARAMS_RATE   44100
#define DEFAULT_CPARAMS_VOICES 1

#define DEFAULT_CPARAMS_FRAG_SIZE 4096
#define DEFAULT_CPARAMS_FRAGS_MIN 1
#define DEFAULT_CPARAMS_FRAGS_MAX 1

/* List of found devices */
#define QSA_MAX_DEVICES       32
#define QSA_MAX_NAME_LENGTH   81+16     /* Hardcoded in QSA, can't be changed */

typedef struct _QSA_Device
{
    char name[QSA_MAX_NAME_LENGTH];     /* Long audio device name for SDL  */
    int cardno;
    int deviceno;
} QSA_Device;

QSA_Device qsa_playback_device[QSA_MAX_DEVICES];
uint32_t qsa_playback_devices;

QSA_Device qsa_capture_device[QSA_MAX_DEVICES];
uint32_t qsa_capture_devices;

#ifdef DYNLOAD_QNX
static void QSA_dlopen(){
    qsa_dlhandle = dlopen("libasound.so", 0);
    if(!qsa_dlhandle) error(1 ,"Failed to load libasound.so");

    snd_cards               = dlsym(qsa_dlhandle, "snd_cards");
    snd_card_get_name       = dlsym(qsa_dlhandle, "snd_card_get_name");
    snd_card_get_longname   = dlsym(qsa_dlhandle, "snd_card_get_longname");
    snd_close               = dlsym(qsa_dlhandle, "snd_close");
    snd_pcm_file_descriptor = dlsym(qsa_dlhandle, "snd_pcm_file_descriptor");
    snd_pcm_format_width    = dlsym(qsa_dlhandle, "snd_pcm_format_width");
    snd_pcm_open            = dlsym(qsa_dlhandle, "snd_pcm_open");
    snd_pcm_open_preferred  = dlsym(qsa_dlhandle, "snd_pcm_open_preferred");
    snd_pcm_plugin_write    = dlsym(qsa_dlhandle, "snd_pcm_plugin_write");
    snd_pcm_plugin_prepare  = dlsym(qsa_dlhandle, "snd_pcm_plugin_prepare");
    snd_pcm_plugin_drain    = dlsym(qsa_dlhandle, "snd_pcm_plugin_drain");
    snd_pcm_plugin_params   = dlsym(qsa_dlhandle, "snd_pcm_plugin_params");
    snd_pcm_plugin_setup    = dlsym(qsa_dlhandle, "snd_pcm_plugin_setup");
    snd_strerror            = dlsym(qsa_dlhandle, "snd_strerror");
}
#endif

static SDL_INLINE int
QSA_SetError(const char *fn, int status)
{
    return SDL_SetError("QSA: %s() failed: %s", fn, snd_strerror(status));
}

/* !!! FIXME: does this need to be here? Does the SDL version not work? */
static void
QSA_ThreadInit(_THIS)
{
    /* Increase default 10 priority to 25 to avoid jerky sound */
    struct sched_param param;
    if (SchedGet(0, 0, &param) != -1) {
        param.sched_priority = param.sched_curpriority + 15;
        SchedSet(0, 0, SCHED_NOCHANGE, &param);
    }
}

/* PCM channel parameters initialize function */
static void
QSA_InitAudioParams(snd_pcm_channel_params_t * cpars)
{
    SDL_zerop(cpars);
    cpars->channel = SND_PCM_CHANNEL_PLAYBACK;
    cpars->mode = SND_PCM_MODE_BLOCK;
    cpars->start_mode = SND_PCM_START_DATA;
    cpars->stop_mode = SND_PCM_STOP_STOP;
    cpars->format.format = SND_PCM_SFMT_S16_LE;
    cpars->format.interleave = 1;
    cpars->format.rate = DEFAULT_CPARAMS_RATE;
    cpars->format.voices = DEFAULT_CPARAMS_VOICES;
    cpars->buf.block.frag_size = DEFAULT_CPARAMS_FRAG_SIZE;
    cpars->buf.block.frags_min = DEFAULT_CPARAMS_FRAGS_MIN;
    cpars->buf.block.frags_max = DEFAULT_CPARAMS_FRAGS_MAX;
}

/* This function waits until it is possible to write a full sound buffer */
static void
QSA_WaitDevice(_THIS)
{
    int result;

    /* Setup timeout for playing one fragment equal to 2 seconds          */
    /* If timeout occured than something wrong with hardware or driver    */
    /* For example, Vortex 8820 audio driver stucks on second DAC because */
    /* it doesn't exist !                                                 */
    result = SDL_IOReady(this->hidden->audio_fd, !this->hidden->iscapture, 2 * 1000);
    switch (result) {
    case -1:
        SDL_SetError("QSA: SDL_IOReady() failed: %s", strerror(errno));
        break;
    case 0:
        SDL_SetError("QSA: timeout on buffer waiting occured");
        this->hidden->timeout_on_wait = 1;
        break;
    default:
        this->hidden->timeout_on_wait = 0;
        break;
    }
}

static void
QSA_PlayDevice(_THIS)
{
    snd_pcm_channel_status_t cstatus;
    int written;
    int status;
    int towrite;
    void *pcmbuffer;

    if (!SDL_AtomicGet(&this->enabled) || !this->hidden) {
        return;
    }

    towrite = this->spec.size;
    pcmbuffer = this->hidden->pcm_buf;

    /* Write the audio data, checking for EAGAIN (buffer full) and underrun */
    do {
        written =
            snd_pcm_plugin_write(this->hidden->audio_handle, pcmbuffer,
                                 towrite);
        if (written != towrite) {
            /* Check if samples playback got stuck somewhere in hardware or in */
            /* the audio device driver */
            if ((errno == EAGAIN) && (written == 0)) {
                if (this->hidden->timeout_on_wait != 0) {
                    SDL_SetError("QSA: buffer playback timeout");
                    return;
                }
            }

            /* Check for errors or conditions */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* Let a little CPU time go by and try to write again */
                SDL_Delay(1);

                /* if we wrote some data */
                towrite -= written;
                pcmbuffer += written * this->spec.channels;
                continue;
            } else {
                if ((errno == EINVAL) || (errno == EIO)) {
                    SDL_zero(cstatus);
                    if (!this->hidden->iscapture) {
                        cstatus.channel = SND_PCM_CHANNEL_PLAYBACK;
                    } else {
                        cstatus.channel = SND_PCM_CHANNEL_CAPTURE;
                    }

                    status =
                        snd_pcm_plugin_status(this->hidden->audio_handle,
                                              &cstatus);
                    if (status < 0) {
                        QSA_SetError("snd_pcm_plugin_status", status);
                        return;
                    }

                    if ((cstatus.status == SND_PCM_STATUS_UNDERRUN) ||
                        (cstatus.status == SND_PCM_STATUS_READY)) {
                        if (!this->hidden->iscapture) {
                            status =
                                snd_pcm_plugin_prepare(this->hidden->
                                                       audio_handle,
                                                       SND_PCM_CHANNEL_PLAYBACK);
                        } else {
                            status =
                                snd_pcm_plugin_prepare(this->hidden->
                                                       audio_handle,
                                                       SND_PCM_CHANNEL_CAPTURE);
                        }
                        if (status < 0) {
                            QSA_SetError("snd_pcm_plugin_prepare", status);
                            return;
                        }
                    }
                    continue;
                } else {
                    return;
                }
            }
        } else {
            /* we wrote all remaining data */
            towrite -= written;
            pcmbuffer += written * this->spec.channels;
        }
    } while ((towrite > 0) && SDL_AtomicGet(&this->enabled));

    /* If we couldn't write, assume fatal error for now */
    if (towrite != 0) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static Uint8 *
QSA_GetDeviceBuf(_THIS)
{
    return this->hidden->pcm_buf;
}

static void
QSA_CloseDevice(_THIS)
{
    if (this->hidden->audio_handle != NULL) {
        if (!this->hidden->iscapture) {
            /* Finish playing available samples */
            snd_pcm_plugin_drain(this->hidden->audio_handle,
                                 SND_PCM_CHANNEL_PLAYBACK);
        } else {
            /* Cancel unread samples during capture */
            snd_pcm_plugin_drain(this->hidden->audio_handle,
                                 SND_PCM_CHANNEL_CAPTURE);
        }
        snd_pcm_close(this->hidden->audio_handle);
    }

    SDL_free(this->hidden->pcm_buf);
    SDL_free(this->hidden);
}

static int
QSA_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    const QSA_Device *device = (const QSA_Device *) handle;
    int status = 0;
    int format = 0;
    SDL_AudioFormat test_format = 0;
    int found = 0;
    snd_pcm_channel_setup_t csetup;
    snd_pcm_channel_params_t cparams;

    /* Initialize all variables that we clean on shutdown */
    this->hidden =
        (struct SDL_PrivateAudioData *) SDL_calloc(1,
                                                   (sizeof
                                                    (struct
                                                     SDL_PrivateAudioData)));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    /* Initialize channel transfer parameters to default */
    QSA_InitAudioParams(&cparams);

    /* Initialize channel direction: capture or playback */
    this->hidden->iscapture = iscapture ? SDL_TRUE : SDL_FALSE;

    if (device != NULL) {
        /* Open requested audio device */
        this->hidden->deviceno = device->deviceno;
        this->hidden->cardno = device->cardno;
        status = snd_pcm_open(&this->hidden->audio_handle,
                              device->cardno, device->deviceno,
                              iscapture ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK);
    } else {
        /* Open system default audio device */
        status = snd_pcm_open_preferred(&this->hidden->audio_handle,
                                        &this->hidden->cardno,
                                        &this->hidden->deviceno,
                                        iscapture ? SND_PCM_OPEN_CAPTURE : SND_PCM_OPEN_PLAYBACK);
    }

    /* Check if requested device is opened */
    if (status < 0) {
        this->hidden->audio_handle = NULL;
        return QSA_SetError("snd_pcm_open", status);
    }

    /* Try for a closest match on audio format */
    format = 0;
    /* can't use format as SND_PCM_SFMT_U8 = 0 in qsa */
    found = 0;

    for (test_format = SDL_FirstAudioFormat(this->spec.format); !found;) {
        /* if match found set format to equivalent QSA format */
        switch (test_format) {
        case AUDIO_U8:
            {
                format = SND_PCM_SFMT_U8;
                found = 1;
            }
            break;
        case AUDIO_S8:
            {
                format = SND_PCM_SFMT_S8;
                found = 1;
            }
            break;
        case AUDIO_S16LSB:
            {
                format = SND_PCM_SFMT_S16_LE;
                found = 1;
            }
            break;
        case AUDIO_S16MSB:
            {
                format = SND_PCM_SFMT_S16_BE;
                found = 1;
            }
            break;
        case AUDIO_U16LSB:
            {
                format = SND_PCM_SFMT_U16_LE;
                found = 1;
            }
            break;
        case AUDIO_U16MSB:
            {
                format = SND_PCM_SFMT_U16_BE;
                found = 1;
            }
            break;
        case AUDIO_S32LSB:
            {
                format = SND_PCM_SFMT_S32_LE;
                found = 1;
            }
            break;
        case AUDIO_S32MSB:
            {
                format = SND_PCM_SFMT_S32_BE;
                found = 1;
            }
            break;
        case AUDIO_F32LSB:
            {
                format = SND_PCM_SFMT_FLOAT_LE;
                found = 1;
            }
            break;
        case AUDIO_F32MSB:
            {
                format = SND_PCM_SFMT_FLOAT_BE;
                found = 1;
            }
            break;
        default:
            {
                break;
            }
        }

        if (!found) {
            test_format = SDL_NextAudioFormat();
        }
    }

    /* assumes test_format not 0 on success */
    if (test_format == 0) {
        return SDL_SetError("QSA: Couldn't find any hardware audio formats");
    }

    this->spec.format = test_format;

    /* Set the audio format */
    cparams.format.format = format;

    /* Set mono/stereo/4ch/6ch/8ch audio */
    cparams.format.voices = this->spec.channels;

    /* Set rate */
    cparams.format.rate = this->spec.freq;

    /* Setup the transfer parameters according to cparams */
    status = snd_pcm_plugin_params(this->hidden->audio_handle, &cparams);
    if (status < 0) {
        return QSA_SetError("snd_pcm_plugin_params", status);
    }

    /* Make sure channel is setup right one last time */
    SDL_zero(csetup);
    if (!this->hidden->iscapture) {
        csetup.channel = SND_PCM_CHANNEL_PLAYBACK;
    } else {
        csetup.channel = SND_PCM_CHANNEL_CAPTURE;
    }

    /* Setup an audio channel */
    if (snd_pcm_plugin_setup(this->hidden->audio_handle, &csetup) < 0) {
        return SDL_SetError("QSA: Unable to setup channel");
    }

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&this->spec);

    this->hidden->pcm_len = this->spec.size;

    if (this->hidden->pcm_len == 0) {
        this->hidden->pcm_len =
            csetup.buf.block.frag_size * this->spec.channels *
            (snd_pcm_format_width(format) / 8);
    }

    /*
     * Allocate memory to the audio buffer and initialize with silence
     *  (Note that buffer size must be a multiple of fragment size, so find
     *  closest multiple)
     */
    this->hidden->pcm_buf =
        (Uint8 *) SDL_malloc(this->hidden->pcm_len);
    if (this->hidden->pcm_buf == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden->pcm_buf, this->spec.silence,
               this->hidden->pcm_len);

    /* get the file descriptor */
    if (!this->hidden->iscapture) {
        this->hidden->audio_fd =
            snd_pcm_file_descriptor(this->hidden->audio_handle,
                                    SND_PCM_CHANNEL_PLAYBACK);
    } else {
        this->hidden->audio_fd =
            snd_pcm_file_descriptor(this->hidden->audio_handle,
                                    SND_PCM_CHANNEL_CAPTURE);
    }

    if (this->hidden->audio_fd < 0) {
        return QSA_SetError("snd_pcm_file_descriptor", status);
    }

    /* Prepare an audio channel */
    if (!this->hidden->iscapture) {
        /* Prepare audio playback */
        status =
            snd_pcm_plugin_prepare(this->hidden->audio_handle,
                                   SND_PCM_CHANNEL_PLAYBACK);
    } else {
        /* Prepare audio capture */
        status =
            snd_pcm_plugin_prepare(this->hidden->audio_handle,
                                   SND_PCM_CHANNEL_CAPTURE);
    }

    if (status < 0) {
        return QSA_SetError("snd_pcm_plugin_prepare", status);
    }

    /* We're really ready to rock and roll. :-) */
    return 0;
}

static void
QSA_DetectDevices(void)
{
    uint32_t it;
    uint32_t cards;
    uint32_t devices;
    int32_t status;

    /* Detect amount of available devices       */
    /* this value can be changed in the runtime */
    cards = snd_cards();

    /* If io-audio manager is not running we will get 0 as number */
    /* of available audio devices                                 */
    if (cards == 0) {
        /* We have no any available audio devices */
        return;
    }

    /* !!! FIXME: code duplication */
    /* Find requested devices by type */
    {  /* output devices */
        /* Playback devices enumeration requested */
        for (it = 0; it < cards; it++) {
            devices = 0;
            do {
                status =
                    snd_card_get_longname(it,
                                          qsa_playback_device
                                          [qsa_playback_devices].name,
                                          QSA_MAX_NAME_LENGTH);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    sprintf(qsa_playback_device[qsa_playback_devices].name +
                            SDL_strlen(qsa_playback_device
                                       [qsa_playback_devices].name), " d%d",
                            devices);

                    /* Store associated card number id */
                    qsa_playback_device[qsa_playback_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, it, devices,
                                     SND_PCM_OPEN_PLAYBACK);
                    if (status == EOK) {
                        qsa_playback_device[qsa_playback_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            SDL_AddAudioDevice(SDL_FALSE, qsa_playback_device[qsa_playback_devices].name, &qsa_playback_device[qsa_playback_devices]);
                            qsa_playback_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }
                } else {
                    break;
                }

                /* Check if we reached maximum devices count */
                if (qsa_playback_devices >= QSA_MAX_DEVICES) {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_playback_devices >= QSA_MAX_DEVICES) {
                break;
            }
        }
    }

    {  /* capture devices */
        /* Capture devices enumeration requested */
        for (it = 0; it < cards; it++) {
            devices = 0;
            do {
                status =
                    snd_card_get_longname(it,
                                          qsa_capture_device
                                          [qsa_capture_devices].name,
                                          QSA_MAX_NAME_LENGTH);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    sprintf(qsa_capture_device[qsa_capture_devices].name +
                            SDL_strlen(qsa_capture_device
                                       [qsa_capture_devices].name), " d%d",
                            devices);

                    /* Store associated card number id */
                    qsa_capture_device[qsa_capture_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, it, devices,
                                     SND_PCM_OPEN_CAPTURE);
                    if (status == EOK) {
                        qsa_capture_device[qsa_capture_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            SDL_AddAudioDevice(SDL_TRUE, qsa_capture_device[qsa_capture_devices].name, &qsa_capture_device[qsa_capture_devices]);
                            qsa_capture_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }

                    /* Check if we reached maximum devices count */
                    if (qsa_capture_devices >= QSA_MAX_DEVICES) {
                        break;
                    }
                } else {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_capture_devices >= QSA_MAX_DEVICES) {
                break;
            }
        }
    }
}

static void
QSA_Deinitialize(void)
{
    /* Clear devices array on shutdown */
    /* !!! FIXME: we zero these on init...any reason to do it here? */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;
}

static int
QSA_Init(SDL_AudioDriverImpl * impl)
{
    #ifdef DYNLOAD_QNX
    QSA_dlopen();
    #endif
    /* Clear devices array */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;

    /* Set function pointers                                     */
    /* DeviceLock and DeviceUnlock functions are used default,   */
    /* provided by SDL, which uses pthread_mutex for lock/unlock */
    impl->DetectDevices = QSA_DetectDevices;
    impl->OpenDevice = QSA_OpenDevice;
    impl->ThreadInit = QSA_ThreadInit;
    impl->WaitDevice = QSA_WaitDevice;
    impl->PlayDevice = QSA_PlayDevice;
    impl->GetDeviceBuf = QSA_GetDeviceBuf;
    impl->CloseDevice = QSA_CloseDevice;
    impl->Deinitialize = QSA_Deinitialize;
    impl->LockDevice = NULL;
    impl->UnlockDevice = NULL;

    impl->ProvidesOwnCallbackThread = 0;
    impl->SkipMixerLock = 0;
    impl->HasCaptureSupport = 1;
    impl->OnlyHasDefaultOutputDevice = 0;
    impl->OnlyHasDefaultCaptureDevice = 0;

    return 1;   /* this audio target is available. */
}

AudioBootStrap QSAAUDIO_bootstrap = {
    "qsa", "QNX QSA Audio", QSA_Init, 0
};

#else /* __QNX__ < 800 */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/select.h>
#include <sys/neutrino.h>
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../../core/unix/SDL_poll.h"
#include "../SDL_audio_c.h"
#include "SDL_qsa_audio.h"

/* default channel communication parameters */
#define DEFAULT_HWPARAMS_RATE   44100
#define DEFAULT_HWPARAMS_VOICES 1

#define DEFAULT_HWPARAMS_PERIOD_SIZE 4096
#define DEFAULT_HWPARAMS_PERIODS_MIN 1
#define DEFAULT_HWPARAMS_PERIODS_MAX 1

/* List of found devices */
#define QSA_800_MAX_DEVICES       32
#define QSA_800_MAX_NAME_LENGTH   81+16     /* Hardcoded in QSA, can't be changed */

typedef struct _QSA_800_Device
{
    char name[QSA_800_MAX_NAME_LENGTH];     /* Long audio device name for SDL  */
    int32_t cardno;
    int32_t deviceno;
} QSA_800_Device;

QSA_800_Device qsa_playback_device[QSA_800_MAX_DEVICES];
uint32_t qsa_playback_devices;

QSA_800_Device qsa_capture_device[QSA_800_MAX_DEVICES];
uint32_t qsa_capture_devices;

static SDL_INLINE int
QSA_800_SetError(const char *fn, int status)
{
    return SDL_SetError("QSA: %s() failed: %s", fn, snd_strerror(status));
}

static void
QSA_800_ThreadInit(_THIS)
{
    /* Increase default 10 priority to 25 to avoid jerky sound */
    struct sched_param param;
    if (SchedGet(0, 0, &param) != -1) {
        param.sched_priority = param.sched_curpriority + 15;
        SchedSet(0, 0, SCHED_NOCHANGE, &param);
    }
}

/* PCM hardware parameters initialize function */
static int
QSA_800_InitAudioParams(snd_pcm_t *handle, snd_pcm_hw_params_t *hw_params,
                        snd_pcm_sw_params_t *sw_params)
{
    int status;

    /* Passed by ref. */
    unsigned int periods_min = DEFAULT_HWPARAMS_PERIODS_MIN;
    unsigned int periods_max = DEFAULT_HWPARAMS_PERIODS_MAX;
    int min_dir = 0;
    int max_dir = 0;

	/* Get current hardware parameters. */
	status = snd_pcm_hw_params_current(handle, hw_params);
	if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_current", -status);
	}

    /* Initialize hardware parameters. */
    /* Round down on period size so we can use the default for later
     * calculations. */
    status = snd_pcm_hw_params_set_period_size(handle, hw_params,
                                               DEFAULT_HWPARAMS_PERIOD_SIZE, 0);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_set_period_size", -status);
    }

    status = snd_pcm_hw_params_set_periods_minmax(handle, hw_params,
                                                  &periods_min, &min_dir,
                                                  &periods_max, &max_dir);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_set_periods_minmax", -status);
    }

    status = snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_set_format", -status);
    }

    status = snd_pcm_hw_params_set_channels(handle, hw_params,
                                            DEFAULT_HWPARAMS_VOICES);
	if (status < 0) {
		return QSA_800_SetError("snd_pcm_hw_params_set_channels", -status);
	}

	status = snd_pcm_hw_params_set_rate(handle, hw_params, DEFAULT_HWPARAMS_RATE,
                                     0);
	if (status < 0) {
		return QSA_800_SetError("snd_pcm_hw_params_set_rate", -status);
	}

	/* Get current software parameters. */
	status = snd_pcm_sw_params_current(handle, sw_params);
	if (status < 0) {
        return QSA_800_SetError("snd_pcm_sw_params_current", -status);
	}
}

static void
QSA_800_WaitDevice(_THIS)
{
    const snd_pcm_sframes_t needed = (snd_pcm_sframes_t) this->spec.samples;
    while (SDL_AtomicGet(&this->enabled)) {
        const snd_pcm_sframes_t rc = snd_pcm_avail(this->hidden->audio_handle);
        if ((rc < 0) && (rc != -EAGAIN)) {
            /* Hmm, not much we can do - abort */
            QSA_800_SetError("snd_pcm_avail", -rc);
            SDL_OpenedAudioDeviceDisconnected(this);
            return;
        } else if (rc < needed) {
            const Uint32 delay = ((needed - (SDL_max(rc, 0))) * 1000) / this->spec.freq;
            SDL_Delay(SDL_max(delay, 10));
        } else {
            break;  /* ready to go! */
        }
    }
}

static void
QSA_800_PlayDevice(_THIS)
{
    int written;
    int status;
    int towrite;
    void *pcmbuffer;

    if (!SDL_AtomicGet(&this->enabled) || !this->hidden) {
        return;
    }

    towrite = this->spec.size;
    pcmbuffer = this->hidden->pcm_buf;

    /* Write the audio data, checking for EAGAIN (buffer full) and underrun */
    do {
        written = snd_pcm_writei(this->hidden->audio_handle, pcmbuffer,
                                 towrite);
        if (written < 0) {
            status = written;
            /* Check for errors or conditions */
            if ((-status == EAGAIN) || (-status == EWOULDBLOCK)) {
                /* Let a little CPU time go by and try to write again */
                SDL_Delay(1);

                /* if we wrote some data */
                towrite -= written;
                pcmbuffer += written * this->spec.channels;
                continue;
            }
            status = snd_pcm_recover(this->hidden->audio_handle, status, 0);
            if (status < 0) {
                /* Hmm, not much we can do - abort */
                QSA_800_SetError("snd_pcm_recover", -status);
                SDL_OpenedAudioDeviceDisconnected(this);
                return;
            }
            continue;
        } else if (written == 0) {
            Uint32 delay = ((this->spec.samples * 1000) / this->spec.freq) * 2;
            SDL_Delay(delay);
        } else {
            /* we wrote all remaining data */
            towrite -= written;
            pcmbuffer += written * this->spec.channels;
        }
    } while ((towrite > 0) && SDL_AtomicGet(&this->enabled));

    /* If we couldn't write, assume fatal error for now */
    if (towrite != 0) {
        SDL_OpenedAudioDeviceDisconnected(this);
    }
}

static Uint8 *
QSA_800_GetDeviceBuf(_THIS)
{
    return this->hidden->pcm_buf;
}

static void
QSA_800_CloseDevice(_THIS)
{
    if (this->hidden->audio_handle != NULL) {
        snd_pcm_close(this->hidden->audio_handle);
    }
    if (this->hidden->pcm_buf != NULL) {
        SDL_free(this->hidden->pcm_buf);
    }
    if (this->hidden->hw_params != NULL) {
        SDL_free(this->hidden->hw_params);
    }
    if (this->hidden->sw_params != NULL) {
        SDL_free(this->hidden->sw_params);
    }

    SDL_free(this->hidden);
}

static int
QSA_800_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    const QSA_800_Device *device = (const QSA_800_Device *) handle;
    int status = 0;
    snd_pcm_format_t format = 0;
    SDL_AudioFormat test_format = 0;
    ssize_t buffer_bytes;

    /* Initialize all variables that we clean on shutdown */
    this->hidden =
        (struct SDL_PrivateAudioData *) SDL_calloc(1,
                                                   (sizeof
                                                    (struct
                                                     SDL_PrivateAudioData)));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    if (this->hidden->hw_params == NULL) {
        snd_pcm_hw_params_alloca(&this->hidden->hw_params);
    }
    if (this->hidden->sw_params == NULL) {
        snd_pcm_sw_params_alloca(&this->hidden->sw_params);
    }

    /* Initialize channel direction: capture or playback */
    this->hidden->iscapture = iscapture ? SDL_TRUE : SDL_FALSE;

    if (device != NULL) {
        /* Open requested audio device */
        this->hidden->deviceno = device->deviceno;
        this->hidden->cardno = device->cardno;
        status = snd_pcm_open(&this->hidden->audio_handle, device->name,
                              iscapture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                              0);

        /* Check if requested device is opened */
        if (status < 0) {
            this->hidden->audio_handle = NULL;
            return QSA_800_SetError("snd_pcm_open", -status);
        }
    } else {
        snd_pcm_info_t *pcm_info;

        /* Open preferred audio device */
        status = snd_pcm_open(&this->hidden->audio_handle, "pcmPreferred",
                              iscapture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                              0);

        /* Check if requested device is opened */
        if (status < 0) {
            this->hidden->audio_handle = NULL;
            return QSA_800_SetError("snd_pcm_open", -status);
        }

        /* Query pcm info */
        snd_pcm_info_alloca(&pcm_info);
        status = snd_pcm_info(this->hidden->audio_handle, &pcm_info);
        if (status < 0) {
            free(pcm_info);
            return QSA_800_SetError("snd_pcm_info", -status);
        }

        this->hidden->deviceno = snd_pcm_info_get_id(pcm_info);
        this->hidden->cardno = snd_pcm_info_get_card(pcm_info);
        free(pcm_info);
    }

    /* Initialize hardware parameters to default */
    QSA_800_InitAudioParams(this->hidden->audio_handle, this->hidden->hw_params,
                            this->hidden->sw_params);

    /* Try for a closest match on audio format */
    status = -1;
    for (test_format = SDL_FirstAudioFormat(this->spec.format);
         test_format && (status < 0);) {
        status = 0;             /* if we can't support a format, it'll become -1. */
        switch (test_format) {
        case AUDIO_U8:
            format = SND_PCM_FORMAT_U8;
            break;
        case AUDIO_S8:
            format = SND_PCM_FORMAT_S8;
            break;
        case AUDIO_S16LSB:
            format = SND_PCM_FORMAT_S16_LE;
            break;
        case AUDIO_S16MSB:
            format = SND_PCM_FORMAT_S16_BE;
            break;
        case AUDIO_U16LSB:
            format = SND_PCM_FORMAT_U16_LE;
            break;
        case AUDIO_U16MSB:
            format = SND_PCM_FORMAT_U16_BE;
            break;
        case AUDIO_S32LSB:
            format = SND_PCM_FORMAT_S32_LE;
            break;
        case AUDIO_S32MSB:
            format = SND_PCM_FORMAT_S32_BE;
            break;
        case AUDIO_F32LSB:
            format = SND_PCM_FORMAT_FLOAT_LE;
            break;
        case AUDIO_F32MSB:
            format = SND_PCM_FORMAT_FLOAT_BE;
            break;
        default:
            status = -1;
            break;
        }
        if (status >= 0) {
            status = snd_pcm_hw_params_set_format(this->hidden->audio_handle,
                                                  this->hidden->hw_params,
                                                  format);
        }
        if (status < 0) {
            test_format = SDL_NextAudioFormat();
        }
    }
    if (status < 0) {
        return QSA_800_SetError("Couldn't find any hardware audio formats", -status);
    }
    this->spec.format = test_format;

    /* Set mono/stereo/4ch/6ch/8ch audio */
    status = snd_pcm_hw_params_set_channels(this->hidden->audio_handle,
                                            this->hidden->hw_params,
                                            this->spec.channels);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_set_channels", -status);
    }

    /* Set rate */
    status = snd_pcm_hw_params_set_rate(this->hidden->audio_handle,
                                        this->hidden->hw_params,
                                        this->spec.freq, 0);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params_set_rate", -status);
    }

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&this->spec);

    this->hidden->pcm_len = this->spec.size;
    if (this->hidden->pcm_len == 0) {
        this->hidden->pcm_len =
            DEFAULT_HWPARAMS_PERIOD_SIZE * this->spec.channels *
            (snd_pcm_format_width(format) / 8);
    }

    /*
     * Allocate memory to the audio buffer and initialize with silence
     *  (Note that buffer size must be a multiple of fragment size, so find
     *  closest multiple)
     */
    this->hidden->pcm_buf =
        (Uint8 *) SDL_malloc(this->hidden->pcm_len);
    if (this->hidden->pcm_buf == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden->pcm_buf, this->spec.silence,
               this->hidden->pcm_len);

    /* Write the options to their respective component. */
	status = snd_pcm_hw_params(this->hidden->audio_handle,
                               this->hidden->hw_params);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_hw_params", status);
    }

	status = snd_pcm_sw_params(this->hidden->audio_handle,
                               this->hidden->sw_params);
    if (status < 0) {
        return QSA_800_SetError("snd_pcm_sw_params", status);
    }

    /* We're really ready to rock and roll. :-) */
    return 0;
}

static void
QSA_800_DetectDevices(void)
{
    int32_t it = -1;
    int32_t devices;
    int32_t status;

    /* Find requested devices by type */
    {  /* output devices */
        /* Playback devices enumeration requested */
        for (snd_card_next(&it); it != -1; snd_card_next(&it)) {
            devices = 0;
            do {
                char *name;
                status = snd_card_get_longname(it, &name);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    snprintf(qsa_playback_device[qsa_playback_devices].name,
                             QSA_800_MAX_NAME_LENGTH, "%s d%d", name, devices);
                    free(name);

                    /* Store associated card number id */
                    qsa_playback_device[qsa_playback_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, qsa_playback_device
                                     [qsa_playback_devices].name,
                                     SND_PCM_STREAM_PLAYBACK, 0);
                    if (status == EOK) {
                        qsa_playback_device[qsa_playback_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            SDL_AddAudioDevice(SDL_FALSE, qsa_playback_device[qsa_playback_devices].name, &qsa_playback_device[qsa_playback_devices]);
                            qsa_playback_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }
                } else {
                    break;
                }

                /* Check if we reached maximum devices count */
                if (qsa_playback_devices >= QSA_800_MAX_DEVICES) {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_playback_devices >= QSA_800_MAX_DEVICES) {
                break;
            }
        }
    }

    {  /* capture devices */
        /* Capture devices enumeration requested */
        for (snd_card_next(&it); it != -1; snd_card_next(&it)) {
            devices = 0;
            do {
                char *name;
                status = snd_card_get_longname(it, &name);
                if (status == EOK) {
                    snd_pcm_t *handle;

                    /* Add device number to device name */
                    snprintf(qsa_capture_device[qsa_capture_devices].name,
                             QSA_800_MAX_NAME_LENGTH, "%s d%d", name, devices);

                    /* Store associated card number id */
                    qsa_capture_device[qsa_capture_devices].cardno = it;

                    /* Check if this device id could play anything */
                    status =
                        snd_pcm_open(&handle, qsa_playback_device
                                     [qsa_playback_devices].name,
                                     SND_PCM_STREAM_CAPTURE, 0);
                    if (status == EOK) {
                        qsa_capture_device[qsa_capture_devices].deviceno =
                            devices;
                        status = snd_pcm_close(handle);
                        if (status == EOK) {
                            SDL_AddAudioDevice(SDL_TRUE, qsa_capture_device[qsa_capture_devices].name, &qsa_capture_device[qsa_capture_devices]);
                            qsa_capture_devices++;
                        }
                    } else {
                        /* Check if we got end of devices list */
                        if (status == -ENOENT) {
                            break;
                        }
                    }

                    /* Check if we reached maximum devices count */
                    if (qsa_capture_devices >= QSA_800_MAX_DEVICES) {
                        break;
                    }
                } else {
                    break;
                }
                devices++;
            } while (1);

            /* Check if we reached maximum devices count */
            if (qsa_capture_devices >= QSA_800_MAX_DEVICES) {
                break;
            }
        }
    }
}

static void
QSA_800_Deinitialize(void)
{
    /* Clear devices array on shutdown */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;
}

static int
QSA_800_Init(SDL_AudioDriverImpl * impl)
{
    /* Clear devices array */
    SDL_zeroa(qsa_playback_device);
    SDL_zeroa(qsa_capture_device);
    qsa_playback_devices = 0;
    qsa_capture_devices = 0;

    /* Set function pointers                                     */
    /* DeviceLock and DeviceUnlock functions are used default,   */
    /* provided by SDL, which uses pthread_mutex for lock/unlock */
    impl->DetectDevices = QSA_800_DetectDevices;
    impl->OpenDevice = QSA_800_OpenDevice;
    impl->ThreadInit = QSA_800_ThreadInit;
    impl->WaitDevice = QSA_800_WaitDevice;
    impl->PlayDevice = QSA_800_PlayDevice;
    impl->GetDeviceBuf = QSA_800_GetDeviceBuf;
    impl->CloseDevice = QSA_800_CloseDevice;
    impl->Deinitialize = QSA_800_Deinitialize;
    impl->LockDevice = NULL;
    impl->UnlockDevice = NULL;

    impl->ProvidesOwnCallbackThread = 0;
    impl->SkipMixerLock = 0;
    impl->HasCaptureSupport = 1;
    impl->OnlyHasDefaultOutputDevice = 0;
    impl->OnlyHasDefaultCaptureDevice = 0;

    return 1;   /* this audio target is available. */
}

AudioBootStrap QSAAUDIO_bootstrap = {
    "qsa", "QNX QSA Audio", QSA_800_Init, 0
};

#endif /* __QNX__ < 800 */
#endif /* SDL_AUDIO_DRIVER_QSA */

/* vi: set ts=4 sw=4 expandtab: */
