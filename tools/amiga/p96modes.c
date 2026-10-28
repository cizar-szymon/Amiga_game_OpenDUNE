/* The 8-bit (CLUT) Picasso96 screen modes: width x height and name. */
#include <exec/types.h>
#include <libraries/Picasso96.h>
#include <proto/exec.h>
#include <proto/Picasso96.h>
#include <stdio.h>

struct Library *P96Base;

int main(void)
{
	struct List *ml;
	struct P96Mode *m;
	P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2);
	if (P96Base == NULL) return 20;
	ml = p96AllocModeListTags(P96MA_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
	if (ml != NULL) {
		for (m = (struct P96Mode *)ml->lh_Head; m->Node.ln_Succ != NULL; m = (struct P96Mode *)m->Node.ln_Succ) {
			printf("%4ldx%-4ld depth %ld  %s\n", (long)m->Width, (long)m->Height, (long)m->Depth, m->Description);
		}
		p96FreeModeList(ml);
	}
	CloseLibrary(P96Base);
	return 0;
}
