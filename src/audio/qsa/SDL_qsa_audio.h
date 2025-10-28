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

#include "../../SDL_internal.h"

#ifndef __SDL_QSA_AUDIO_H__
#define __SDL_QSA_AUDIO_H__

#if __QNX__ < 800
#ifdef DYNLOAD_QNX
#include "qsa_symbol_list.h"
#else
#include <sys/asoundlib.h>
#endif

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice* this

struct SDL_PrivateAudioData
{
    /* SDL capture state */
    SDL_bool iscapture;

    /* The audio device handle */
    int cardno;
    int deviceno;
    snd_pcm_t *audio_handle;

    /* The audio file descriptor */
    int audio_fd;

    /* Select timeout status */
    uint32_t timeout_on_wait;

    /* Raw mixing buffer */
    Uint8 *pcm_buf;
    Uint32 pcm_len;
};
#else /* __QNX__ < 800 */
#include <alsa/asoundlib.h>

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the audio functions */
#define _THIS SDL_AudioDevice* this

struct SDL_PrivateAudioData
{
    /* SDL capture state */
    SDL_bool iscapture;

    /* The audio device handle */
    int32_t cardno;
    int32_t deviceno;
    snd_pcm_t *audio_handle;

    /* Select timeout status */
    uint32_t timeout_on_wait;

    /* Configuration parameters */
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;

    /* Raw mixing buffer */
    Uint8 *pcm_buf;
    Uint32 pcm_len;
};
#endif /* __QNX__ < 800 */

#endif /* __SDL_QSA_AUDIO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
