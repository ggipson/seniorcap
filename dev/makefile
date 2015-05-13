# File Name: makefile
# Author: Grant Gipson
# Date Last Edited: April 11, 2011
# Description: Used to make Master Program for Senior CAP Project

XERCESLIB=/home/grant/seniorcap/xerces-c-3.1.1/lib

all: capmaster filecopy

capmaster: master.cpp xml.cpp xml.h log.cpp log.h master.h pipe.h pipe.cpp \
buffer.cpp sql_stmt.cpp
	@g++ -o capmaster -L$(XERCESLIB) -lxerces-c -lmysqlcppconn master.cpp \
		xml.cpp log.cpp pipe.cpp buffer.cpp sql_stmt.cpp

filecopy: capconf.xml capconf.dtd
	@cp capconf.xml /var/cap/
	@cp capconf.dtd /var/cap/
