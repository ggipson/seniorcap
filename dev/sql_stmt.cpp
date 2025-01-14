/*******************************************************************************
  File Name: sql_stmt.cpp
  Author: Grant Gipson
  Date Last Edited: May 16, 2011
  Description: Series of functions for executing SQL statements
*******************************************************************************/
#include "log.h"
#include "sql.h"
#include <time.h>
#include <mysql/mysql.h>
#include <mysql/mysql_time.h>
#include <string.h>
#include <iostream>
using namespace std;

extern class CAP_Log* errlog;

/* dosql_archive_insert()
   Inserts a new archive record into database */
void dosql_archive_insert(const int user_id, list<string>& body) {
  if( !errlog || !sqlconn ) { throw -1; } /* SCREW THAT JAZZ!! */

  /* adding an archive is similar to adding content; duplicate the archive's 
     title and pass to function */
  list<string> newbody;
  list<string>::iterator it=body.begin();
  newbody.push_front(*it);
  newbody.push_front(*it);

  string trash;
  unsigned archive_id=0;
  if( !dosql_content_insert(newbody, user_id, trash, archive_id) ) {
    errlog->writef("failed to insert archive into database: call to "
		   "dosql_content_insert() failed", LOG_ERROR);
    return;
  }

  /* prepare SQL statements */
  static PreparedStatement* pstmt_archive_insert=NULL;
  static PreparedStatement* pstmt_archcnt_insert=NULL;

  if( !pstmt_archive_insert ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_archive_insert = sqlconn->prepareStatement(
        "insert into archive (id) values ((?))");
      pstmt_archcnt_insert = sqlconn->prepareStatement(
	"insert into archive_content (archive_id,content_id) values "
        "((?),(?))");
    }
    catch( SQLException err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  try {
    /* set parameters for SQL */
    pstmt_archive_insert->setUInt(1, archive_id);

    /* insert record into archive table to distinguish this from content */  
    int ret;
    if( (ret=pstmt_archive_insert->executeUpdate()) != 1 ) {
      errlog->writef("insert into archive values (%d) returned %d "
        "when 1 was expected", LOG_WARNING, archive_id, ret);
    }

    /* insert every archive-content pair into table */
    for( ++it; it!=body.end(); it++ ) {
      /* convert string to content ID */
      unsigned long content_id = strtoul((*it).c_str(),NULL,0);

      pstmt_archcnt_insert->setUInt(1, archive_id);
      pstmt_archcnt_insert->setUInt(2, content_id);

      if( (ret=pstmt_archcnt_insert->executeUpdate()) != 1 ) {
	errlog->writef("insert into archive_content values (%d,%d) returned %d "
	  "when 1 was expected", LOG_WARNING, archive_id, content_id, ret);
      }      
    }
  }
  catch( SQLException err ) {
    errlog->writef("failed to execute an SQL statement to insert content: "
      "what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
      err.getErrorCode(), err.getSQLState().c_str());
  }
}

/* dosql_archive_select()
   Selects next archive which has yet to be created */
bool dosql_archive_select(unsigned& archive_id, int& user_id, 
  list<ContentRec>& content)
{
  static PreparedStatement* pstmt_select_archive=NULL;
  static PreparedStatement* pstmt_select_content=NULL;

  if( !pstmt_select_archive ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_select_archive = sqlconn->prepareStatement(
        "select archive.id, content.user_id "
	"from archive inner join content on content.id=archive.id "
	"where archive.cmpl_date is null limit 1");
      pstmt_select_content = sqlconn->prepareStatement(
        "select content_id, title "
	"from archive_content inner join content "
	  "on archive_content.content_id=content.id "
	"where archive_id=(?)");
    }
    catch( SQLException& err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* execute query */
  ResultSet* res=NULL;
  try {
    res = pstmt_select_archive->executeQuery();

    /* if there is a job, then return its fields */
    if( res->next() ) {
      archive_id = res->getInt("id");
      user_id = res->getInt("user_id");
    }
    else { return false; }

    /* select all content within this archive */
    pstmt_select_content->setUInt(1, archive_id);
    res = pstmt_select_content->executeQuery();
    while( res->next() ) {
      ContentRec rec;
      rec.id = res->getUInt("content_id");
      rec.title = res->getString("title");
      content.push_back(rec);
    }
  }
  catch( SQLException& err ) {
    errlog->writef("failed to select records from archive table: what: %s, "
      "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
      err.getSQLState().c_str());
    return false;
  }
  return true;
}

/* dosql_archive_finish()
   Updates given archive ID in database with a completion date */
void dosql_archive_finish(const unsigned archive_id) {
  /* make sure caller is paying attention */
  if( !archive_id ) { throw CAP_Exception(CAPEXC_INVALPARAM); }

  static PreparedStatement* pstmt_archive_finish=NULL;

  if( !pstmt_archive_finish ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_archive_finish = sqlconn->prepareStatement(
        "update archive set cmpl_date=(?) where id=(?)");
    }
    catch( SQLException& err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* assign paramters to SQL and execute */
  try {
    /* format completion date */
    time_t t = time(NULL);
    tm* timeptr = localtime(&t);

    char sz[32];
    memset(sz,'\0',32);
    strftime(sz,32,"%Y-%m-%d %H:%M:%S",timeptr);
    string datetime(sz);

    /* set parameters */
    pstmt_archive_finish->setDateTime(1,datetime);
    pstmt_archive_finish->setInt(2,archive_id);

    /* update record */
    int ret;
    if( (ret=pstmt_archive_finish->executeUpdate()) != 1 ) {
      errlog->writef("updating archive with ID %d returned %d "
        "when 1 was expected", LOG_WARNING, archive_id, ret);
    }
  }
  catch( SQLException err ) {
    errlog->writef("failed to execute SQL to update archive record: "
      "what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
      err.getErrorCode(), err.getSQLState().c_str());
  }  
}

/* dosql_content_delete()
   Marks a content item deleted in database */
void dosql_content_delete(list<string>& body)
{
  	if( !errlog || !sqlconn ) { throw -1; } /* SCREW THAT JAZZ!! */

  	/* extract individual strings from body */
  	list<string>::iterator it=body.begin();
  	it++; /* ID of content */

  	static PreparedStatement* pstmt_content_delete=NULL;

	if( !pstmt_content_delete ) {
	    /* has not been prepared yet--give it a shot */
	    try {
	      pstmt_content_delete = sqlconn->prepareStatement(
	        "update content set status='D' where id=(?)");
	    }
	    catch( SQLException err ) {
			errlog->writef("failed to generate a prepared SQL statement for "
				"dosql_content_delete: what: %s, code: %d, state: %s", 
				LOG_FATAL, err.what(), err.getErrorCode(), err.getSQLState().c_str());
			throw -1;
		}
	}

	/* convert ID to a number */
	unsigned long uID = atol((*it).c_str());

	/* set parameters for SQL */
	try {
		pstmt_content_delete->setUInt(1, uID);

		/* execute update */
		int ret;
		if( (ret=pstmt_content_delete->executeUpdate()) != 1 ) {
			errlog->writef("update of content values (%u) returned %d "
        		"when 1 was expected", LOG_WARNING, uID, ret);
    	}
    }
	catch( SQLException err ) {
	errlog->writef("failed to execute an SQL statement to update content: "
		"what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
		err.getErrorCode(), err.getSQLState().c_str());
	}  
}

/* dosql_content_rename()
   Changes a content item's title */
void dosql_content_rename(list<string>& body)
{
  	if( !errlog || !sqlconn ) { throw -1; } /* SCREW THAT JAZZ!! */

  	/* extract individual strings from body */
  	list<string>::iterator it=body.begin();
  	it++; /* ID of content */

  	static PreparedStatement* pstmt_content_rename=NULL;

	if( !pstmt_content_rename ) {
	    /* has not been prepared yet--give it a shot */
	    try {
	      pstmt_content_rename = sqlconn->prepareStatement(
	        "update content set title=(?) where id=(?)");
	    }
	    catch( SQLException err ) {
			errlog->writef("failed to generate a prepared SQL statement for "
				"dosql_content_rename: what: %s, code: %d, state: %s", 
				LOG_FATAL, err.what(), err.getErrorCode(), err.getSQLState().c_str());
			throw -1;
		}
	}

	/* check list of strings */
	if( body.size() != 3 ) {
		errlog->writef("message body parsing error in dosql_content_rename(): %d lines "
			"when 3 were expected", LOG_ERROR, body.size());
		return;
	}

	/* convert ID to a number */
	unsigned long uID = atol((*it).c_str());
	string strTitle = *(++it);

	/* set parameters for SQL */
	try {
		pstmt_content_rename->setString(1, strTitle.c_str());
		pstmt_content_rename->setUInt(2, uID);

		/* execute update */
		int ret;
		if( (ret=pstmt_content_rename->executeUpdate()) != 1 ) {
			errlog->writef("update of content values (%u, %s) returned %d "
        		"when 1 was expected", LOG_WARNING, uID, strTitle.c_str(), ret);
    	}
    }
	catch( SQLException err ) {
		errlog->writef("failed to execute an SQL statement to update content in "
			"dosql_content_rename(): what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
			err.getErrorCode(), err.getSQLState().c_str());
	}  
}

/* dosql_content_insert()
   Inserts a new content item into database */
bool dosql_content_insert(list<string>& body, int user_id, string& filename, 
			  unsigned& content_id)
{
  if( !errlog || !sqlconn ) { throw -1; } /* SCREW THAT JAZZ!! */

  /* extract individual strings from body */
  list<string>::iterator it=body.begin();
  filename = *it;
  it++; /* title of content */

  static PreparedStatement* pstmt_content_insert=NULL;
  static PreparedStatement* pstmt_get_id=NULL;

  if( !pstmt_content_insert ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_content_insert = sqlconn->prepareStatement(
        "insert into content (user_id,folder_id,add_date,status,title) "
	"values ((?),(?),(?),'A',(?))");
      pstmt_get_id = sqlconn->prepareStatement("select last_insert_id()");
    }
    catch( SQLException err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* format date added */
  time_t t = time(NULL);
  tm* timeptr = localtime(&t);

  char sz[32];
  memset(sz,'\0',32);
  strftime(sz,32,"%Y-%m-%d %H:%M:%S",timeptr);
  string datetime(sz); 

  /* set parameters for SQL */
  try {
    pstmt_content_insert->setInt(1, user_id);
    pstmt_content_insert->setInt(2, 1);
    pstmt_content_insert->setDateTime(3, datetime);
    pstmt_content_insert->setString(4, (*it).c_str());

    int ret;
    if( (ret=pstmt_content_insert->executeUpdate()) != 1 ) {
      errlog->writef("insert into content values (%d,%d,%s,%s) returned %d "
        "when 1 was expected", LOG_WARNING, user_id, 1, datetime.c_str(), 
	(*it).c_str(), ret);
    }

    /* get ID of content just inserted */
    ResultSet* rs = pstmt_get_id->executeQuery();
    if( !rs->next() ) {
      errlog->write("failed to get ID of inserted content",LOG_ERROR);
      return false;
    }
    content_id = rs->getUInt(1);
  }
  catch( SQLException err ) {
    errlog->writef("failed to execute an SQL statement to insert content: "
      "what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
      err.getErrorCode(), err.getSQLState().c_str());
  }  

  return true;
}

/* dosql_job_insert()
   Inserts a new record into *job* table */
void dosql_job_insert(const int user_id, list<string>& body)
{
  static PreparedStatement* pstmt_insert_job=NULL;
  int ret=0; /* various uses */

  if( !errlog || !sqlconn ) { throw -1; } /* SCREW THAT JAZZ!! */

  if( !pstmt_insert_job ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_insert_job = sqlconn->prepareStatement(
        "insert into job (user_id,type,status,url) values ((?),(?),\"P\",(?))");
    }
    catch( SQLException err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* check list of strings */
  if( body.size() != 3 ) {
    errlog->writef("message body parsing error: %d lines when "
      "3 were expected", LOG_ERROR, body.size());
    return;
  }

  /* create job type value from request */
  char type[2];
  list<string>::iterator it=body.begin(); /* type string */

  if( *it == "download" ) { 
    type[0]='d';
    it++; /* mode string */

    if( *it == "single" ) { type[1]='S'; }
    else { /* unknown request type */
      errlog->writef("discarded unknown client request %s,%s received",
        LOG_WARNING, (*(--it)).c_str(), (*(++it)).c_str());
      return;
    }
  }
  else { /* unknown request type */
    errlog->writef("discarded unknown client request %s,%s received",
      LOG_WARNING, (*it).c_str(), (*(++it)).c_str());
    return;
  }

  /* now assign values to prepared statement and execute */
  try {
    pstmt_insert_job->setInt(1,user_id);
    pstmt_insert_job->setString(2,type);
    pstmt_insert_job->setString(3,*(++it));
    if( (ret=pstmt_insert_job->executeUpdate()) != 1 ) {
      errlog->writef("insert into job values (%d,%s,...) returned %d "
        "when 1 was expected", LOG_WARNING, user_id, type, ret);
    }
  }
  catch( SQLException err ) {
    errlog->writef("failed to generate a prepared SQL statement: "
      "what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
      err.getErrorCode(), err.getSQLState().c_str());
  }
}

/* dosql_job_select()
   Selects the next available job from *job table */
bool dosql_job_select(unsigned& job_id, int& user_id, string& type, string& url)
{
  static PreparedStatement* pstmt_select_job=NULL;

  if( !pstmt_select_job ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_select_job = sqlconn->prepareStatement(
        "select * from job where status=\"P\" limit 1");
    }
    catch( SQLException& err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* execute query */
  ResultSet* res=NULL;
  try {
    res = pstmt_select_job->executeQuery();

    /* if there is a job, then return its fields */
    if( res->next() ) {
      job_id = res->getInt("id");
      user_id = res->getInt("user_id");
      type = res->getString("type");
      url = res->getString("url");
    }
  }
  catch( SQLException& err ) {
    errlog->writef("failed to select records from job table: what: %s, "
      "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
      err.getSQLState().c_str());
    return false;
  }
  return true;
}

/* dosql_job_failed()
   Updates given job ID in database indicating that it failed */
void dosql_job_failed(const int job_id) {
  /* make sure caller is paying attention */
  if( !job_id ) { throw CAP_Exception(CAPEXC_INVALPARAM); }

  static PreparedStatement* pstmt_job_failed=NULL;

  if( !pstmt_job_failed ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_job_failed = sqlconn->prepareStatement(
        "update job set status=\"F\" where id=(?)");
    }
    catch( SQLException& err ) {
		errlog->writef("failed to generate a prepared SQL statement in "
			"dosql_job_failed(): what: %s, code: %d, state: %s", 
			LOG_FATAL, err.what(), err.getErrorCode(), 
        	err.getSQLState().c_str());
      	throw -1;
    }
  }

  /* assign paramters to SQL and execute */
  try {
    /* set parameters */
    pstmt_job_failed->setInt(1,job_id);

    /* update record */
    int ret;
    if( (ret=pstmt_job_failed->executeUpdate()) != 1 ) {
      errlog->writef("updating job with ID %d returned %d "
        "when 1 was expected", LOG_WARNING, job_id, ret);
    }
  }
  catch( SQLException err ) {
    errlog->writef("failed to execute SQL to update job record in "
		"dosql_job_failed(): what: %s, code: %d, state: %s", 
		LOG_FATAL, err.what(), err.getErrorCode(), 
		err.getSQLState().c_str());
  }  
}

/* dosql_job_finish()
   Updates given job ID in database with a completion date */
void dosql_job_finish(const int job_id) {
  /* make sure caller is paying attention */
  if( !job_id ) { throw CAP_Exception(CAPEXC_INVALPARAM); }

  static PreparedStatement* pstmt_job_finish=NULL;

  if( !pstmt_job_finish ) {
    /* has not been prepared yet--give it a shot */
    try {
      pstmt_job_finish = sqlconn->prepareStatement(
        "update job set cmpl_date=(?), status=\"C\" where id=(?)");
    }
    catch( SQLException& err ) {
      errlog->writef("failed to generate a prepared SQL statement: what: %s, "
        "code: %d, state: %s", LOG_FATAL, err.what(), err.getErrorCode(), 
        err.getSQLState().c_str());
      throw -1;
    }
  }

  /* assign paramters to SQL and execute */
  try {
    /* format completion date */
    time_t t = time(NULL);
    tm* timeptr = localtime(&t);

    char sz[32];
    memset(sz,'\0',32);
    strftime(sz,32,"%Y-%m-%d %H:%M:%S",timeptr);
    string datetime(sz);

    /* set parameters */
    pstmt_job_finish->setDateTime(1,datetime);
    pstmt_job_finish->setInt(2,job_id);

    /* update record */
    int ret;
    if( (ret=pstmt_job_finish->executeUpdate()) != 1 ) {
      errlog->writef("updating job with ID %d returned %d "
        "when 1 was expected", LOG_WARNING, job_id, ret);
    }
  }
  catch( SQLException err ) {
    errlog->writef("failed to execute SQL to update job record: "
      "what: %s, code: %d, state: %s", LOG_FATAL, err.what(), 
      err.getErrorCode(), err.getSQLState().c_str());
  }  
}
