#!/usr/bin/perl 

#
# Copyright (C) 2011, Marvell International Ltd.
# All Rights Reserved.
#

# A script to analyse the allocation/free/realloc logs and generate useful
# statistics. 

# We will keep following associative arrays as a database during our analysis:
# (1) Malloc handles <=> allocated size [ handleArr ]
#            This is needed during free because free does not give us the
# size information.
# (2) Allocated size <=> Cumulative Count (does not decrement on free) [ cumulativeCnt ]
#            This keeps a count of total how many allocations occured for a
# particular size.
# (3) Allocated size <=> Current Count (internal purpose) [ curCnt ]
# (4) Allocated size <=> Max active ( Max total allocations needed at any 
# time) [ maxActiveCnt ]


use constant PROGNAME=>"heap_analyse.pl";
use constant MINARGS=>1;
use constant MAXARGS=>1;

if (@ARGV < MINARGS || @ARGV > MAXARGS) {
    printf "Usage: " . PROGNAME . " log-filename\n";
    exit 1;
}

$LOGFILE=$ARGV[0];


$MAX_ALLOCATION=0;
$CURRENT_ALLOCATION=0;

printf "Open log file: $LOGFILE  ... \n";
open LOG, $LOGFILE or die $!;

#$lineCnt = 1;
while (<LOG>) {
#    printf ("Processing line: %d\n", $lineCnt++);
    chomp;
    if( /^MDC/ ) {
	if (/^MDC:A/) {
	    my ($magic, $type, $handle, $size) = split(':', $_);
	    if(defined $handleArr{$handle}) {
		printf "Fatal: Handle repeated\n";
#		exit 1;
	    }
	    $handleArr{$handle} = $size;


	    $CURRENT_ALLOCATION += $size;
	    if ($MAX_ALLOCATION < $CURRENT_ALLOCATION) {
		$MAX_ALLOCATION = $CURRENT_ALLOCATION;
	    }
	    
	    # update 2nd array
	    if(defined $cumulativeCnt{$size}) {
		$cumulativeCnt{$size} += 1;
	    }
	    else {
		$cumulativeCnt{$size} = 1;
		$curCnt{$size} = 1;
		$maxActiveCnt{$size} = 1;
		next;
	    }
	    
	    # update 3rd array
	    $curCnt{$size} += 1;
	    # update 4th array
	    if ($maxActiveCnt{$size} < $curCnt{$size}) {
		$maxActiveCnt{$size} = $curCnt{$size};
	    }
	}
	elsif (/^MDC:F/) {
	    my ($magic, $type, $handle) = split(':', $_);
	    if(!defined $handleArr{$handle}) {
		printf "Fatal: Free: Handle not present\n";
#		exit 1;
	    }
	    my $size = $handleArr{$handle};

	    $CURRENT_ALLOCATION -= $size;

	    # do not update 2nd array as it is cumulative count
	    # update 3rd array
	    $curCnt{$size}--;
	    # no need to update 4th array as it is max count
	    # delete associative array element
	    delete $handleArr{$handle};
	}
	elsif (/^MDC:R/) {
	    my ($magic, $type, $oldHandle, $newHandle, $newSize) = split(':', $_);
	    if(!defined $handleArr{$oldHandle}) {
		printf "Fatal: Realloc: Handle not present\n";
#		exit 1;
	    }
	    my $oldSize = $handleArr{$oldHandle};
	    # do not update 2nd array as it is cumulative count
	    # update 3rd array
	    $curCnt{$oldSize}--;
	    # no need to update 4th array as it is max count

	    $CURRENT_ALLOCATION += ($newSize - $oldSize);
	    if ($MAX_ALLOCATION < $CURRENT_ALLOCATION) {
		$MAX_ALLOCATION = $CURRENT_ALLOCATION;
	    }

	    # check if we need to create a new element
	    if ($newHandle ne $oldHandle) {
		delete $handleArr{$oldHandle};
		$handleArr{$newHandle} = $newSize;
	    }
	    else {
		$handleArr{$oldHandle} = $newSize;
	    }
	    
	    # update 2nd array
	    if(defined $cumulativeCnt{$newSize}) {
		$cumulativeCnt{$newSize} += 1;
	    }
	    else {
		$cumulativeCnt{$newSize} = 1;
		$curCnt{$newSize} = 1;
		$maxActiveCnt{$newSize} = 1;
		next;
	    }
	    
	    # update 3rd array
	    $curCnt{$newSize} += 1;
	    # update 4th array
	    if ($maxActiveCnt{$newSize} < $curCnt{$newSize}) {
		$maxActiveCnt{$newSize} = $curCnt{$newSize};
	    }	    
	}
	else {
	    printf "IGNORE: $_\n";
	}
    }
}



# We have analysis data ready ... print the statistics

# loop over the associative arrays and print values
printf "\n\n\033[1;34mCumulative allocations for each size\033[0m";
printf "\n------------------------------------------------------\n";
printf "Allocation size      Allocation count     Total size\n";
printf "------------------------------------------------------\n";

foreach $size (sort { $a <=> $b } keys %cumulativeCnt) {
    printf ("%-20d %-20d %-d\n", $size, $cumulativeCnt{$size}, ($size * $cumulativeCnt{$size}));
}


printf "\n\n\033[1;34mMax active allocations at any time for each size\033[0m";
printf "\n------------------------------------------------------\n";
printf "Allocation size      Allocation count     Total size\n";
printf "------------------------------------------------------\n";

foreach $size (sort { $a <=> $b } keys %maxActiveCnt) {
    printf ("%-20d %-20d %-d\n", $size, $maxActiveCnt{$size}, ($size * $maxActiveCnt{$size}));
}


if (keys( %handleArr )) {
    printf "\n\n\033[1;31mMemory leaks\033[1;34m for each size\033[0m";
    printf "\n------------------------------------------------------\n";
    printf "Allocation size      Alloqcation count     Total size\n";
    printf "------------------------------------------------------\n";
    
    $totalLeak=0;
    foreach $size (sort { $a <=> $b } values %handleArr) {
	$leakSize = ($size * $curCnt{$size});
	printf ("%-20d %-20d %-d\n", $size, $curCnt{$size}, $leakSize);
	$totalLeak += $leakSize;
    }
}
printf "\n------------------------------------------------------\n";
printf ("Max amount of memory allocated = %-6d bytes\n", $MAX_ALLOCATION);
printf ("\033[1;36mTotal memory leaked            = %-6d bytes\033[0m\n",
	$totalLeak);
printf "------------------------------------------------------\n";
