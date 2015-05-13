//-----------------------------------------------------------------------------
// File Name: log.h
// Author: Grant Gipson
// Date Last Edited: March 12, 2011
// Description: Log file object used for activity logging
//-----------------------------------------------------------------------------
#ifndef _LOG_H_
#define _LOG_H_

#include "master.h"
#include <string>
using namespace std;

// function pointer for caller's error handling procedure
typedef void (*PCAP_LogErrHandlerProc)(const char*);

// Log exception class
class CAP_LogException {
 public:
  CAP_LogException(const char* pszmsg)
    : msg(pszmsg) {}

  const string msg;
};

// Log message priority level enumeration
enum LogPriority {
  LOG_NONE=0,   // MUST BE THE LOWEST VALUE!!
  LOG_FATAL=1,
  LOG_ERROR=2,
  LOG_WARNING=3,
  LOG_INFO=4
};

// Log object class
class CAP_Log {
 protected:
  int fileD; // file descriptor for log file
  int writeLvl; // least priority messages which may be written (default 
    // is LOG_FATAL only
  PCAP_LogErrHandlerProc errHandler; // error handling procedure

  void errorHandle(const char* pszmsg);

 public:
  inline CAP_Log(void) : fileD(0), writeLvl(LOG_FATAL), errHandler(0) {}
  inline ~CAP_Log() { close(); }

  void open(const string& strFile);
  void close();
  void write(const char* pszMsg, LogPriority lvl=LOG_INFO);
  void writef(const char* pszMsg, LogPriority lvl=LOG_INFO, ...);
  void setLeastLogPriority(LogPriority lvl);
  void setErrHandler(PCAP_LogErrHandlerProc newProc);
};

#endif // _LOG_H_
