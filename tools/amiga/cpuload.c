/* CPU load from an idle-priority counter: cpuload SECONDS
   Runs a busy loop at priority -127 for SECONDS and prints the loops per
   second; the same run on an idle Amiga gives 100 %, so
   load = 100 - 100 * loops / idle_loops. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <stdio.h>
#include <stdlib.h>
struct Device *TimerBase;
static struct timerequest tr;
int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 5;
    struct EClockVal e0, e1;
    ULONG freq, target;
    volatile unsigned long n = 0;
    struct Task *me = FindTask(NULL);
    LONG oldpri;
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0) != 0) return 20;
    TimerBase = tr.tr_node.io_Device;
    freq = ReadEClock(&e0);
    target = freq * secs;
    oldpri = SetTaskPri(me, -127);
    for (;;) {
        int i;
        for (i = 0; i < 1000; i++) n++;
        ReadEClock(&e1);
        if (e1.ev_lo - e0.ev_lo >= target) break;
    }
    SetTaskPri(me, oldpri);
    printf("loops per second: %lu\n", (unsigned long)(n / secs));
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
