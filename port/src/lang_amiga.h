/** @file src/lang_amiga.h The languages of the Amiga port. */

#ifndef LANG_AMIGA_H
#define LANG_AMIGA_H

extern const char *Lang_T(const char *english);
extern const char *Lang_Name(uint8 language);
extern uint8 Lang_Chosen(void);
extern void Lang_Choose(uint8 language);
extern uint8 Lang_Next(uint8 language);
extern void Lang_Init(void);
extern bool Lang_ApplyChosen(void);

#endif /* LANG_AMIGA_H */
