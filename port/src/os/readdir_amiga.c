/** @file src/os/readdir_amiga.c Directory traversal for AmigaOS (dos.library). */

#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>

#include "types.h"
#include "readdir.h"

/**
 * Process all files in the directory.
 *
 * @param dirpath path to directory to traverse.
 * @param callback function called back for each file found.
 * @return True if and only if everything was ok.
 */
bool ReadDir_ProcessAllFiles(const char *dirpath, bool (*callback)(const char *name, const char *path, uint32 size))
{
	struct FileInfoBlock *fib;
	BPTR lock;
	bool ok = true;
	size_t n = strlen(dirpath);
	const char *sep = (n == 0 || dirpath[n - 1] == ':' || dirpath[n - 1] == '/') ? "" : "/";

	lock = Lock((CONST_STRPTR)dirpath, ACCESS_READ);
	if (lock == 0) return false;
	fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
	if (fib == NULL) {
		UnLock(lock);
		return false;
	}
	if (Examine(lock, fib)) {
		while (ExNext(lock, fib)) {
			char path[1024];
			if (fib->fib_DirEntryType >= 0) continue;	/* directory */
			snprintf(path, sizeof(path), "%s%s%s", dirpath, sep, fib->fib_FileName);
			if (!callback((const char *)fib->fib_FileName, path, (uint32)fib->fib_Size)) {
				ok = false;
				break;
			}
		}
	}
	FreeDosObject(DOS_FIB, fib);
	UnLock(lock);
	return ok;
}
