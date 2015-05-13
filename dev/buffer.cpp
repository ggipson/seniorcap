/* File Name: buffer.cpp
   Author: Grant Gipson
   Date Last Edited: April 16, 2011
   Description: Implements CAP_PipeBuffer class
*/

#include "master.h"
#include "pipe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/* CAP_PipeBuffer::CAP_PipeBuffer()
   Class constructor */
CAP_PipeBuffer::CAP_PipeBuffer(int _size, CAP_Log* _errlog, string _pipename)
  : data(NULL),curr(NULL),used(0),pipename(_pipename)
{
  /* check parameters */
  if( !_errlog ) { throw CAP_PipeException(EXCPIPE_NOERRLOG); }
  else { errlog=_errlog; }
  
  if( !_size ) { throw CAP_PipeException(EXCPIPE_BUFFEREMPTY); }
  else { size=_size; }
  
  /* allocate data buffer */
  if( !(data=(char*)malloc(size)) ) {
    errlog->writef("Pipe %s failed to allocate data buffer", LOG_ERROR, 
		   pipename.c_str());
    throw CAP_PipeException(EXCPIPE_BUFFERFAIL);
  }
  else {
    curr = data;
  }
}

/* CAP_PipeBuffer::~CAP_PipeBuffer()
   Class destructor */
CAP_PipeBuffer::~CAP_PipeBuffer() {
  /* free data buffer */
  if( data ) { free(data); }
}

/* CAP_PipeBuffer::next()
   Returns next character in buffer */
char CAP_PipeBuffer::next() {
  char ret='\0';
  
  /* check for end of buffer */
  if( curr-data >= used ) {
    throw CAP_PipeException(EXCPIPE_BUFFEREMPTY);
  }
  else {
    ret = *curr; /* return character */
    curr++;      /* advance pointer */
  }
  return ret;
}

/* CAP_PipeBuffer::read()
   Reads data into buffer */
void CAP_PipeBuffer::read(int fileD) {
  /* check buffer and make sure it's empty */
  if( curr-data < used ) {
    errlog->writef("Pipe %s attempted read into non-empty buffer", 
		   LOG_ERROR, pipename.c_str());
    throw CAP_PipeException(EXCPIPE_READFAIL);
  }
  
  /* check file descriptor */
  if( !fileD ) { 
    errlog->writef("Pipe %s attempted to read into buffer without a "
		   "file descriptor", LOG_ERROR, pipename.c_str());
    throw CAP_PipeException(EXCPIPE_READFAIL);
  }
  
  /* read data into buffer */
  curr=data;
  if( (used=::read(fileD, data, size)) == -1 ) { /* error */
    errlog->writef("Pipe %s failed to read data into buffer: %d", 
		   LOG_ERROR, pipename.c_str(), errno);
    used=0;
    throw CAP_PipeException(EXCPIPE_READFAIL);
  }
  
  errlog->writef("Pipe %s read %d bytes into buffer", LOG_INFO, 
		 pipename.c_str(), used);
}
