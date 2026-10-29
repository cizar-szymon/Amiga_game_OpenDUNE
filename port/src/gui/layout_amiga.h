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
extern int Layout_SavedShowFPS(void);
extern void Layout_SetShowFPS(bool on);
extern bool Video_Amiga_ShowFPS(int on);
extern void Layout_UpdateLoadSaveButton(void);
extern void Layout_DrawLoadSaveButton(Screen screenID);
extern void Layout_PanelBlackBelow(void);
extern int g_viewZoom;
extern int g_zoomCopy;
extern int Layout_MaxZoom(void);
extern void Layout_ZoomRequest(int step);
extern void Layout_ZoomCancel(void);
extern void Layout_ZoomTick(void);
extern void Layout_ViewPoint(int16 *x, int16 *y);
extern bool Layout_InView(int x, int y);
extern bool Layout_ZoomCopy(int x, int y, int w, int h);
extern void Layout_ZoomShift(int16 dx, int16 dy);

#endif /* GUI_LAYOUT_AMIGA_H */
