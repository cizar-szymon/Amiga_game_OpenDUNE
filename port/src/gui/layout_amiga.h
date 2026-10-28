/** @file src/gui/layout_amiga.h The large in-game screen (AmigaOS). */

#ifndef GUI_LAYOUT_AMIGA_H
#define GUI_LAYOUT_AMIGA_H

#define LAYOUT_RESOLUTIONS 6
extern const uint16 g_layoutResolutions[LAYOUT_RESOLUTIONS][2];

extern void Layout_Set(bool big);
extern bool Layout_IsBig(void);
extern void Layout_ClampViewport(void);
extern void Layout_ComposeBackground(Screen screenID);
extern bool Layout_SavedResolution(int *w, int *h);
extern bool Layout_SetResolution(int w, int h);
extern void Layout_UpdateLoadSaveButton(void);
extern void Layout_DrawLoadSaveButton(Screen screenID);
extern void Layout_PanelBlackBelow(void);

#endif /* GUI_LAYOUT_AMIGA_H */
