/*******************************************************************************
  File Name: comp.h
  Author: Grant Gipson
  Date Last Edited: April 15, 2011
  Description: Component class which is used to manage Master Program's 
    peripheral components
*******************************************************************************/
#ifndef _COMP_H_
#define _COMP_H_

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include "pipe.h"
using namespace std;

class CAP_Comp {
protected:
  pid_t pID;     /* process ID */
  bool busy;     /* variable used by caller */
  string path;   /* component path */
  string name;   /* name of component */

public:
   CAP_Comp(string _name, string& _path);
  ~CAP_Comp();

  void start();
  void stop(CAP_Pipe* pipe);
  bool isRunning();
  inline void setBusy(bool _busy) {busy=_busy; }
  inline bool isBusy ()           {return busy;}
};

#endif /* COMP_H_ */
