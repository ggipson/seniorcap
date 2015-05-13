#!/usr/bin/perl

# File Name: archive.pl
# Author: Grant Gipson
# Date Last Edited: May 14, 2011
# Description: Archiver component for Senior CAP Project

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
	$xmlref->{pipes}->{pipes_archiver} or
	errf("unable to get archiver pipe name from XML");
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
#  ARCHIVER BODY
################################################################################

# open XML file to retrieve configuration settings
my $xmlfile = "/var/cap/capconf.xml";
-e $xmlfile or die "XML configuration file $xmlfile does not exist\n";
$xmlref = XMLin($xmlfile);

# open error log
my $errlogname = $xmlref->{log_files}->{archiver_log}
  or die "unable to get name of error log from XML\n";
open($errlog, ">>", $errlogname)
  or die "failed to open $errlogname\n";
err("NEW LOG SESSION",'I');

# read messages from Master Program
my $errcount_rd=0; # number of read errors which have occurred
my $readerr_max=20;

# prepare for archiving
my $archive_dir = $xmlref->{components}->{archiver_dir} or
    errf("could not get location for archives",'E');
chdir($archive_dir) or
    errf("could not change to archive directory",'E');

err("entering message loop",'I');

while( $errcount_rd < $readerr_max ) {
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
	case "MSG_ARCHIVE" {
	    # create archive from content in working directory
	    my $content_id = $body; # form %010d
	    system("zip -q $content_id.zip *");

	    # send message back to Master Program
	    pipe_out_open();
	    print $p_out "MSG_ARCHIVED\n0\n";
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
