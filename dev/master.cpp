//-----------------------------------------------------------------------------
// File Name: master.cpp
// Author: Grant Gipson
// Date Last Edited: May 14, 2011
// Description: Entry point for Senior CAP Project Master Program
//-----------------------------------------------------------------------------
#include "master.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/file.h>
#include <stdio.h>
#include "xml.h"
#include "log.h"
#include <stdlib.h>
#include "pipe.h"
#include <time.h>
#include <list>
#include "sql.h"
#include <signal.h>
using namespace std;

// Global Variables
CAP_Log* errlog = 0;       // global error log
CAP_XML* xmlconfig = 0;    // global XML configuration file
Connection* sqlconn=NULL;  /* connection to database */

// logErrHandler()
// Handles errors from log files
void logErrHandler(const char* pszmsg) {
  cerr << "log write failed: " << pszmsg << endl;
}

/* parseBody()
   Parses message body into separate lines */
void parseBody(string& str, list<string>& body) {
  int start=0;
  int end=0;

  /* search for newlines */
  while( (end=str.find('\n',start)) != string::npos ) {
    body.push_back(str.substr(start,end-start));
    start=end+1;
  }
  body.push_back(str.substr(start)); /* last string */
}

/* sig_pipe()
   Handles SIGPIPE signals which indicate a broken pipe */
void sig_pipe(int sig) {
  /* make sure this is the expected signal */
  if( sig==SIGPIPE ) {
    errlog->write("received SIGPIPE signal", LOG_ERROR);
  }
  else {
    /* what is this garbage??? */
    errlog->writef("received signal %d when SIGPIPE was expected", 
      LOG_WARNING, sig);
  }
}

// main()
// Program entry point; open pipes, database connection and begin message 
// processing loop
int main(int argc, char* argv[]) {
  int ret=0;                      /* return value */
  CAP_Pipe* pipe_master=NULL;     /* receives messages */
  CAP_Pipe* pipe_downloader=NULL; /* commands to downloader */
  CAP_Pipe* pipe_archiver=NULL;   /* commands to archiver */
  int nFdRuntime=0;               /* file descriptor of PID file */
  mysql::MySQL_Driver* sqldriver=NULL;

  /* exit status is thrown upon an abort or normal terminaton */
  try {

  // check for proper number of command line arguments; there should be the 
  // location of a runtime lock file, and the system configuration file
#ifdef VERBOSE_OUT
  cout << "Validating command line arguments..." << endl;
#endif
  if( argc < 3 ) {
    cerr << "Unexpected number of command line arguments. You must specify "
         << "the location of a runtime lock file and the system configuration "
         << "file."
         << endl;
    throw -1;
  }

  // clear file permissions mask for new files
  umask(0);

  //---------------------------------------------------------------------------
  //    Process ID file
  //---------------------------------------------------------------------------
#ifdef VERBOSE_OUT
  cout << "Preparing process ID file..." << endl;
#endif

  // open PID file and lock access to it so as to prevent 
  // another instance of program from running
  nFdRuntime = open(argv[1], O_RDWR|O_CREAT,CAP_FILE_MASK);
  if( nFdRuntime == -1 ) {
    cerr << "Fatal error occurred trying to open runtime lock file. errno: "
         << errno
         << endl;
    throw -1;
  }
  if( flock(nFdRuntime, LOCK_EX|LOCK_NB) == -1 ) {
    if( errno != 11 ) {
      cerr << "Fatal error occurred trying to lock access to PID file. errno: "
	   << errno
	   << endl;
      throw -1;
    }
    else {
      cerr << "Instance of program is already running." << endl;
      throw -1;
    }
  }

  // get process ID and write it to runtime lock file
  pid_t pidThis = getpid();
  char szPidThis[32] = {'\0'};
  int nPidThisLen = snprintf(szPidThis, 32, "%i", pidThis);
  if( nPidThisLen == -1 || nPidThisLen >= 32 ) {
    cerr << "Unable to format process ID before writing to file. return: "
         << nPidThisLen
         << endl;
    throw -1;
  }
  if( write(nFdRuntime, szPidThis, nPidThisLen) == -1 ) {
    cerr << "Failed to write process ID to runtime lock file. errno: "
         << errno
         << endl;
    throw -1;
  }

  //---------------------------------------------------------------------------
  //    XML and error log
  //---------------------------------------------------------------------------
#ifdef VERBOSE_OUT
  cout << "Initializing XML libraries and reading configuration file..."
       << endl;
#endif

  // initialize XML libraries and read configuration file
  try {
    xmlconfig = new CAP_XML(argv[2]);
  }
  catch( CAP_XMLException err ) {
    cerr << err.strmsg << endl;
    throw -1;
  }

#ifdef VERBOSE_OUT
  cout << "Opening log file..." << endl;
#endif

  // get error log file name from XML
  string strMasterLog;
  if( !xmlconfig->getValue("log_files.master_log", strMasterLog) ) {
    cerr << "Failed to get master_log value from XML" << endl;
    throw -1;
  }

  // create log object and open file for use
  try {
    errlog = new CAP_Log();
    errlog->open(strMasterLog);

    string strLogPriority;
    if( !xmlconfig->getValue("log_priority_write", strLogPriority) ) {
      cerr << "failed to get log_priority_write from XML "
	   << "defaulting to 0"
	   << endl;
      strLogPriority="0";
    }
    errlog->setLeastLogPriority( (LogPriority)atoi(strLogPriority.c_str()) );

    // wait and set this here so that only write errors will be sent to 
    // function
    errlog->setErrHandler(logErrHandler);
  }
  catch( CAP_LogException& err ) {
    cerr << "failed to open master log for use: " << err.msg << endl;
    throw -1;
  }

  /* write initial line so log parser knows where to start */
  errlog->write("NEW LOG SESSION", LOG_NONE);

  //---------------------------------------------------------------------------
  //    Additional Setup
  //---------------------------------------------------------------------------

  /* designate handler for any SIGPIPE signals */
  if( signal(SIGPIPE, sig_pipe) == SIG_ERR ) {
    errlog->write("failed to designate handler for SIGPIPE signals", 
      LOG_WARNING);
  }

  /* get locations for downloading, content and archiving */
  string strDownload_Dir="";
  string strContent_Dir="";
  string strArchive_Dir="";
  if( !xmlconfig->getValue("components.downloader_dir", strDownload_Dir) ) {
    errlog->write("failed to read download directory from XML", LOG_FATAL);
    throw -1;
  }
  if( !xmlconfig->getValue("components.content_dir", strContent_Dir) ) {
    errlog->write("failed to read content directory from XML", LOG_FATAL);
    throw -1;
  }
  if( !xmlconfig->getValue("components.archiver_dir", strArchive_Dir) ) {
    errlog->write("failed to read archive directory from XML", LOG_FATAL);
    throw -1;
  }

  // get location and names of pipes
  string strPipe_Dir="";
  string strPipe_Master="";
  string strPipe_Downloader="";
  string strPipe_Archiver="";
  if( !xmlconfig->getValue("pipes.pipes_dir", strPipe_Dir) ) {
    errlog->write("Failed to read pipes dir. from XML", LOG_FATAL);
    throw -1;
  }
  else {
    if( !xmlconfig->getValue("pipes.pipes_master", strPipe_Master) ) {
      errlog->write("failed to read master pipe name from XML", LOG_FATAL);
      throw -1;
    }
    if( !xmlconfig->getValue("pipes.pipes_downloader", strPipe_Downloader) ) {
      errlog->write("failed to read downloader pipe name from XML", LOG_FATAL);
      throw -1;
    }
    if( !xmlconfig->getValue("pipes.pipes_archiver", strPipe_Archiver) ) {
      errlog->write("failed to read archiver pipe name from XML", LOG_FATAL);
      throw -1;
    }
  }

  // prepare to open pipes for communication
  strPipe_Master = strPipe_Dir + strPipe_Master;
  strPipe_Downloader = strPipe_Dir + strPipe_Downloader;
  strPipe_Archiver = strPipe_Dir + strPipe_Archiver;
  try {
    pipe_master = new CAP_Pipe("master", errlog);
    pipe_master->create(strPipe_Master, PIPE_RDONLY);
    pipe_downloader = new CAP_Pipe("downloader", errlog);
    pipe_downloader->create(strPipe_Downloader, PIPE_WRONLY);
    pipe_archiver = new CAP_Pipe("archiver", errlog);
    pipe_archiver->create(strPipe_Archiver, PIPE_WRONLY);
  }
  catch( CAP_PipeException err) {
    throw -1;
  }

  /* get information needed to connect to database */
  string strDBconnect="";
  string strDBuser="";
  string strDBpasswd="";
  if( !xmlconfig->getValue("database.connect", strDBconnect) ) {
    errlog->write("failed to read database connection string from XML", 
      LOG_FATAL);
    throw -1;
  }
  if( !xmlconfig->getValue("database.master_user", strDBuser) ) {
    errlog->write("failed to read database username from XML", LOG_FATAL);
    throw -1;
  }
  if( !xmlconfig->getValue("database.master_password", strDBpasswd) ) {
    errlog->write("failed to read database password from XML", LOG_FATAL);
    throw -1;
  }

  /* connect to database */
  try {
    sqldriver = mysql::get_driver_instance();
    sqlconn = sqldriver->connect(strDBconnect.c_str(),strDBuser.c_str(),
      strDBpasswd.c_str());
    errlog->write("opened connection to database");
  }
  catch( SQLException& err ) {
    errlog->writef("failed to connect to database: what: %s, code: %d, "
                   "state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
	           err.getSQLState().c_str());
    throw -1;
  }

  /* give other components some time to start before we start */
  sleep(CAP_STARTUP_DELAY);

  //---------------------------------------------------------------------------
  //    Message Loop
  //---------------------------------------------------------------------------

  errlog->write("master program started; entering message loop");

  // enter message loop; reading pipes until told to terminate
  CAP_PipeMessage msg;
  int errCount_Rd=0; /* number of errors which have occurred trying 
			to read from pipe */
  unsigned download_job_id=0; /* job ID being handled by downloader */
  int download_user_id=0; /* user ID of above job */
  unsigned archive_job_id=0; /* ID of archive being created */
  int archive_user_id=0; /* user ID of above archive */

	while( true ) {
    /* if the downloader is not busy, then check for available jobs and 
       send one to be processed */
    if( !download_job_id ) {
      CAP_PipeMessage msg_send;
      if( dosql_job_select(download_job_id, download_user_id, 
			   msg_send.command, msg_send.body) )
      {
        /* there is a job so send it to downloader */
				pipe_downloader->sendMessage(msg_send);
      }
    }

    /* if the archiver is not busy, then check for the next one which needs 
       created and send it off */
    if( !archive_job_id ) {
      CAP_PipeMessage msg_send;
      list<ContentRec> content;

      if( dosql_archive_select(archive_job_id, archive_user_id, content) ) {
	if( content.size() ) {
	  /* copy content items into directory for archiving */
	  for( list<ContentRec>::iterator it=content.begin();
	       it!=content.end();
	       it++ )
	  {
	    char sz[512];
	    memset(sz, '\0', 512);
	    sprintf(sz, "cp \"%s%010d.html\" \"%s%d-%s.html\"", 
		    strContent_Dir.c_str(), (*it).id, strArchive_Dir.c_str(), 
		    (*it).id, (*it).title.c_str());
	    system(sz);
	  }
	
	  char sz[32];
	  memset(sz, '\0', 32);
	  sprintf(sz, "%010d", archive_job_id);

	  /* send command to archiver */
	  msg_send.command="MSG_ARCHIVE";
	  msg_send.body.assign(sz);
	  pipe_archiver->sendMessage(msg_send);
	}
	else { /* how is this empty? */
	  dosql_archive_finish(archive_job_id);
	  errlog->writef("found an empty archive %d and marked it complete", 
            LOG_WARNING, archive_job_id);
	  archive_job_id=0;
	  archive_user_id=0;
	}
      }
    }

    /* wait for a message */
    if( !pipe_master->getMessage(msg) ) {
      errlog->writef("message from pipe %s was lost", LOG_WARNING, 
                     pipe_master->getName().c_str());

      /* if number of read errors has reached a concerning level, then 
         something has gone wrong, so terminate */
      if( ++errCount_Rd == PIPE_READ_ERROR_MAX ) {
        errlog->writef("The maximum number of allowed errors (%d) reading "
	               "from pipe %s has been reached. Master Program will "
	               "now terminate.", LOG_FATAL, PIPE_READ_ERROR_MAX, 
	               pipe_master->getName().c_str());
	throw -1;
      }
    }
		else {
      errCount_Rd=0; /* a successful read disregards any errors */

      if( msg.command == "MSG_QUIT" ) {
	/* stop Master Program */
	throw 0;
      }
      else if( msg.command == "MSG_NULL" ) {
	/* do nothing */
      }
      else if( msg.command == "MSG_ARCHIVEREQ" ) {
	/* request to create an archive */
	list<string> body;
	parseBody(msg.body,body);
	dosql_archive_insert(1,body);
      }
      else if( msg.command == "MSG_ARCHIVED" ) {
	if( !archive_job_id ) {
	  errlog->write("received unexpected MSG_ARCHIVED", LOG_WARNING);
	  continue;
	}

	/* move archive to content directory */
	char sz[128];
	memset(sz, '\0', 128);
	sprintf(sz, "mv \"%s%010d.zip\" \"%s\"", strArchive_Dir.c_str(), 
	  archive_job_id, strContent_Dir.c_str());
	system(sz);

	/* clear archive directory */
	memset(sz, '\0', 128);
	sprintf(sz, "rm %s*", strArchive_Dir.c_str());
	system(sz);

	/* update archive as completed */
	dosql_archive_finish(archive_job_id);
	errlog->writef("created archive %010d.zip", LOG_INFO, archive_job_id);
	archive_job_id=0;
	archive_user_id=0;
      }
			else if( msg.command == "MSG_CLIENTREQ" ) {
				/* request from client extension */
				list<string> body;
				parseBody(msg.body,body);
				list<string>::iterator type=body.begin();

				/* determine client request type */
				if( *type == "download" ) {
			        dosql_job_insert(1,body);
				}
				else if( *type == "delete" ) {
					dosql_content_delete(body);
				}
				else if( *type == "rename" ) {
					dosql_content_rename(body);
				}
				else {
					errlog->writef("received unknown MSG_CLIENTREQ type: %s", 
						LOG_WARNING, (*type).c_str());
				}
			}
      		else if( msg.command == "MSG_DOWNLOADED" ) {
		        /* downloader has finished */
				list<string> body;
				parseBody(msg.body,body);

				/* insert content into database */
				string strFilename="";
				unsigned content_id=0;
				if( !dosql_content_insert(body, download_user_id, strFilename, content_id) ) {
					continue;
				}

				/* prepare move commad */
				char szSystemCmd[1024];
				memset(szSystemCmd, '\0', 1024);

				int fret=0;
				if( sprintf(szSystemCmd, "mv %s%s %s%010u.html", 
					strDownload_Dir.c_str(), strFilename.c_str(), 
					strContent_Dir.c_str(), content_id) == -1 )
				{
					errlog->writef("unable to format file name for content %d", LOG_ERROR, content_id);
					continue;
				}

				/* move content into storage */
				system(szSystemCmd);

				/* clear download location */
				memset(szSystemCmd, '\0', 1024);
				if( sprintf(szSystemCmd, "rm %s*", strDownload_Dir.c_str()) == -1 ) {
					errlog->writef("unable to format command to clear download dir.", LOG_ERROR);
				}
				else {
					system(szSystemCmd);
				}

				/* mark job completed */
				dosql_job_finish(download_job_id);
				download_job_id  =0;
				download_user_id =0;
			}
			else if( msg.command == "MSG_DOWNLOADFAIL" ) {
				errlog->writef("downloader indicated that job %u failed", LOG_WARNING, download_job_id);

				/* mark job failed */
				dosql_job_failed(download_job_id);
				download_job_id  =0;
				download_user_id =0;
			}
			else {
				/* unknown message */
				errlog->writef("received unknown message %s", LOG_WARNING, msg.command.c_str());
			}
		}

		/* next message! */
	}

  //---------------------------------------------------------------------------
  //    Cleanup
  //---------------------------------------------------------------------------

  } catch( int err ) {
    /* zero is a normal termination */
    ret=err;
  }

  // close pipes
  delete pipe_master;
  delete pipe_downloader;
  delete pipe_archiver;

  // close file descriptors and streams
  if( close(nFdRuntime) == -1 ) {
    errlog->writef("Failed to close file descripter for runtime lock "
		   "file. errno: %d", LOG_ERROR, errno);
  }

  try { 
    delete sqlconn;
    errlog->write("closed connection to database");
  }
  catch( sql::SQLException& err ) {
    errlog->writef("failed to close database: what: %s, code: %d, "
                   "state: %s", LOG_WARNING, err.what(), err.getErrorCode(), 
	           err.getSQLState().c_str());	  
  }

  errlog->write("terminating Master Program");
  delete errlog;
  delete xmlconfig;

  return ret;
}
