//-----------------------------------------------------------------------------
// File Name: master.h
// Author: Grant Gipson
// Date Last Edited: April 16, 2011
// Description: Various constants, data structures, etc... For Master Program
//-----------------------------------------------------------------------------
#ifndef _MASTER_H_
#define _MASTER_H_

#define CAP_FILE_MASK 0666 // file mask for newly-created files
#define CAP_STARTUP_DELAY 2 /* time to wait before interactions with other 
			       components begins */
#define PIPE_BUFFER_SIZE 1000 // size of pipes' read buffers
#define PIPE_LINE_MAX 64 /* maximum length for a line not in message body */
#define PIPE_READ_ERROR_MAX 20 /* max. number of read errors from pipe */

/* general exception class and common exception codes */
#define CAPEXC_NOERRLOG      1
#define CAPEXC_INVALPARAM    2

class CAP_Exception {
 public:
  inline CAP_Exception(const int _msg) : msg(_msg) {}
  const int msg;
};

#endif
