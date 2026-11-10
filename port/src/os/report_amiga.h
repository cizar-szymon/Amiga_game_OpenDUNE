/** @file src/os/report_amiga.h A bug report to the game server (AmigaOS). */

#ifndef OS_REPORT_AMIGA_H
#define OS_REPORT_AMIGA_H

#define REPORT_MAX_FILES 6

/* A process sends the fields and up to REPORT_MAX_FILES files (missing ones
 * are left out). Report_Send: 0 if one is still being sent. Report_Result: 0
 * while it goes on; 1 when done, *ok 1 and text the number of the report, or
 * *ok 0 and text why not. Report_Quit at the end of the program. */
extern int Report_Send(const char *host, int port, const char *version, const char *kind, const char *email,
                       const char *text, const char *machine, const char *const *files, int nfiles);
extern int Report_Result(char *text, int size, int *ok);
extern void Report_Quit(void);
/* 1: a TCP/IP stack is running (its bsdsocket.library in memory) - a report
 * can go; else F10 only saves the files. */
extern int Report_Network(void);

/* gui/report_amiga.c: the window. Report_Open asks for it (kind "F10" or
 * "LAN", the files by their names in the save directory); Report_Poll, in the
 * game's loop on the map screen, shows it. */
extern void Report_Open(const char *kind, const char *const *names, int count);
extern void Report_Poll(void);

#endif /* OS_REPORT_AMIGA_H */
