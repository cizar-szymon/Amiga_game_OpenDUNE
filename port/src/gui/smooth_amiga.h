/** @file src/gui/smooth_amiga.h Smooth unit movement on the screen (AmigaOS). */

#ifndef GUI_SMOOTH_AMIGA_H
#define GUI_SMOOTH_AMIGA_H

struct Unit;

extern void Smooth_StepStart(void);
extern tile32 Smooth_Position(struct Unit *u);
extern void Smooth_MarkMoving(void);

#endif /* GUI_SMOOTH_AMIGA_H */
