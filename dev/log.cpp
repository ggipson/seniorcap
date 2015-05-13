//-----------------------------------------------------------------------------
// File: log.cpp
// Author: Grant Gipson
// Date Last Edited: April 13, 2011
// Description: Log file object used for activity logging
//-----------------------------------------------------------------------------
#include "log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

// CAP_Log::open()
// Opens the specified file for logging
void CAP_Log::open(const string& strFile) {
  // check for an already-open log file
  if( fileD != 0 ) {
    errorHandle("a log file is already open. Close it first.");
  }

  // attempt to open given log file; write-only, append mode
  fileD = ::open(strFile.c_str(), O_WRONLY|O_APPEND|O_CREAT,CAP_FILE_MASK);
  if( fileD == -1 ) { // FAIL
    fileD = 0;

    // format error message    
    char sz[256];
    sprintf(sz, "unable to open given log filename. errno: %d", errno);
    errorHandle(sz);
  }
}

// CAP_Log::close()
// Closes log file
void CAP_Log::close() {
  // close file descriptor
  if( ::close(fileD) == -1 ) { // failed
    // format error message    
    char sz[256];
    sprintf(sz, "unable to log file descriptor. errno: %d", errno);
    errorHandle(sz);
  }
}

// CAP_Log::write()
// Writes given message to log if it is allowed by priority
void CAP_Log::write(const char* pszMsg, LogPriority lvl) {
  // make sure log file is open
  if( !fileD ) { // nope
    errorHandle("no log file has been specified");
  }

  // check priority level; LOG_FATAL is the lowest
  if( lvl < LOG_NONE ) { // invalid
    errorHandle("invalid log priority given");
  }
  else if( lvl > writeLvl ) { // not allowed
    return;
  }

  // format timestamp for log entry
  char timestamp[32];
  time_t rawTime;
  tm* fTime = 0;

  if( (rawTime=time(NULL)) == -1 ) { // what just happened???
    errorHandle("unable retrieve time for log entry");
  }
  fTime = localtime(&rawTime);
  if( !strftime(timestamp, 32, "[%m/%d/%Y %H:%M:%S]", fTime) ) {
    errorHandle("unable to format time for log entry");
  }

  // determine character to display based on priority
  char levelCh;
  switch( lvl ) {
  case LOG_NONE:    levelCh='-'; break;
  case LOG_FATAL:   levelCh='F'; break;
  case LOG_ERROR:   levelCh='E'; break;
  case LOG_WARNING: levelCh='W'; break;
  case LOG_INFO:    levelCh='I'; break;
  default:          levelCh='U'; break;
  }

  // combine strings into log entry and write it out
  int entrylen = sizeof(timestamp)+sizeof(levelCh)+strlen(pszMsg);
  char* entry = new char[entrylen+16]; // add a few extra bytes for some space
  if( sprintf(entry, "%s -%c- %s\n", 
        timestamp, levelCh, pszMsg) == -1 )
  {
    errorHandle("unable for format log entry string");
  }

  // entry is finished--now write it
  if( ::write(fileD, entry, strlen(entry)) == -1 ) {
    char sz[256];
    sprintf(sz, "unable to write log entry, errno: %d", errno);
    errorHandle(sz);
  }
}

// CAP_Log::writef()
// Calls above function, but takes formatting options just like printf()
void CAP_Log::writef(const char* pszMsg, LogPriority lvl, ...) {
  // allocate variable arguments list
  va_list args;
  va_start(args, lvl);

  // format log message and pass to write()
  char sz[256];
  vsprintf(sz, pszMsg, args);
  write(sz, lvl);

  // free list
  va_end(args);
}

// CAP_Log::setLeastLogPriority()
// Sets the lowest priority which will be written to log
void CAP_Log::setLeastLogPriority(LogPriority lvl) {
  if( lvl < LOG_FATAL || lvl > LOG_INFO ) {
    char sz[256];
    sprintf(sz, "log priority %d specified is invalid", (int)lvl);
    errorHandle(sz);
  }
  writeLvl = lvl;
}

// CAP_Log::setErrHandler()
// Sets the error handling procedure called my log
void CAP_Log::setErrHandler(PCAP_LogErrHandlerProc newProc) {
  if( newProc != 0 ) {
    errHandler = newProc;
  }
  else {
    throw CAP_LogException("no error handler procedure specified");
  }
}

// CAP_Log::errorHandle()
// Internal error handler
void CAP_Log::errorHandle(const char* pszmsg) {
  // call external error handler if specified
  if( errHandler != 0 ) {
    (*errHandler)(pszmsg);
    return;
  }

  // no error handler, so throw an exception
  throw CAP_LogException(pszmsg);
}
