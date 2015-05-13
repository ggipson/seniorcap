/*******************************************************************************
  File Name: sql.h
  Author: Grant Gipson
  Date Last Edited: May 14, 2011
  Description: Function prototypes for database access
*******************************************************************************/
#ifndef _SQL_H_
#define _SQL_H_

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <list>
#include <string>
using namespace std;
using namespace sql;

struct ContentRec {
  unsigned id;
  string title;
};

extern Connection* sqlconn;

void dosql_archive_insert(const int user_id, list<string>& body);
bool dosql_archive_select(unsigned& archive_id, int& user_id, 
  list<ContentRec>& content);
void dosql_archive_finish(const unsigned archive_id);
void dosql_content_delete(list<string>& body);
void dosql_content_rename(list<string>& body);
bool dosql_content_insert(list<string>& body, int user_id, string& filename, unsigned& content_id);
void dosql_job_insert(const int user_id, list<string>& body);
bool dosql_job_select(unsigned& job_id, int& user_id, string& type, string& url);
void dosql_job_failed(const int job_id);
void dosql_job_finish(const int job_id);

#endif /* _SQL_H_ */
