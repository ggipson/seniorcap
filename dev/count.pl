#!/usr/bin/perl

$total=0;

open(LSFILE, "count.out");
while($filename = <LSFILE>) {
	$count = 0;

	open(SRCFILE, $filename);
	chomp($filename);
	foreach $line (<SRCFILE>) {
		$count++;
	}

	$total += $count;
	print "$filename\t$count\n";
}

print "TOTAL\t$total\n";
