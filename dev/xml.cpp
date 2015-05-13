//-----------------------------------------------------------------------------
// File Name: xml.cpp
// Author: Grant Gipson
// Date Last Edited: Mar. 5, 2011
// Description: XML wrapper class for interactions with Xerces-C++
//-----------------------------------------------------------------------------
#include "xml.h"
#include <iostream>
using namespace std;

// CAP_XMLErrHandler::error
// Responds to errors from XML parser
void CAP_XMLErrHandler::error(const SAXParseException& err) {
  const char* msg = XMLString::transcode(err.getMessage());
  cerr << "XML parsing error: " << msg << endl;
}

// CAP_XMLErrHandler::fatalError
// Responds to fatal errors from XML parser
void CAP_XMLErrHandler::fatalError(const SAXParseException& err) {
  error(err);
}

// CAP_XMLErrHandler::warning
// Responds to warnings from XML parser
void CAP_XMLErrHandler::warning(const SAXParseException& err) {
  error(err);
}

// CAP_XML::searchNode()
// Recursively searches XML tree for given value path
bool CAP_XML::searchNode(
  DOMElement* node, list<string>& elemPath, string& strDest)
{
  // check to make sure element path is not empty--if it is, then this is 
  // the value being searched for
  if( elemPath.empty() ) {
    // child node should be text value requested--check for it
    DOMText* text = 0;;
    if( !(text=(DOMText*)node->getFirstChild()) ) {
      return false; // what a FAIL!!!!
    }

    // pull text data from element and return it to caller
    char* val = XMLString::transcode(text->getData());
    strDest = val;
    XMLString::release(&val);
    return true;
  }

  // get first child of element; if this is not a match, then search 
  // each of the siblings
  if( !(node=node->getFirstElementChild()) ) {
    cerr << "No children" << endl;
    return false; // no children--give up
  }

  char* szTagName = XMLString::transcode(node->getTagName());
  if( !(elemPath.front()).compare(szTagName) ) {
    // found it! remove this element from path list and move on to next
    XMLString::release(&szTagName);
    elemPath.pop_front();
    return searchNode(node, elemPath, strDest);
  }
  XMLString::release(&szTagName);

  // wasn't the first child, so search its siblings
  while( (node=node->getNextElementSibling()) ) {
    szTagName = XMLString::transcode(node->getTagName());
    if( !(elemPath.front()).compare(szTagName) ) {
      // found it! remove this element from path list and move on to next
      XMLString::release(&szTagName);
      elemPath.pop_front();
      return searchNode(node, elemPath, strDest);
    }
    XMLString::release(&szTagName);
  }

  return false; // no mo' children  
}

// CAP_XML::CAP_XML()
// Class constructor
CAP_XML::CAP_XML(const char* xmlfile)
  : parser(0), errHandler(0), xmldoc(0)
{
  try {
    // initialize and set parser's error handler
    XMLPlatformUtils::Initialize();
    parser = new XercesDOMParser();
    errHandler = new CAP_XMLErrHandler();
    parser->setErrorHandler(errHandler);
    parser->setValidationScheme(XercesDOMParser::Val_Always);

    // parse file and get XML document
    parser->parse(xmlfile);
    xmldoc = parser->getDocument();
  }
  catch( const XMLException& err ) {
    string str1 = "XMLException occurred while initializing and "
      "reading XML: ";
    char* errmsg = XMLString::transcode(err.getMessage());
    string str2 = errmsg;
    XMLString::release(&errmsg);
    throw CAP_XMLException(str1 + str2);
  }
  catch( ... ) {
    throw CAP_XMLException("An unknwon exception has occurred while "
      "initializing and reading XML");
  }
}

// CAP_XML::CAP_XML(CAP_XML&)
// Copy constructor--does nothing
CAP_XML::CAP_XML(CAP_XML& ref) {
  // do nothing--copies not supported
}

// CAP_XML::~CAP_XML()
// Class destructor
CAP_XML::~CAP_XML() {
  delete parser;
  delete errHandler;

  try {
    XMLPlatformUtils::Terminate();
  }
  catch( const XMLException& err ) {
    string str1 = "XMLException occurred while terminating XML: ";
    char* errmsg = XMLString::transcode(err.getMessage());
    string str2 = errmsg;
    XMLString::release(&errmsg);
    throw CAP_XMLException(str1 + str2);
  }
  catch( ... ) {
    throw CAP_XMLException("An unknwon exception has occurred while "
      "terminating XML");
  }
}

// CAP_XML::getValue(..., string&)
// Returns string value of specified element
bool CAP_XML::getValue(const char* elem, string& strDest) {
  // parse given element path; every parent and child node is separated 
  // by a period
  string strElem = elem;
  int nParseStart = 0;
  int nPeriod = 0;
  list<string> elemPath;
  list<string>::iterator elemPathIt;  

  nPeriod = strElem.find('.', nPeriod);
  while( nPeriod != string::npos ) {
    elemPath.push_back( strElem.substr(nParseStart, nPeriod-nParseStart) );
    nParseStart = nPeriod+1; // next character after period
    nPeriod++;
    nPeriod = strElem.find('.', nPeriod);
  }
  elemPath.push_back( strElem.substr(nParseStart) );

  // store pointer to root XML element
  DOMElement* node = 0; // used to traverse XML tree
  DOMElement* prev_node = 0; // used to store previously visited element
  node = xmldoc->getDocumentElement();

  // traverse all elements within XML tree; searching for value
  return searchNode(node, elemPath, strDest);
}
