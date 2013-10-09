#!/usr/bin/perl -w
use strict;
use File::Temp qw/:POSIX/;

my $dts = shift @ARGV;
my @types = @ARGV;
my $max_nr_addrs = 64;
my $max_nr_compats = 4;

if (!defined $dts || $#types < 0) {
	print STDERR "Usage: gen-devtree-iomaps ".
		"<dts-file|-> <addr-type> [addr-types...]\n";
	exit 1;
}

my $dtb = tmpnam();
system "dtc -I dts -O dtb $dts -o $dtb";

my $g = join '|', map { $_ . '@' } @types;
my @devs = grep { /$g/ } `fdtget -l $dtb / 2>/dev/null`;

my %iomaps;
foreach my $dev (@devs) {

	chomp($dev);
	my ($type, $addr) = split /@/, $dev;

	if (!exists $iomaps{$type}) {

		my $compatible = `fdtget $dtb /$dev compatible 2>/dev/null`;
		chomp($compatible);
		my @compats = split ' ', $compatible;

		$iomaps{$type}{compats} = \@compats;
		$iomaps{$type}{addrs} = [$addr];
	} else {
		push @{ $iomaps{$type}{addrs} }, $addr;
	}
}
unlink $dtb;

print <<EOF;
/*
 * Generated file. See gen-devtree-iomaps.pl
 */
#include "iomaps.h"
EOF
print "\nconst struct iomap iomaps[] = {\n";
foreach my $type (keys %iomaps) {

	my $compats = $iomaps{$type}{compats};
	my $addrs = $iomaps{$type}{addrs};

	my $nr_compats = $#{ $compats } + 1;
	if ($nr_compats > $max_nr_compats) {
		print STDERR "$type has $nr_compats compats, but iomaps can ".
			"only support up to $max_nr_compats.\n";
		splice @{ $compats }, $max_nr_compats;
	}

	@{ $addrs } = sort @{ $addrs };

	my $nr = $#{ $addrs } + 1;
	if ($nr > $max_nr_addrs) {
		print STDERR "$type has $nr addrs, but iomaps can ".
			"only support up to $max_nr_addrs.\n";
		$nr = $max_nr_addrs;
		splice @{ $addrs }, $nr;
	}

	@{ $addrs } = map { $_ = sprintf '0x%.8x', hex($_) } @{ $addrs };

	print "{\n";
	print "\t.type = \"$type\",\n";

	print "\t.compats = {";
	foreach my $compat (@{ $compats }) {
		print " \"$compat\",";
	}
	print " NULL, },\n";

	print "\t.nr = $nr,\n";
	print "\t.addrs = {";
	if ($nr < 5) {
		print ' ';
		print join ', ', @{ $addrs };
		print ", },\n";
	} else {
		print "\n";
		for (my $i = 0; $i < $nr; $i += 5) {
			print "\t\t";
			my $j = $i;
			while ($j < $i + 4 && $j < $nr - 1) {
				print $addrs->[$j] . ", ";
				++$j;
			}
			print $addrs->[$j] . ",\n";
		}
		print "\t},\n";
	}
	print "},\n";
}
print "{\n\t.type = NULL,\n},\n";
print "};\n";
exit 0;
