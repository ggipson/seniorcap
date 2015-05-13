//-----------------------------------------------------------------------------
// File Name: pipe.cpp
// Author: Grant Gipson
// Date Last Edited: April 16, 2011
// Description: Implementation of CAP_Pipe class
//-----------------------------------------------------------------------------
#include "pipe.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
using namespace std;

// CAP_Pipe::CAP_Pipe()
// Class constructor
CAP_Pipe::CAP_Pipe(string strNewName, CAP_Log* plog, int n_waitRead) 
  : strName(strNewName), fileD(0), strPathname(""), mode(PIPE_RDONLY)
{
  if( !plog ) {
    throw CAP_PipeException(EXCPIPE_NOERRLOG);
  }
  errlog = plog;

  /* allocate data buffer (pass up exceptions) */
  data=new CAP_PipeBuffer(PIPE_BUFFER_SIZE, errlog, strName);
}

// CAP_Pipe::~CAP_Pipe()
// Class destructor
CAP_Pipe::~CAP_Pipe() {
  close();
  if( data ) { delete data; }
}

// CAP_Pipe::open()
// Opens pipe's file descriptor assuming it has been created already
void CAP_Pipe::open(void) {
  if( fileD ) { 
    /* already open */
    throw CAP_PipeException(EXCPIPE_ISOPEN);
  }

  // open pipe for use; do not block when opening because 
  // fifos will block by default until something is written
  fileD = ::open(strPathname.c_str(), 
		 mode==PIPE_RDONLY ? O_RDONLY : O_WRONLY|O_NONBLOCK);
  if( fileD==-1 && errno==ENXIO ) {
    errlog->writef("%s pipe cannot be written to because it is closed", 
      LOG_WARNING, strName.c_str());
    fileD=0;
    throw CAP_PipeException(EXCPIPE_ISCLOSED);
  }
  else if( fileD==-1 ) {
    errlog->writef("%s pipe could not be opened. errno: %d", 
		   LOG_ERROR, strName.c_str(), errno);
    fileD=0;
    throw CAP_PipeException(EXCPIPE_OPENFAIL);
  }
  else {
    errlog->writef("%s pipe opened", LOG_INFO, strName.c_str());
  }
}

// CAP_Pipe::create()
// Creates FIFO at given pathname; caller may specify pipe as being 
// either one of two modes
void CAP_Pipe::create(string& pathname, PipeMode _mode) {
  // check current file descriptor
  if( fileD ) {
    errlog->writef("pipe %s is already open", LOG_WARNING, strName.c_str());
    throw CAP_PipeException(EXCPIPE_ISOPEN);
  }

  // check parameters
  if( pathname == "" ) {
    errlog->writef("no pathname specified when opening pipe %s", LOG_ERROR, 
		   strName.c_str());
    throw CAP_PipeException(EXCPIPE_CREATEFAIL);
  }
  if( !(_mode==PIPE_RDONLY || _mode==PIPE_WRONLY) ) {
    errlog->writef("invalid mode specified when opening pipe %s", LOG_ERROR, 
		   strName.c_str());
    throw CAP_PipeException(EXCPIPE_CREATEFAIL);
  }

  // create fifo--unless it already exists
  fileD = mkfifo(pathname.c_str(), CAP_FILE_MASK);
  if( fileD == -1 && errno == EEXIST) {
    /* we're good! */
  }
  else if( fileD == -1 ) {
    errlog->writef("%s pipe could not be created. errno: %d", 
		   LOG_ERROR, strName.c_str(), errno);
    throw CAP_PipeException(EXCPIPE_CREATEFAIL);
  }

  fileD = 0; /* restore value */
  strPathname = pathname;
  mode = _mode;
}

// CAP_Pipe::close()
// Closes pipe's file descriptor
void CAP_Pipe::close() {
  if( !fileD ) { return; } // already closed

  // close file descriptor
  if( ::close(fileD) == -1 ) {
    errlog->writef("Pipe %s could not be closed errno: %d", LOG_ERROR, 
		   strName.c_str(), errno);
  }
  else {
    errlog->writef("pipe %s closed", LOG_INFO, strName.c_str());
    fileD=0;
  }
}

/* CAP_Pipe::read()
   Reads in *length chracters from buffer. If *use_delim is true, then 
   function will stop once *delim is reached or *length has been read. */
void CAP_Pipe::read(string& dest, int length, bool use_delim, char delim) {
  char ch='\0'; /* character read from buffer */
  int numread=0; /* number of characters read */
  bool failed=false; /* throw exception after cleanup */

  /* not sure why you would want to do this... */
  if( length <= 0 ) { dest=""; return; }

  /* allocate buffer for read operation */
  char* dest_tmp=NULL;
  if( !(dest_tmp=(char*)malloc(length+1)) ) { /* one extra for null-t. */
      errlog->writef("Pipe %s failed allocate buffer for read", 
		     LOG_ERROR, strName.c_str());
      throw CAP_PipeException(EXCPIPE_READFAIL);
  }
  memset(dest_tmp, 0, length+1); /* initialize to zero */
  char* dest_pos = dest_tmp; /* position for copying */

  try {

  /* read characters from buffer until length or delim are reached */
  do {
    /* if this is not first loop, then store last character read */
    if( numread > 0 ) {
      *dest_pos = ch;
      dest_pos++; /* advance to next space in buffer */
    }

    /* read next character from buffer */
    try { ch = data->next(); numread++; }
    catch( CAP_PipeException err ) {
      /* read data into buffer and try again (pass exceptions up) */
      if( err.msg == EXCPIPE_BUFFEREMPTY ) {
	open();
	data->read(fileD);
	close();
      }
      else { throw err; }
    }
  } while(numread < length &&
	  ch != (use_delim ? delim : !ch));

  /* any errors while reading into data buffer */
  } catch( CAP_PipeException err ) {
	  failed=true;
  }

  /* copy into destination string and free allocated buffer */
  if( !failed ) {
    dest.assign(dest_tmp);
  }
  free(dest_tmp);

  /* throw exception if an error occurred */
  if( failed ) { throw CAP_PipeException(EXCPIPE_READFAIL); }
}

/* CAP_Pipe::write()
   Writes given string into pipe */
void CAP_Pipe::write(string& src) {
  open();
  if( ::write(fileD, src.c_str(), src.length()) == -1 ) {
    errlog->writef("pipe %s could not write message: %d", LOG_ERROR, 
      strName.c_str(), errno);
    throw CAP_PipeException(EXCPIPE_WRITEFAIL);
  }
  errlog->writef("wrote %d characters to pipe %s", LOG_INFO,
    src.length(), strName.c_str());
  close();
}

/* CAP_Pipe::getMessage()
   Reads in a message header and body */
bool CAP_Pipe::getMessage(CAP_PipeMessage& msg) {
  string strCommand=""; /* first line */
  string strLength="";  /* second line */
  string strBody="";

  /* make sure pipe was created in correct mode */
  if( mode!=PIPE_RDONLY ) {
    throw CAP_PipeException(EXCPIPE_WRONGMODE);
  }

  try {
    read(strCommand, PIPE_LINE_MAX, true);
    read(strLength, PIPE_LINE_MAX, true);

    /* convert length to an integer and read body */
    unsigned long length = atol(strLength.c_str());
    read(strBody, length);

    /* copy into message structure */
    msg.command = strCommand;
    msg.body = strBody;
  }
  catch( CAP_PipeException err ) {
    return false;
  }

  errlog->writef("received message %s", LOG_INFO, msg.command.c_str());
  return true;
}

/* CAP_Pipe::sendMessage()
   Writes given message to pipe */
bool CAP_Pipe::sendMessage(CAP_PipeMessage& msg) {
  /* make sure pipe was created in correct mode */
  if( mode!=PIPE_WRONLY ) {
    throw CAP_PipeException(EXCPIPE_WRONGMODE);
  }

  /* convert body length to a string */
  char* psz = new char[32];
  memset(psz, '\0', 32);
  sprintf(psz, "%d", msg.body.length());

  /* combine strings into a message */
  string message(msg.command);
  message += "\n";
  message += psz;
  message += "\n";
  message += msg.body;
  message += "\n";
  delete psz; /* free buffer */

  /* write message to pipe */
  try { 
    write(message); 
  }
  catch( CAP_PipeException err ) {
    errlog->writef("message sent to pipe %s has been lost", 
      LOG_WARNING, strName.c_str());
    return false;
  }

  errlog->writef("sent message %s", LOG_INFO, msg.command.c_str());
  return true;
}
