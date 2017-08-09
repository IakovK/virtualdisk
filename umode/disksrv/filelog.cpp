#include <stdio.h>
#include <atlbase.h>
#include <string>
#include "filelog.h"

FileLog::FileLog (const char *prefix, const char *fn, DWORD logMask)
{
	m_fp = NULL;

	try
	{
		m_logMask = logMask;
		if (!GetModuleFileName (NULL, buffer, MAX_PATH))
			return;
		std::string lfPath (buffer);
		int n = lfPath.find_last_of ('\\');
		m_file = lfPath.substr (0, n+1);
		m_file += fn;
		if (logMask)
			m_fp = fopen (m_file.c_str(), "w");
		m_cs.Init();
	}
	catch(...)
	{
		m_fp = NULL;
	}

}

FileLog::~FileLog()
{
	if (m_fp)
	{
		fclose (m_fp);
	}
}

void FileLog::Format (char *fmt...)
{
	CComCritSecLock <CComCriticalSection> lock (m_cs);
	if (!m_logMask)
		return;
	va_list argptr;
	va_start(argptr, fmt);
	int n = sprintf (buffer, "%s (%d)", m_strPrefix.c_str(), GetCurrentThreadId());
	vsprintf_s (&buffer[n], 1024-n, fmt, argptr);
	OutputDebugString (buffer);
	if (m_fp)
	{
		fprintf (m_fp, "%s", buffer);
		fflush (m_fp);
	}
	printf ("%s", buffer);
}

void FileLog::Release()
{
	delete this;
}
