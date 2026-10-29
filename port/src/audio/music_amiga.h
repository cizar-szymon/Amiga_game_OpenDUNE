/** @file src/audio/music_amiga.h The music (AmigaOS): recordings played from disk. */

#ifndef AUDIO_MUSIC_AMIGA_H
#define AUDIO_MUSIC_AMIGA_H

extern bool Music_Amiga_Init(void);
extern bool Music_Amiga_Play(const char *name, int track);
extern void Music_Amiga_Tick(void);
extern void Music_Amiga_Stop(void);
extern void Music_Amiga_FadeOut(void);
extern bool Music_Amiga_IsPlaying(void);
extern const char *Music_Amiga_Now(void);

#endif /* AUDIO_MUSIC_AMIGA_H */
