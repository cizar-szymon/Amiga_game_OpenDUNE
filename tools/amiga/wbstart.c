/* wbstart PATH: starts a program the way a double click on its icon does
 * (workbench.library OpenWorkbenchObjectA, OS 3.5+), for icon-start tests. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/wb.h>

struct Library *WorkbenchBase;

int main(int argc, char **argv)
{
	int rc = 20;
	if (argc < 2) {
		PutStr("wbstart PATH\n");
		return 10;
	}
	WorkbenchBase = OpenLibrary("workbench.library", 44);
	if (WorkbenchBase == NULL) {
		PutStr("workbench.library 44 missing\n");
		return 20;
	}
	if (OpenWorkbenchObjectA((STRPTR)argv[1], NULL)) rc = 0;
	else PutStr("OpenWorkbenchObject failed\n");
	CloseLibrary(WorkbenchBase);
	return rc;
}
