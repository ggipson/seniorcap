//-----------------------------------------------------------------------------
// File Name: pipe.h
// Author: Grant Gipson
// Date Last Edited: April 16, 2011
// Description: Pipe class for handling FIFOs in Senior CAP Project
//-----------------------------------------------------------------------------
#ifndef _PIPE_H_
#define _PIPE_H_

#include "master.h"
#include "log.h"
#include <string>
using namespace std;

// modes in which a pipe may be open--mutually exclusive
enum PipeMode {
  PIPE_WRONLY=0,
  PIPE_RDONLY=1
};

/* CAP_Pipe exception constants and exception class */
#define EXCPIPE_NOERRLOG       1
#define EXCPIPE_ISOPEN         2
#define EXCPIPE_OPENFAIL       3
#define EXCPIPE_CREATEFAIL     4
#define EXCPIPE_READFAIL       5
#define EXCPIPE_BUFFEREMPTY    6
#define EXCPIPE_BUFFERFAIL     7
#define EXCPIPE_WRONGMODE      8
#define EXCPIPE_WRITEFAIL      9
#define EXCPIPE_ISCLOSED      10

class CAP_PipeException {
 public:
  inline CAP_PipeException(const int nmsg) : msg(nmsg) {}
  const int msg;
};

// message structure passed through pipes
struct CAP_PipeMessage {
  string command;
  string body;
};

class CAP_PipeBuffer {
 protected:
  char* data;      /* data buffer */
  char* curr;      /* read position in data buffer */
  int size;        /* size of data buffer */
  int used;        /* amount of buffer used */
  CAP_Log* errlog; /* log events are written to */
  string pipename; /* name of pipe passed to errlog */

 public:
	CAP_PipeBuffer(int _size, CAP_Log* _errlog, string _pipename);
	~CAP_PipeBuffer();

	char next();
	void read(int fileD);
};

class CAP_Pipe {
 protected:
  CAP_Log* errlog;      // log events are written to
  int fileD;            // pipe file descriptor
  const string strName; // name of pipe (set by caller)
  string strPathname;   // path name of FIFO
  CAP_PipeBuffer* data; /* data buffer */
  PipeMode mode;        /* mode in which pipe is to be opened */

 public:
  CAP_Pipe(string strNewName, CAP_Log* plog, int n_waitRead=0);
  ~CAP_Pipe();

  void create(string& pathname, PipeMode _mode);
  void open();
  void close();
  void read(string& dest, int length, bool use_delim=false, char delim='\n');
  void write(string& src);
  bool getMessage(CAP_PipeMessage& msg);
  bool sendMessage(CAP_PipeMessage& msg);
  inline const string& getName() const
    { return strName; }
};

#endif /* PIPE_H_ */
