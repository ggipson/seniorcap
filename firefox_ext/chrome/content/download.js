// File Name: download.js
// Author: Grant Gipson
// Date Last Edited: May 10, 2011
// Description: Handles download requests for Senior CAP Firefox extension

// downloadReq()
// Sends a request to SeniorCAP server to download content for this user
function downloadReq(mode) {
	if( mode === "single" ) { // single page
		// prepare download request
		var req_type = "download";
		var req_url = window.content.location.href;
		var req_mode = "single";

		var req = new String();
		req = req.concat(
		    req_type, "\n", 
		    req_mode, "\n", 
		    req_url
		);

		// pull server address from preferences
		var prefManager = Components.classes["@mozilla.org/preferences-service;1"]
			.getService(Components.interfaces.nsIPrefBranch);
		var serverAddr = prefManager.getCharPref("extensions.seniorcap.serverAddress");
		if( serverAddr == "" ) {
			alert("ERROR: No server address for connection specified!");
			return;
		}

		// send request to CGI script
		var xmlhttp = new XMLHttpRequest();
		xmlhttp.onreadystatechange=function() {
			if( xmlhttp.readyState==4 && xmlhttp.status==200 ) {
				if( !(xmlhttp.responseText == "REQ_RECEIVED\n") ) {
					alert(xmlhttp.responseText);
				}
			}
			else if( xmlhttp.readyState==4 && !xmlhttp.status ) {
				alert("ERROR: Server cannot be reached");
			}
			else if( xmlhttp.readyState==4 ) {
				alert("UNKNOWN ERROR: "
					+" state:"+xmlhttp.readyState
					+" status:"+xmlhttp.status
					+" response:"+xmlhttp.responseText
				);
			}
		}

		xmlhttp.open("POST", "http://"+serverAddr+"/cgi-bin/cap/clientreq.pl", true);
		xmlhttp.send(req);
	}
}
