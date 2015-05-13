/*******************************************************************************
  File Name: comp.cpp
  Author: Grant Gipson
  Date Last Edited: April 16, 2011
  Description: Implementation of CAP_Comp class
*******************************************************************************/
#include "comp.h"
#include "log.h"
#include "master.h"
#include <errno.h>
#include <unistd.h>
#include <iostream>
using namespace std;

extern CAP_Log* errlog; /* master.cpp */

/* CAP_Comp::CAP_Comp()
   Class constructor */
CAP_Comp::CAP_Comp(string _name, string& _path) 
  : name(_name), path(_path), pID(0), busy(false)
{
  /* check for error log */
  if( !errlog ) { throw CAP_Exception(CAPEXC_NOERRLOG); }
}

/* CAP_Comp::~CAP_Comp()
   Class destructor */
CAP_Comp::~CAP_Comp() {
  /* component must be stopped! */
  if( isRunning() ) { throw CAP_Exception(CAPEXC_COMPUP); }
}

/* CAP_Comp::isRunning()
   Checks if component is running */
bool CAP_Comp::isRunning() {
  return pID ? true : false;
}

/* CAP_Comp::start()
   Starts component */
void CAP_Comp::start() {
  /* make sure it's not already running */
  if( isRunning() ) {
    errlog->writef("attempted to start %s when it should already be "
      "running", LOG_WARNING, name.c_str());
    return;
  }

  /* create child process which will become component */
  switch( pID=fork() ) {
  case -1:
    errlog->writef("%s could not be forked when starting: %d", LOG_ERROR, 
      name.c_str(), errno);
    pID=0;
    break;
  case 0:
    execve(path.c_str(), NULL, NULL);
    cerr << "EXECVE FAILED" << endl;
    break;
  }

  /* successfully forked, so we should be good to go */
  errlog->writef("started DOWNLOADER with PID %d", LOG_INFO, pID);
}

/* CAP_Comp::stop()
   Sends message to component to terminate */
void CAP_Comp::stop(CAP_Pipe* pipe) {
  /* send terminate message to component */
  CAP_PipeMessage msg;
  msg.command = "MSG_QUIT";
  msg.body = "";
  if( !pipe->sendMessage(msg) ) {
    errlog->writef("unable to stop %s", LOG_ERROR, name.c_str());

    /* component will still be reading pipe so we can't just ignore it; 
       leave component status as is--someone will have to manually kill it */
    return;
  }

  /* all done */
  pID=0;
  busy=false;
}
