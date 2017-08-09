#ifndef __FILELOG_H__
#define __FILELOG_H__

#include "ilog.h"

#include <string>

class FileLog : public ILog
{
	std::string m_strPrefix;
	DWORD m_logMask;
	char buffer[1024];
	FILE *m_fp;
	std::string m_file;
	CComCriticalSection m_cs;
	~FileLog ();
public:
	FileLog (const char *prefix, const char *fn, DWORD logMask);
	virtual void Format (char *fmt...);
	virtual void Release();
};

#endif
