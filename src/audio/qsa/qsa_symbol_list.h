
#ifndef QSA_SYMBOL_LIST_H_
#define QSA_SYMBOL_LIST_H_

#include <stddef.h>
#include "qsa_struct.h"

void* qsa_dlhandle;


int 	(*snd_cards)				(void);
int 	(*snd_card_get_name)		(int, char*, size_t);
int 	(*snd_card_get_longname)	(int, char*, size_t);
int 	(*snd_close)				(snd_pcm_t*);
int 	(*snd_pcm_file_descriptor)	(snd_pcm_t*, int);
int 	(*snd_pcm_format_width)		(int);
int 	(*snd_pcm_open)				(snd_pcm_t**, int, int, int);
int 	(*snd_pcm_open_preferred)	(snd_pcm_t**, int*, int*, int);
int 	(*snd_pcm_plugin_write)		(snd_pcm_t*, const void*, size_t);
int 	(*snd_pcm_plugin_prepare)	(snd_pcm_t*, int);
int 	(*snd_pcm_plugin_drain)		(snd_pcm_t*, int);
int 	(*snd_pcm_plugin_params)	(snd_pcm_t*, snd_pcm_channel_params_t*);
int 	(*snd_pcm_plugin_setup)		(snd_pcm_t*, snd_pcm_channel_setup_t*);
char* 	(*snd_strerror)				(int);

/*
#define QSA_SYMBOLS_LIST 		\
	X(snd_cards)				\
	X(snd_card_get_name)		\
	X(snd_card_get_longname)	\
	X(snd_pcm_close)			\
	X(snd_pcm_file_descriptor)	\
	X(snd_pcm_format_width)		\
	X(snd_pcm_open)				\
	X(snd_pcm_open_preferred)	\
	X(snd_pcm_plugin_write)		\
	X(snd_pcm_plugin_prepare)	\
	X(snd_pcm_plugin_drain)		\
	X(snd_pcm_plugin_params)	\
	X(snd_pcm_plugin_setup)		\
	X(snd_strerror)

#define X(sym) sym##_t ##sym = NULL;
QSA_SYMBOLS_LIST
#undef X
*/
/*
############DEFS
SND_PCM_CHANNEL_PLAYBACK
SND_PCM_CHANNEL_CAPTURE
SND_PCM_STATUS_UNDERRUN
SND_PCM_STATUS_READY
SND_PCM_MODE_BLOCK
SND_PCM_START_DATA
SND_PCM_STOP_STOP
SND_PCM_SFMT_S16_LE
SND_PCM_OPEN_CAPTURE
SND_PCM_OPEN_PLAYBACK
SND_PCM_SFMT_U8
SND_PCM_SFMT_S8
SND_PCM_SFMT_S16_LE
SND_PCM_SFMT_S16_BE
SND_PCM_SFMT_U16_LE
SND_PCM_SFMT_U16_BE
SND_PCM_SFMT_S32_LE
SND_PCM_SFMT_S32_BE
SND_PCM_SFMT_FLOAT_LE
SND_PCM_SFMT_FLOAT_BE
*/
#endif