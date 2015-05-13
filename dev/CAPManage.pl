#!/usr/bin/perl

# File Name: CAPManage.pl
# Author: Grant Gipson
# Date Last Edited: April 22, 2011
# Description: Used to manage Senior CAP Project system

use strict;
use warnings;
use Switch;
use XML::Simple;
use Fcntl ":flock";

# global variables
my $xmlfile;
my $pidfile;
my $master_program;
my $downloader;
my $archiver;

# opens pipe to Master Program
sub pipe_master_open {
  # check for XML file
  my $xmlfile = "/var/cap/capconf.xml";
  -e $xmlfile or die "XML configuration file $xmlfile does not exist\n";

  # open XML file
  my $xmlref = XMLin($xmlfile);

  # open pipe to Master Program
  my $pipename = 
    $xmlref->{pipes}->{pipes_dir} . 
    $xmlref->{pipes}->{pipes_master} or 
    die "Unable to get pipe name from XML\n";
  -p $pipename or 
    die "$pipename does not exist or is not a pipe: $!\n";
  open(my $pipe, ">>", $pipename) or
    die "unable to open pipe $pipename: $!\n";

  return $pipe;
}

# closes pipe to Master Program
sub pipe_master_close {
    my $pipe = $_[0];
    close($pipe) or die "unable to close pipe to Master Program\n";
}

# opens pipe to Downloader
sub pipe_download_open {
  # check for XML file
  my $xmlfile = "/var/cap/capconf.xml";
  -e $xmlfile or die "XML configuration file $xmlfile does not exist\n";

  # open XML file
  my $xmlref = XMLin($xmlfile);

  # open pipe to Downloader
  my $pipename = 
    $xmlref->{pipes}->{pipes_dir} . 
    $xmlref->{pipes}->{pipes_downloader} or 
    die "Unable to get pipe name from XML\n";
  -p $pipename or 
    die "$pipename does not exist or is not a pipe: $!\n";
  open(my $pipe, ">>", $pipename) or
    die "unable to open pipe $pipename: $!\n";

  return $pipe;
}

# closes pipe to Downloader
sub pipe_download_close {
    my $pipe = $_[0];
    close($pipe) or die "unable to close Downloader pipe\n";
}

# opens pipe to Archiver
sub pipe_archive_open {
  # check for XML file
  my $xmlfile = "/var/cap/capconf.xml";
  -e $xmlfile or die "XML configuration file $xmlfile does not exist\n";

  # open XML file
  my $xmlref = XMLin($xmlfile);

  # open pipe to Archiver
  my $pipename = 
    $xmlref->{pipes}->{pipes_dir} . 
    $xmlref->{pipes}->{pipes_archiver} or 
    die "Unable to get pipe name from XML\n";
  -p $pipename or 
    die "$pipename does not exist or is not a pipe: $!\n";
  open(my $pipe, ">>", $pipename) or
    die "unable to open pipe $pipename: $!\n";

  return $pipe;
}

# closes pipe to Archiver
sub pipe_archive_close {
    my $pipe = $_[0];
    close($pipe) or die "unable to close Archiver pipe\n";
}

# starts up an individual component
sub start_comp {
    my $procid;
    switch( my $comp=$_[0] ) {
	case "master" {
	    if( !($procid = fork()) ) {
		exec $master_program, $pidfile, $xmlfile or
		    die "failed to start Master Program\n";
	    }
	}	
	case "downloader" {
	    if( !($procid = fork()) ) {
		exec $downloader or die "failed to start Downloader\n";
	    }
	}
	case "archiver" {
	    if( !($procid = fork()) ) {
		exec $archiver or die "failed to start Archiver\n";
	    }
	}
	else {
	    print "unrecognized component $comp\n";
	}
    }
}

# stops and individual component
sub stop_comp {
    my $pipe;
    switch( my $comp=$_[0] ) {
	case "master" {
	    $pipe = pipe_master_open();
	    print $pipe "MSG_QUIT\n0\n";
	    pipe_master_close($pipe);
	}
	case "downloader" {
	    $pipe = pipe_download_open();
	    print $pipe "MSG_QUIT\n0\n";
	    pipe_download_close($pipe);
	}
	case "archiver" {
	    $pipe = pipe_archive_open();
	    print $pipe "MSG_QUIT\n0\n";
	    pipe_archive_close($pipe);
	}
	else {
	    print "unrecognized component $comp\n";
	}
    }
}

# kicks and individual component out of its blocked state
sub kick_comp {
    my $pipe;
    switch( my $comp=$_[0] ) {
	case "master" {
	    $pipe = pipe_master_open();
	    print $pipe "MSG_NULL\n0\n";
	    pipe_master_close($pipe);
	}
	case "downloader" {
	    $pipe = pipe_download_open();
	    print $pipe "MSG_NULL\n0\n";
	    pipe_download_close($pipe);
	}
	case "archiver" {
	    $pipe = pipe_archive_open();
	    print $pipe "MSG_NULL\n0\n";
	    pipe_archive_close($pipe);
	}
	else {
	    print "unrecognized component $comp\n";
	}
    }    
}

# handle given command from caller
switch( my $argCmd = $ARGV[0] or die "You must specify a command\n" )
{
    case ("start") {
	# check for XML file
	$xmlfile = "/var/cap/capconf.xml";
	-e $xmlfile or die "XML configuration file $xmlfile does not exist\n";

	# open XML file
	my $xmlref = XMLin($xmlfile);

	# get values for PID file and Master Program location
	$pidfile = $xmlref->{pid_file} or 
	    die "Cannot find PID file value in XML\n";
	$master_program = $xmlref->{components}->{master_program} or
	    die "Cannot find Master Program value in XML\n";
	$downloader = $xmlref->{components}->{downloader} or
	    die "cannot find Downloader path in XML\n";
	$archiver = $xmlref->{components}->{archiver} or
	    die "cannot find Archiver path in XML\n";

	# fork and then run component(s) using children
	switch( my $comp=($ARGV[1] or "all") ) {
	    case "all" {
		start_comp("master");
		start_comp("downloader");
		start_comp("archiver");
	    }
	    else {
		start_comp($comp);
	    }
	}
    }
    case ("stop") {
	switch( my $comp=($ARGV[1] or "all") ) {
	    case "all" {
		stop_comp("master");
		stop_comp("downloader");
		stop_comp("archiver");
	    }
	    else {
		stop_comp($comp);
	    }
	}
    }
    case "kick" {
	# kick component(s) out of blocking state
	switch( my $comp=($ARGV[1] or "all" ) ) {
	    case "all" {
		kick_comp("master");
		kick_comp("downloader");
		kick_comp("archiver");
	    }
	    else {
		kick_comp($comp);
	    }
	}
    }
    else {
	print "unrecognized command\n";
	exit 1;
    }
    exit 0;
}
