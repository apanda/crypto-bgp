#!/usr/bin/perl
#use warnings;
#use strict;
use List::Util qw/shuffle/;
use List::Util qw[min max];

# ./build.pl <#peers> <degree> <input file <topo file>
# for example, ./build.pl 3000 30 test.txt test_topo.txt

my $usage = "./build.pl <#peers> <degree> <input file <topo file>";

my $numPeer = $ARGV[0] or die $usage;
my $actual_degree = $ARGV[1] or die $usage;
my $file = $ARGV[2] or die $usage;
my $topofile = $ARGV[3] or die $usage;

my $ISP_PROBABILITY = 0.15;

my $degree = int($actual_degree/2); #approximate
print "$actual_degree\n";

my @isISP;
my @ISPList;
my @ASList;

$" = ",";
$, = ",";

# create list of ISPs and ASs
while( $#ISPList < 0 ) { # have at least 1 ISP!
	for my $i(0..$numPeer-1) {
		$isISP[$i] = (rand()<=$ISP_PROBABILITY)? 1:0;
		if($isISP[$i]) {push( @ISPList, $i+1);} else {push( @ASList, $i+1);}
	}
}

my @peerList;
for my $i(0..$numPeer-1) {
	 my $obj = {
		'ID' => $i+1,
		'Type' => $isISP[$i],
		'num' => 0,
		'Pref' => ()
	 };
	 push( @peerList, $obj);
}

# connect ISPs with random ISPs and ASs
# ~1/2 of the connections should be to other ISPs
my $jj;
my $rmv = 0;
foreach my $id(@ISPList) {
	my $ISP = $peerList[ $id-1 ];
	
	# other ISPs
	my @tempISPList = @ISPList;
	splice(@tempISPList, $rmv++, 1);
	@tempISPList = (shuffle(@tempISPList))[ 0..($degree-1) ];
	
	# randomly generate list of ASs to connect to
	my @tempASList = (shuffle(@ASList))[ 0..($degree-1) ];
	
	my @tempList;
	
	foreach my $ASID (@tempASList) {
		my $AS = $peerList[$ASID-1];
		push(@{$$AS{'Pref'}}, $id);
		push(@tempList, $ASID);
	}
	
	foreach my $xID (@tempISPList) {
		my $xISP = $peerList[$xID-1];
		my $xTemp = $$xISP{'num'};
		if( $xTemp < $degree ) {
			push(@{$$xISP{'Pref'}}, $id);
			$$xISP{'num'} = $xTemp + 1;
			push(@tempList, $xID);
		}
	}
	#@tempList = (@tempASList, @tempISPList);
	@tempList = shuffle(@tempList);
	@tempList = map {$_ ? $_ : ()} @tempList;
	$$ISP{'num'} += $#tempList + 1;
	
	@{$$ISP{'Pref'}} = @tempList;
}


foreach my $ASID(@ASList) {
	my $AS = $peerList[ $ASID-1 ];
	my $k = @{$$AS{'Pref'}};
	if( $k == 0 ) {
	  # each AS must be connected to at least one ISP
		# link this AS to a random ISP
		my $id = $ISPList[rand @ISPList];
		push(@{$$AS{'Pref'}}, $id);
		
		my $ISP = $peerList[ $id-1 ];
		push(@{$$ISP{'Pref'}}, $ASID);
	}
}

print "\n";

my $count = 1;
my $pnum = 1;
my $dx = int(rand($#peerList));
my $x = 0;
my $y = 0;
open (TOPOFILE, ">$topofile"); # TOPOFILE LOCATION
print TOPOFILE "nitems=$numPeer\n";

open (MYFILE, ">$file"); # INPUT LOCATION
print MYFILE "\n\nnitems=$numPeer\n";
print MYFILE "Destination=$dx\n";
		
foreach my $peer(@peerList) {
	if($$peer{'Type'}==1) { #for calculating avg, if needed
		$x += $$peer{'num'};
	}
	else {
		$y += $$peer{'num'};
	}
	
	print MYFILE "\n";
	print MYFILE $count . "_peerID=" . $$peer{'ID'} . "\n";
	print MYFILE $count . "_peerType=" . $$peer{'Type'} . "\n";
	print MYFILE $count . "_Preferences=";
	my @xx = @{$$peer{'Pref'}};
	
	print MYFILE @xx;
	print TOPOFILE $$peer{'ID'} . "=";
	print TOPOFILE @xx;
	print TOPOFILE "\n";
	
	print MYFILE "\n";
	if($$peer{'Type'}==1) {
		foreach my $i(@xx) {
			print MYFILE $count . "_" . "$i=";
			my @tempList = @xx;
			my $j=0; $j++ until ( $j > $#tempList or ($tempList[$j]==$i) ); splice(@tempList, $j, 1); # remove i
			my @tempList = shuffle(@tempList[ 0..int(rand($#tempList)) ]);
			print MYFILE "@tempList\n";
		}
	}
	
	$count++;
}

