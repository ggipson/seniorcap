/******************************************************************************
	File Name: connect.js
	Author: Grant Gipson
	Date Last Edited: May 10, 2011
	Description: Performs various tasks related to connection to server but 
		not related to download requests.
******************************************************************************/

/* 	browseServer()
	Opens window to server address */
function browseServer() {
	/* pull server address from preferences */
	var prefManager = Components.classes["@mozilla.org/preferences-service;1"]
		.getService(Components.interfaces.nsIPrefBranch);
	var serverAddr = prefManager.getCharPref("extensions.seniorcap.serverAddress");
	if( serverAddr == "" ) {
		alert("ERROR: No server address specified!");
		return;
	}

	/* open server in window */
	content.wrappedJSObject.location = "http://"+serverAddr+"/cap/home.php";
}
