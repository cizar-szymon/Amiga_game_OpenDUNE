/*
 * fprovide.h - stand-in for AdPlug's file provider (MakeMusic): the ADL file is
 * already in memory (taken out of SOUND.PAK), adl.cpp reads it through this.
 */
#ifndef MAKEMUSIC_FPROVIDE_H
#define MAKEMUSIC_FPROVIDE_H

#include <string>
#include <string.h>

class binistream
{
public:
	const unsigned char *data;
	unsigned long size, pos;

	unsigned long readInt(int n)	/* little endian, as AdPlug's binio */
	{
		unsigned long v = 0;
		for (int i = 0; i < n; i++) v |= (unsigned long)(pos < size ? data[pos++] : 0) << (8 * i);
		return v;
	}
	void seek(long p) { pos = (unsigned long)p; }
	void readString(char *d, unsigned long n)
	{
		unsigned long k = (pos + n <= size) ? n : size - pos;
		memcpy(d, data + pos, k);
		pos += k;
	}
};

class CFileProvider
{
public:
	CFileProvider() : data(0), size(0) {}
	CFileProvider(const unsigned char *d, unsigned long n) : data(d), size(n) {}

	binistream *open(const std::string &) const
	{
		s.data = data;
		s.size = size;
		s.pos = 0;
		return data ? &s : 0;
	}
	void close(binistream *) const {}
	static bool extension(const std::string &, const std::string &) { return true; }
	static unsigned long filesize(binistream *f) { return f ? f->size : 0; }

private:
	const unsigned char *data;
	unsigned long size;
	mutable binistream s;
};

class CProvider_Filesystem : public CFileProvider {};

#endif
