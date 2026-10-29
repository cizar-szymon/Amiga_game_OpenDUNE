/*
 * player.h - stand-in for AdPlug's CPlayer (MakeMusic): only what the ADL
 * player (adl.cpp, adl.h) needs, no AdPlug database.
 */
#ifndef H_ADPLUG_PLAYER
#define H_ADPLUG_PLAYER

#include <string>

#include "fprovide.h"
#include "opl.h"

class CPlayer
{
public:
	CPlayer(Copl *newopl) : opl(newopl) {}
	virtual ~CPlayer() {}

	virtual bool load(const std::string &filename, const CFileProvider &fp = CProvider_Filesystem()) = 0;
	virtual bool update() = 0;
	virtual void rewind(int subsong = -1) = 0;
	virtual float getrefresh() = 0;

	virtual std::string gettype() = 0;
	virtual unsigned int getsubsongs() { return 1; }
	virtual unsigned int getsubsong() { return 0; }

protected:
	Copl *opl;
};

#endif
