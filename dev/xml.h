//-----------------------------------------------------------------------------
// File Name: xml.h
// Author: Grant Gipson
// Date Last Edited: Mar. 4, 2011
// Description: XML wrapper class for interactions with Xerces-C++
//-----------------------------------------------------------------------------
#ifndef _XML_H_
#define _XML_H_

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <string>
#include <list>
using namespace std;
using namespace xercesc;

class CAP_XMLErrHandler : public HandlerBase {
public:
  void error(const SAXParseException& err);
  void fatalError(const SAXParseException& err);
  void warning(const SAXParseException& err);
};

class CAP_XMLException {
public:
  CAP_XMLException(const char* msg) : strmsg(msg) {}
  CAP_XMLException(const string& msg) : strmsg(msg) {}
  const string strmsg;
};

class CAP_XML {
protected:
  XercesDOMParser* parser;  // XML parser
  ErrorHandler* errHandler;
  DOMDocument* xmldoc;      // XML configuration document
 
  bool searchNode(DOMElement* node, 
    list<string>& elemPath, string& strDest);

public:
  CAP_XML(const char* xmlfile);
  CAP_XML(CAP_XML& ref);
  ~CAP_XML();

  bool getValue(const char* elem, string& strDest);
};

#endif // _XML_H_
