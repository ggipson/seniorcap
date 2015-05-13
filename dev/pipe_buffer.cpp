/*******************************************************************************
    File Name: pipe_buffer.cpp
    Author: Grant Gipson
    Date Last Edited: April 8, 2011
    Description: Implementation of buffer in CAP_Pipe class
*******************************************************************************/

#include "pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "master.h"

/* CAP_Pipe::binit()
   Initializes data buffer */
void CAP_Pipe::binit() {
  /* initialize variables */
  buffer.size = sizeof(char)*PIPE_BUFFER_SIZE;
  buffer.size_used = 0;
  buffer.pos_start = NULL;
  buffer.pos_end = NULL;

  /* allocate data buffer */
  if( !(buffer.data=malloc(buffer.size)) ) {
    errlog->writef("Pipe %s failed to allocate data buffer", LOG_ERROR, 
	 strName.c_str());
  }
  else {
    buffer.pos_start = (char*)buffer.data;
    buffer.pos_end = buffer.pos_start;
  }
}

/* CAP_Pipe::bfree()
   Free data buffer */
void CAP_Pipe::bfree() {
  /* free data buffer */
  if( buffer.data ) { 
    free(buffer.data);
    buffer.data = NULL;
  }
}

/* CAP_Pipe::bread()
   Reads data from pipe into buffer */
bool CAP_Pipe::bread(void) {
#ifdef _DEBUG_BUFFER
  errlog->write("entering CAP_Pipe::bread(void)");
#endif
  /* check data buffer */
  if( !buffer.data ) {
    errlog->writef("Pipe %s cannot perform read because data buffer has not "
		   "been allocated", LOG_ERROR, strName.c_str());
    return false;
  }

#ifdef _DEBUG_BUFFER
  errlog->write("data buffer is not null");
#endif
  /* open pipe for reading--blocks until there is something to be read */
  if( !open() ) {
    /* don't bother reporting an error because open() already did and an 
       additional message would be redundant */
    return false;
  }

#ifdef _DEBUG_BUFFER
  errlog->write("opened pipe");
#endif

  /* read data from pipe into buffer */
#ifdef _DEBUG_BUFFER
  errlog->write("calling ::read()");
#endif
  int ret = ::read(fileD, buffer.data, buffer.size);
#ifdef _DEBUG_BUFFER
  errlog->writef("::read() returned %d", LOG_INFO, ret);
#endif
  if( ret == -1 ) { /* error */
    errlog->writef("Pipe %s failed to read data: %d", LOG_ERROR, 
		   strName.c_str(), errno);
  }
  else {
    /* update amount of buffer used and moveable pointers to it */
    buffer.size_used = ret;
    buffer.pos_start = (char*)(buffer.data);
    buffer.pos_end = (char*)( ((char*)(buffer.data)) +buffer.size_used);
#ifdef _DEBUG_BUFFER
    errlog->write("set buffer variables");
#endif
  }

  /* close pipe until next read */
  close();
#ifdef _DEBUG_BUFFER
  errlog->write("closed pipe, exiting CAP_Pipe::bread()");
#endif
  return ret!=(-1);
}

/* CAP_Pipe::bread(char*, int)
   Reads destl number of characters from buffer into destination buffer */
bool CAP_Pipe::bread(char* dest, const int destl) {
#ifdef _DEBUG_BUFFER
  errlog->write("entering CAP_Pipe::bread(char*,int)");
#endif

  /* read data until caller's request has been satisfied */
  bool ret=true; /* return from calls to read() */
  int read_count=0;

  while( ret ) {
    /* do another read if buffer is empty; otherwise copy over */
    while( (buffer.pos_start!=buffer.pos_end) && (read_count<destl) ) {
      *dest = *(buffer.pos_start);
      dest++;
      buffer.pos_start++;
      read_count++;
    }

    if( read_count==destl ) { /* all done */
#ifdef _DEBUG_BUFFER
      errlog->writef("finished reading %d", LOG_INFO, read_count);
#endif
      return true;
    }

#ifdef _DEBUG_BUFFER
    errlog->write("end of buffer reached; need to perform read");
#endif

    /* read in more data */
    if( !(ret=bread()) ) { return false; }
  }
  return true;
}

/* CAP_Pipe::findNewline()
   Finds newline character in buffer and returns length of string from 
   pointer in buffer through the character
*/
bool CAP_Pipe::findNewline(int* plen) {
  int read_count=0;
  char* pos = buffer.pos_start; /* pointer for searching */
  while( (pos!=buffer.pos_end) && (*pos!='\n') ) {
    pos++;
    read_count++;
  }
  if( pos != buffer.pos_end ) {
    read_count++; /* include newline */
  }
}

/* CAP_Pipe::breadline()
   Reads in an entire line from data buffer; allocates buffer for data which 
   caller is responsible for deallocating. If another read from the pipe is 
   performed and a newline is not found, then function returns whatever 
   contents it read from buffer already. */
bool CAP_Pipe::breadline(char** ppdest) {
#ifdef _DEBUG_BUFFER
  errlog->write("entering CAP_Pipe::breadline(char**)");
#endif

  /* find newline in data buffer */
  bool bFoundNewline = false;

  /* did we find it? */
  if( (pos!=buffer.pos_end) && (*pos=='\n') ) {
    bFoundNewline = true;
  }

#ifdef _DEBUG_BUFFER
  errlog->writef("finished first scan for newline; read_count=%d", 
		 LOG_ERROR, read_count);
#endif

  /* now we know how large the initial destination buffer must be--regardless 
     of whether we are actually finished */
  char* pdest = NULL;
  int dest_size = (read_count ? read_count+1 : 0); /* add null-terminator */
  if( dest_size ) { /* there was something in the buffer */
#ifdef _DEBUG_BUFFER
    errlog->write("buffer was non-empty; preparing to allocate first "
		  "intermediate buffer");
#endif

    /* allocate said buffer (and null-terminator) */
    pdest = (char*)malloc(sizeof(char)*(dest_size+1));
    if( !pdest ) {
      errlog->writef("Pipe %s failed to allocate intermediate buffer while "
		     "reading in line", LOG_ERROR, strName.c_str());
      *ppdest = NULL;
      return false;
    }

#ifdef _DEBUG_BUFFER
    errlog->write("buffer allocated");
#endif

    /* initialize to zero */
    memset(pdest, '\0', dest_size);

#ifdef _DEBUG_BUFFER
    errlog->write("initialized to zero");
#endif

    /* copy data into buffer (subtract null-terminator from size)*/
    if( !bread(pdest, dest_size-1) ) { 
      *ppdest = NULL;
      return false; 
    }

#ifdef _DEBUG_BUFFER
    errlog->write("copied string from first scan");
#endif
  }
  else { /* buffer was empty so go fetch something */
#ifdef _DEBUG_BUFFER
    errlog->write("buffer is empty");
#endif
    if( !bread() ) { return false; }
  }

  /* did we find the newline character? */
  if( !bFoundNewline ) { /* NO */
#ifdef _DEBUG_BUFFER
    errlog->write("did not find newline on first scan");
#endif

    /* look for it one more time */
    pos = buffer.pos_start;
    read_count = 0;

    while( (pos!=buffer.pos_end) && (*pos!='\n') ) {
      pos++;
      read_count++;
    }
    if( pos != buffer.pos_end ) {
      read_count++; /* include newline */
    }

#ifdef _DEBUG_BUFFER
    errlog->write("finished second scan for newline");
#endif

    /* if we didn't find it then fail and return what we already read */
    if( pos == buffer.pos_end ) {
      errlog->writef("Pipe %s master did not find expected newline", 
		     LOG_ERROR, strName.c_str());
      *ppdest = pdest;
      return false;
    }
    else if( *pos == '\n' ) { /* found it */
      bFoundNewline = true;
#ifdef _DEBUG_BUFFER
      errlog->writef("found newline on second scan");
#endif

      /* allocate new buffer to accomodate rest of line */
      char* pdest_old = pdest;
      int dest_size_old = dest_size;

#ifdef _DEBUG_BUFFER
      errlog->write("preparing to allocate new intermediate buffer for "
		    "combined strings");
#endif

      dest_size = dest_size_old+read_count+1; /* add for null-terminator */
      pdest = (char*)malloc(sizeof(char)*dest_size);
      if( !pdest ) {
	errlog->writef("Pipe %s failed to reallocate intermediate buffer "
		       "while reading in line", LOG_ERROR, strName.c_str());

	/* return what we managed to read */
	*ppdest = pdest_old;
	return false;
      }

      /* initialize to zero */
      memset(pdest, '\0', dest_size);

#ifdef _DEBUG_BUFFER
      errlog->write("buffer allocated; initialized to zero");
#endif

      /* copy data from old buffer into new */
      if( pdest_old ) {
#ifdef _DEBUG_BUFFER
	errlog->write("copying first buffer into new");
#endif
	memcpy(pdest, pdest_old, dest_size_old);
      }
#ifdef _DEBUG_BUFFER
      else {
	errlog->write("no first buffer to copy");
      }
#endif

#ifdef _DEBUG_BUFFER
      errlog->write("read in second part of line");
#endif

      /* read in the rest of the line */
      pos = pdest+dest_size_old; /* recycled variable */
      if( !bread(pos, read_count) ) {
	/* return what we got */
	*ppdest = pdest;
	return false;
      }

#ifdef _DEBUG_BUFFER
      errlog->writef("done; read in %s", LOG_INFO, pos);
#endif
    }
#ifdef _DEBUG_BUFFER
    else {
      errlog->write("found newline on first scan");
    }
#endif
  }

  /* found newline character and read in line--now erase newline */
  *( (char*)(pdest+dest_size-2) ) = '\0'; /* end of buffer before null-t. */
  *ppdest = pdest;
#ifdef _DEBUG_BUFFER
  errlog->writef("exiting CAP_Pipe::breadline(char**); *pdest=%s", 
		 LOG_INFO, pdest);
#endif
  return true;
}
