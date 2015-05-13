#!/usr/bin/perl

chdir("/var/cap/content");
opendir(DIR, ".");
@filelist = readdir(DIR);

foreach $_ (@filelist) {
    next if !/content/;
    ($id,$ext) = split(/\./);
    system("mv $id.$ext $id.html");
}
