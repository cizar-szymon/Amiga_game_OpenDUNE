/** @file src/gui/layout_amiga.h The large in-game screen (AmigaOS). */

#ifndef GUI_LAYOUT_AMIGA_H
#define GUI_LAYOUT_AMIGA_H

#define LAYOUT_BIG_WIDTH  640
#define LAYOUT_BIG_HEIGHT 480

extern void Layout_Set(bool big);
extern bool Layout_IsBig(void);
extern void Layout_ClampViewport(void);
extern void Layout_ComposeBackground(Screen screenID);

#endif /* GUI_LAYOUT_AMIGA_H */
