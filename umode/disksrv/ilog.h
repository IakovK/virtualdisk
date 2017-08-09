#ifndef __ILOG_H__
#define __ILOG_H__

class ILog
{
public:
	virtual void Format (char *fmt...) = 0;
	virtual void Release() = 0;
};

extern ILog *g_pLog;

#endif
