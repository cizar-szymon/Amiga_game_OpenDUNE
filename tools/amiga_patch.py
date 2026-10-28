"""One-time edits of the shared OpenDUNE sources in port/src for AmigaOS
(all under __amigaos__). Each replacement must match exactly once."""
import sys, os

SRC = os.path.join(os.path.dirname(__file__), "..", "port", "src")

EDITS = [
    ("file.c",
     '#if defined(TOS) || defined(DOS)\n#define DUNE_DATA_DIR "DATA"\n',
     '#if defined(TOS) || defined(DOS)\n#define DUNE_DATA_DIR "DATA"\n#elif defined(__amigaos__)\n#define DUNE_DATA_DIR "PROGDIR:data"\n'),
    ("file.c",
     '''		i = snprintf(buf, len, "%s\\\\%s", g_personal_data_dir, filename);
	}
#else''',
     '''		i = snprintf(buf, len, "%s\\\\%s", g_personal_data_dir, filename);
	}
#elif defined(__amigaos__)
	{
		/* "DH0:" + "x" must not become "DH0:/x" (the parent of the root) */
		const char *d = (dir == SEARCHDIR_PERSONAL_DATA_DIR) ? g_personal_data_dir : g_dune_data_dir;
		size_t n = strlen(d);
		const char *sep = (n == 0 || d[n - 1] == ':' || d[n - 1] == '/') ? "" : "/";
		i = snprintf(buf, len, "%s%s%s", d, sep, filename);
	}
#else'''),
    ("file.c",
     '''#elif defined(TOS) || defined(DOS)
		(void)homedir;
		strcpy(g_personal_data_dir, "SAVES");''',
     '''#elif defined(TOS) || defined(DOS)
		(void)homedir;
		strcpy(g_personal_data_dir, "SAVES");
#elif defined(__amigaos__)
		(void)homedir;
		strcpy(g_personal_data_dir, "PROGDIR:saves");'''),
    ("inifile.c",
     '#elif !defined(TOS) && !defined(DOS)  /* __HAIKU__ */',
     '#elif defined(__amigaos__)\n\tf = fopen("PROGDIR:opendune.ini", "rb");\n#elif !defined(TOS) && !defined(DOS)  /* __HAIKU__ */'),
    ("os/strings.h",
     '#if !defined(__APPLE__) && !defined(TOS) &&',
     '#if !defined(__APPLE__) && !defined(TOS) && !defined(__amigaos__) &&'),
    ("os/endian.h",
     '#elif defined(__HAIKU__)\n\t#include <posix/endian.h>',
     '#elif defined(__amigaos__)\n\t#define __LITTLE_ENDIAN 1234\n\t#define __BIG_ENDIAN 4321\n\t#define __BYTE_ORDER __BIG_ENDIAN\n#elif defined(__HAIKU__)\n\t#include <posix/endian.h>'),
    ("gfx.h",
     '#if defined(TOS)\n#define GFX_STORE_DIRTY_AREA_BLOCKS',
     '#if defined(TOS) || defined(__amigaos__)\n#define GFX_STORE_DIRTY_AREA_BLOCKS'),
    # timer: Amiga has its own clock and a sleeping cooperative tick (os/timer_amiga.c)
    ("timer.c",
     "#if !defined(_MSC_VER) && !defined(TOS) && !defined(__WATCOMC__)",
     "#if !defined(_MSC_VER) && !defined(TOS) && !defined(__WATCOMC__) && !defined(__amigaos__)"),
    ("timer.c",
     "#elif defined(TOS)\n\t/* use the 200 HZ system timer which has a 5ms granularity */",
     "#elif defined(__amigaos__)\n\treturn AmigaTimer_GetMs();\n#elif defined(TOS)\n\t/* use the 200 HZ system timer which has a 5ms granularity */"),
    ("timer.c",
     "static uint32 s_timerLastTime;",
     "static uint32 s_timerLastTime;\n\n#if defined(__amigaos__)\nextern uint32 AmigaTimer_GetMs(void);\nextern void AmigaTimer_WaitMs(uint32 ms);\n#endif"),
    ("timer.c",
     "#if defined(TOS) || defined(DOS)\nvoid SleepAndProcessBackgroundTasks(void)\n{\n\tTimer_InterruptRun(0);\n}",
     """#if defined(__amigaos__)
/* Sleep until the next 120 Hz tick instead of spinning (the CPU is shared
 * with the rest of the system), then run the due timers. */
void SleepAndProcessBackgroundTasks(void)
{
	static uint32 s_next = 0;
	uint32 now = AmigaTimer_GetMs();
	if (s_next == 0 || (int32)(now - s_next) > 100) s_next = now;
	if ((int32)(s_next - now) > 0) AmigaTimer_WaitMs(s_next - now);
	s_next += 8 + ((s_next % 3) == 0 ? 1 : 0);	/* ~8.33 ms */
	Timer_InterruptRun(0);
}

#elif defined(TOS) || defined(DOS)
void SleepAndProcessBackgroundTasks(void)
{
	Timer_InterruptRun(0);
}"""),
    ("timer.c",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__)\n\n/**\n * Suspend",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__) && !defined(__amigaos__)\n\n/**\n * Suspend"),
    ("timer.c",
     "#elif defined(DOS) || defined(__HAIKU__)\n\t/* */",
     "#elif defined(DOS) || defined(__HAIKU__) || defined(__amigaos__)\n\t/* */"),
    ("timer.c",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__)\n\tTimer_InterruptResume();",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__) && !defined(__amigaos__)\n\tTimer_InterruptResume();"),
    ("timer.c",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__)\n\tTimer_InterruptSuspend();",
     "#if !defined(TOS) && !defined(DOS) && !defined(__HAIKU__) && !defined(__amigaos__)\n\tTimer_InterruptSuspend();"),
]

# error.c / error.h: Amiga logs like TOS (file + console only when started from the Shell)
for f in ("os/error.c", "os/error.h"):
    p = os.path.join(SRC, f)
    s = open(p, encoding="latin-1").read()
    s = s.replace("#if defined(TOS) || defined(DOS)", "#if defined(TOS) || defined(DOS) || defined(__amigaos__)")
    open(p, "w", encoding="latin-1", newline="\n").write(s)

ok = True
for f, old, new in EDITS:
    p = os.path.join(SRC, f)
    s = open(p, encoding="latin-1").read()
    n = s.count(old)
    if n != 1:
        print("%s: %d matches for %r" % (f, n, old[:60]))
        ok = False
        continue
    s = s.replace(old, new)
    open(p, "w", encoding="latin-1", newline="\n").write(s)
print("ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
