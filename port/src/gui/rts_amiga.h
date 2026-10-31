/** @file src/gui/rts_amiga.h Group selection and right-click orders (AmigaOS). */

#ifndef GUI_RTS_AMIGA_H
#define GUI_RTS_AMIGA_H

struct Unit;

extern uint8 RTS_FilterButtons(uint16 x, uint16 y, uint8 buttons, bool *clickFirst);
extern bool RTS_GetDragRect(int16 *x0, int16 *y0, int16 *x1, int16 *y1);
extern bool RTS_IsInGroup(struct Unit *u);
extern void RTS_Tick(void);
extern void RTS_PauseRequest(void);
extern void RTS_PauseDraw(void);
extern uint16 RTS_EdgeSprite(uint16 def);
extern void RTS_GroupAction(struct Unit *done, uint16 action, uint16 packed);
extern int RTS_GroupSize(void);
extern uint16 RTS_GroupHealthPercent(void);
extern int16 RTS_DrawSquadList(int16 x, int16 y);
extern void RTS_PlaceSquadButtons(int16 y);

#endif /* GUI_RTS_AMIGA_H */
