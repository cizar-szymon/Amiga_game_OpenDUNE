/* CPU time per task without taking CPU time: a vertical-blank interrupt
   server records which task was running (SysBase->ThisTask) and whether the
   CPU was idle (interrupted in supervisor mode = exec's idle loop / STOP).
   taskload SECONDS  ->  "% idle", then the busiest tasks.
   (cpuload's idle-priority busy loop locked the Amiga up together with
   NovaCoder's OpenDUNE, 28.10.2026.) */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/execbase.h>
#include <hardware/intbits.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct ExecBase *SysBase;
#define MAXT 64
static struct Task *volatile tsk[MAXT];
static volatile ULONG cnt[MAXT], idle, total, overflow;
/* PCs of the busiest task (taskload SECONDS pc): 1024-byte buckets */
#define MAXPC 512
static volatile ULONG pcb[MAXPC], pcn[MAXPC];
static struct Task *volatile pctask;
static volatile int pcmode;

void tl_sample(UWORD *sp);
void tl_server(void);
__asm__(
    "    .globl _tl_server\n"
    "_tl_server:\n"
    "    move.l  sp,d0\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l  d0,-(sp)\n"
    "    jsr     _tl_sample\n"
    "    addq.l  #4,sp\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    moveq   #0,d0\n"
    "    rts\n");

void tl_sample(UWORD *sp)
{
    int i;
    struct Task *t = SysBase->ThisTask;
    total++;
    /* level 3 autovector frame: SR, PC, 0x006C; S bit set = the CPU was in
       the system (exec's idle STOP) */
    for (i = 0; i < 160; i++) {
        if (sp[i + 3] == 0x006C) {
            if (sp[i] & 0x2000) { idle++; return; }
            if (pcmode && t == pctask) {
                ULONG pc = ((ULONG)sp[i + 1] << 16) | sp[i + 2], b = pc & ~63UL;
                int k;
                for (k = 0; k < MAXPC; k++) {
                    if (pcb[k] == b) { pcn[k]++; break; }
                    if (pcb[k] == 0) { pcb[k] = b; pcn[k] = 1; break; }
                }
            }
            break;
        }
    }
    for (i = 0; i < MAXT; i++) {
        if (tsk[i] == t) { cnt[i]++; return; }
        if (tsk[i] == NULL) { tsk[i] = t; cnt[i] = 1; return; }
    }
    overflow++;
}

int main(int argc, char **argv)
{
    static struct Interrupt is;
    int secs = argc > 1 ? atoi(argv[1]) : 5, i, j;
    is.is_Node.ln_Type = NT_INTERRUPT;
    is.is_Node.ln_Pri = 127;
    is.is_Node.ln_Name = (char *)"taskload";
    is.is_Code = (void (*)(void))tl_server;
    pcmode = argc > 2;
    if (pcmode) {
        /* first find the busiest task for one second */
        AddIntServer(INTB_VERTB, &is);
        Delay(50);
        RemIntServer(INTB_VERTB, &is);
        for (i = 0, j = -1; i < MAXT; i++) if (tsk[i] != NULL && (j < 0 || cnt[i] > cnt[j])) j = i;
        pctask = j >= 0 ? tsk[j] : NULL;
        for (i = 0; i < MAXT; i++) { tsk[i] = NULL; cnt[i] = 0; }
        idle = total = 0;
    }
    AddIntServer(INTB_VERTB, &is);
    Delay(secs * 50);
    RemIntServer(INTB_VERTB, &is);
    printf("samples %lu, idle %lu%%\n", (unsigned long)total, total ? (unsigned long)(idle * 100 / total) : 0);
    for (j = 0; j < 8; j++) {
        int best = -1;
        for (i = 0; i < MAXT; i++) if (tsk[i] != NULL && cnt[i] > 0 && (best < 0 || cnt[i] > cnt[best])) best = i;
        if (best < 0) break;
        Forbid();
        printf("%3lu%%  %s\n", (unsigned long)(cnt[best] * 100 / total),
               tsk[best]->tc_Node.ln_Name ? tsk[best]->tc_Node.ln_Name : "?");
        Permit();
        cnt[best] = 0;
    }
    if (pcmode) {
        printf("PCs of the busiest task, 64-byte blocks:\n");
        for (j = 0; j < 12; j++) {
            int best = -1;
            ULONG all = 0;
            for (i = 0; i < MAXPC; i++) all += pcn[i];
            for (i = 0; i < MAXPC; i++) if (pcn[i] > 0 && (best < 0 || pcn[i] > pcn[best])) best = i;
            if (best < 0 || all == 0) break;
            printf("  %08lx  %lu\n", (unsigned long)pcb[best], (unsigned long)pcn[best]);
            pcn[best] = 0;
        }
    }
    return 0;
}
