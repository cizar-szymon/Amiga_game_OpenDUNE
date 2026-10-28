/* Mouse and key input through input.device, for programs AmiAgent cannot
   drive (it only sends keys to the active window):
     inject move X Y        pointer to X,Y of the front screen
     inject click X Y       move, left button down and up
     inject rclick X Y      the same with the right button
     inject key CODE        raw key down and up (hex, e.g. 0x45)
     inject rmove DX DY     relative mouse movement in steps of 1 pixel (for
                            programs that capture the mouse and read deltas)
     inject here            left click where the pointer is */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern struct IntuitionBase *IntuitionBase;
static struct MsgPort *port;
static struct IOStdReq *io;

static void send(struct InputEvent *ie)
{
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = ie;
    io->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)io);
}

static void move(int x, int y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;
    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = IntuitionBase->FirstScreen;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_EventAddress = &pp;
    send(&ie);
}

static void button(UWORD code)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    send(&ie);
}

static void rel(int dx, int dy)
{
    struct InputEvent ie;
    while (dx != 0 || dy != 0) {
        int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
        memset(&ie, 0, sizeof(ie));
        ie.ie_Class = IECLASS_RAWMOUSE;
        ie.ie_Code = IECODE_NOBUTTON;
        ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
        ie.ie_X = sx;
        ie.ie_Y = sy;
        send(&ie);
        dx -= sx;
        dy -= sy;
    }
}

static void key(UWORD code)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    send(&ie);
    Delay(5);
    ie.ie_Code = code | IECODE_UP_PREFIX;
    send(&ie);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 10;
    port = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io));
    if (io == NULL || OpenDevice((CONST_STRPTR)"input.device", 0, (struct IORequest *)io, 0) != 0) return 20;
    if (strcmp(argv[1], "here") == 0) {
        /* optional hold time in 1/50 s (busy programs poll the buttons slowly) */
        button(IECODE_LBUTTON);
        Delay(argc > 2 ? atoi(argv[2]) : 5);
        button(IECODE_LBUTTON | IECODE_UP_PREFIX);
    } else if (strcmp(argv[1], "rmove") == 0 && argc >= 4) {
        rel(atoi(argv[2]), atoi(argv[3]));
    } else if (strcmp(argv[1], "key") == 0) {
        key((UWORD)strtol(argv[2], NULL, 0));
    } else if (argc >= 4) {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        move(x, y);
        if (strcmp(argv[1], "click") == 0 || strcmp(argv[1], "rclick") == 0) {
            UWORD b = argv[1][0] == 'r' ? IECODE_RBUTTON : IECODE_LBUTTON;
            Delay(2);
            button(b);
            Delay(5);
            button(b | IECODE_UP_PREFIX);
        }
    }
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    return 0;
}
