#!/usr/bin/perl

# File Name: SeniorCAP.pl
# Author: Grant Gipson
# Date Last Edited: March 14, 2011
# Description: Downloads archived Senior CAP files from server

# format date for renamed archive
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
$thisdate = sprintf("%02d%02d%d", $mon+1, $mday, $year%100);
$rename = "seniorcap" . $thisdate . "\.tar";

# download archive and rename it
chdir "/home/grant/seniorcap/backup";
print `wget http://149.143.3.106:8080/dev/seniorcap.tar`;
print `mv seniorcap.tar $rename`;

