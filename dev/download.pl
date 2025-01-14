#!/usr/bin/perl

# File Name: download.pl
# Author: Grant Gipson
# Date Last Edited: May 12, 2011
# Description: Downloader component for Senior CAP Project

use strict;
use warnings;
use XML::Simple;
use Switch;
use Fcntl;

# global variables
my $xmlref;
my $p_in;
my $p_out;
my $errlog;

# handles errors
sub err {
    (my $message,my $type) = @_;

    # get time of error
    (my $sec,my $min,my $hour,my $mday,my $mon,my $year,my $wday,
     my $yday,my $isdst) = localtime(time());
    $mon += 1;
    $year += 1900;

    # append to error log if it is open
    if( $errlog ) {
	print $errlog sprintf(
	    "[%02d/%02d/%d %02d:%02d:%02d] -%s- %s\n", 
	    $mon, $mday, $year, $hour, $min, $sec, 
	    $type, $message
	);
    }
}

# handles fatal errors
sub errf { &err(@_,'F'); exit -1; }

# open pipe to read messages
sub pipe_in_open {
    my $pipename = 
	$xmlref->{pipes}->{pipes_dir} . 
	$xmlref->{pipes}->{pipes_downloader} or
	errf("unable to get downloader pipe name from XML");
    -p $pipename or
	errf("$pipename does not exist or is not a pipe: $!");

    sysopen($p_in, $pipename, O_RDONLY) or
	errf("unable to open $pipename to read messages: $!");
    err("opened pipe $pipename",'I');
}

# close pipe for reading messages
sub pipe_in_close {
    close($p_in) or errf("failed to close pipe for receiving messages");
    err("closed receiving pipe",'I');
}

# open pipe for sending messages to Master Program
sub pipe_out_open {
    my $pipename = 
	$xmlref->{pipes}->{pipes_dir} . 
	$xmlref->{pipes}->{pipes_master} or 
	errf("unable to get master pipe name from XML");
    -p $pipename or 
	errf("$pipename does not exist or is not a pipe: $!");
    open($p_out, ">>", $pipename) or
	errf("unable to open $pipename to write messages: $!");
    err("opened pipe $pipename",'I');
}
# close pipe for sending messages
sub pipe_out_close {
    close($p_out) or errf("failed to close pipe for sending messages");
    err("closed sending pipe",'I');
}

################################################################################
#  DOWNLOADER BODY
################################################################################

# open XML file to retrieve configuration settings
my $xmlfile = "/var/cap/capconf.xml";
-e $xmlfile or die "XML configuration file $xmlfile does not exist\n";
$xmlref = XMLin($xmlfile);

# open error log
my $errlogname = $xmlref->{log_files}->{downloader_log}
  or die "unable to get name of error log from XML\n";
open($errlog, ">>", $errlogname)
  or die "failed to open $errlogname\n";
err("NEW LOG SESSION",'I');

# read messages from Master Program
my $errcount_rd=0; # number of read errors which have occurred
my $readerr_max=20;

# prepare for downloading
my $download_dir = $xmlref->{components}->{downloader_dir} or
    errf("could not get location for downloads",'E');
chdir($download_dir) or
    errf("could not change to download directory",'E');

err("entering message loop",'I');

while( $errcount_rd < $readerr_max ) {
		my $wgetFAIL = 0; # used to indicate a fatal error from WGet

    # open pipe for receiving messages
    pipe_in_open();
    err("reading in a message",'I');

    my $command;
    my $length;
    my $body;

    # message command header
    if( !($command=<$p_in>) ) {
	err("failed to read command header from message: $!",'E');
	$errcount_rd++; next;
    }
    chomp($command);

    # message body length
    if( !($length=<$p_in>) ) {
	err("failed to read body length from $command message: $!",'E');
	$errcount_rd++; next;
    }
    chomp($length);

    # is there a body?
    if( $length ) {
	# read in message body
	if( !read($p_in, $body, $length) ) {
	    err("failed to read body of length $length from $command "
		. "message: $!",'E');
	    $errcount_rd++; next;
	}
	chomp($body);
    }

    pipe_in_close(); # close until next message
    err("read in message $command with body of length $length",'I');

    # process message
    switch( $command ) {
	case "MSG_QUIT" {
	    # terminate this component
	    exit 0;
	}
	case "dS" {
	    # download a single URL
	    if(	system("wget -o wget.out $body") == -1 ) {
				err("failed to download URL: $!",'E');
	      next;
	    }

			# open WGet output to check result
	    if( !open(WGET, "<", "wget.out") ) {
				err("failed to open download output: $!",'E');
				next;
	    }
	    while( <WGET> ) {
				# check for name of download file
				last if /Saving to:/;

				# check for errors
				if( /ERROR 403: Forbidden/ ) {
					err("forbidden (403) to download content from $body", 'F');
					$wgetFAIL = 1;
				}
				elsif( /ERROR/ ) {
					err("found an error on this line: '$_' when downloading content from $body", 'E');
				}
			}
	    close(WGET);

			my $html; # name of downloaded HTML file
			my $title; # title of page

			if( !$wgetFAIL ) {
		    # trim off unwanted characters
		    @_ = split(/`/);
		    $html = $_[1];
		    chomp($html);
		    chop($html);

		    # search for title in HTML
		    $title=$html;
		    if( !open(HTML, "<", $html) ) {
					err("could not open $html: $!",'W');
		    }
		    else {
					while( <HTML> ) { last if /<title>/; }
					close(HTML);

					if( $_ ) {
				    # trim off HTML tags
				    /<title>/;
				    $_ = "$'"; # trim off left tag
				    m|</title>|;
				    $_ = "$`"; # trim off right tag

				    $title=$_ if $_;
					}
		    }
			} # WGet success

			# prepare message for Master Program
			my $send_command;
	    my $send_body;
			if( !$wgetFAIL ) {
				$send_command = "MSG_DOWNLOADED";
				$send_body = "$html\n$title\n";
			}
			else { 
				$send_command = "MSG_DOWNLOADFAIL";
				$send_body = "";
			}
	    my $send_length = length($send_body);

	    # send confirmation back to Master Program
	    pipe_out_open();
	    print $p_out "$send_command\n$send_length\n$send_body" or
				err("failed to send message to Master Program: $!",'E');
	    err("sent $send_command to master",'I');
	    pipe_out_close();
	}
	else {
	    # what's going on??
	    err("received unknown message $command",'W');
	}
    }
}

# check for errors
if( $errcount_rd==$readerr_max ) {
    err("maximum number of allowed errors ($readerr_max) reading from pipe " 
	. "has occurred; terminating",'F');
}
