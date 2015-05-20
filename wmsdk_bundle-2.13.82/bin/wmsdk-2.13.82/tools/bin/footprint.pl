#!/usr/bin/perl -w

# Print out the detailed footprint description of a image.
# 
# The algorithm works as follows:
# Step 0: Skip untill you find a string that starts with .text. This is the start of the
# image address space
# Step 1: Since each symbol is in a section of its own, look for .text.symbolname or 
# .data.symbolname and identify the symbol name and the correspond section (text/data). 
# Note that such symbols start with a space as the first character
# Step 2: In some cases, symbols are introduced by the linker script. These do not have a
# space as the first character. Nor is the object name (.a or .o) available for such symbols.
# Deal with this.
# Step 3: Now we have the section (text,data) and symbol name. The same line or the line 
# after that will contain the size and the object name (.a or .o). Read this.
# Now that we have all the information, create a hash object with all this extracted information.
# Display information in a format requested by the user.
# 
# 
# Note: This ignores any fill-bytes and such minor chunks in the analysis. Hence there is a slight
# difference in the output reported by this script and that of 'size'
# 
use strict;

use File::Basename;
use Getopt::Std;

my $fname;
my $start_flag = 0;
my %options;
sub VERSION_MESSAGE()
{
    print "";
}

sub HELP_MESSAGE() 
{
    print "\nUsage: footprint.pl -m map_file [ -l library ] [ -s text|rodata|data|bss ]\n\n";
    print "By default prints the footprint impact of all the libraries.\n";
    print "When used with '-l', explicitly lists symbols from a \n";
    print "given library that are included in the image.\n";
    exit;
}

getopts('hs:l:m:', \%options);

HELP_MESSAGE if $options{h};

if ( ! defined($options{m})) {
    HELP_MESSAGE;
}

$fname = $options{m};
if (! -e $fname) {
    print "Error: Can't access $fname\n";
    exit 1;
}

open TFILE, $fname;
# skip upto .text
while (my $readl = <TFILE>) {
    if ($readl =~ /^\.text/) {
	$start_flag = 1;
	last;
    }
}

if (!$start_flag) {
    die "No .text found";
}

# Process .text
# This contains both .text and .rodata
#
my $mode = "text";
my $symbol = "default";
my $size = "-1";
my $object = "default";

my %bigdata;

while (my $readl = <TFILE>) {
# This can either be a .text line with size and all on the same line
# or it could be a remnant of the previous .text line
    $readl =~ s/\r\n$/\n/;
#    print "new line : $readl";

    if ($readl =~ /^ \.([^\.\s]*)\.([\S]*)/) {
	# This line contains the region and symbol name at the least
	$mode = $1;
	$symbol = $2;
    } elsif ($readl =~ /^ COMMON/) {
	$mode = "common";
	} elsif ($readl =~ m/_actual_heap_size/) {
	# to find heap size #
	$mode = "bss";
	$object = "linker_script";
	my @heap_data = split /\s+ /, $readl;
	$size = hex ($heap_data[1]);
	$symbol = "heap";
	} elsif ($readl =~ /^\.([^\.\s]*)\s*0x[0-9a-f]*\s*0x([0-9a-f]*)/) {
	# We need to use this to determine some components that are 
	# directy addedin the linker script. These of course, are accounted
	# as bss
	$mode = "bss";
	$symbol = $1;
	if ($symbol eq "data" || $symbol eq "bss" || $symbol eq "text" ||
	    $symbol =~ /debug_/) {
	    # ignore aggregate or debug symbols
	    next;
	}
	if (hex($2) == 0) {
	    # ignore symbols with 0 size
	    next;
	}
	# Ensure that the logic below detects the object as linker_script
	$readl .= "linker_script";
    } elsif ($readl =~ /^ \.([^\.\s]*)\s/) {
	$mode = $1;
	$symbol = "unknown";
    } else {
#	print "Ignoring line $readl\n";
	next;
    }

    if ($readl =~ /0x[0-9a-f]*\s*0x([0-9a-f]*)\s(.*)/) {
	$size = hex($1);
	$object = $2;
    } else {
	# This line is spilled over to the next line, process that.
	$readl = <TFILE>;
    $readl =~ s/\r\n$/\n/;
#	print "new line: $readl";
	if ($readl =~ /0x[0-9a-f]*\s*0x([0-9a-f]*)\s(.*)/) {
	    $size = hex($1);
	    $object = $2;
	}
    }

    if ($size == 0) {
	# No use for symbols with 0 size
	next;
    }
    if ($object =~ /([\S]*\.a)/) {
	$object = basename($1);
    }
#    print "$mode $symbol $size $object\n";
    $bigdata{$object}{$mode}{size} += $size;
    $bigdata{$object}{$mode}{symbols}{$size} .= " $symbol($size)";
}

sub print_all()
{
    printf("%-30s %10s %10s %10s %10s %10s\n", "Library", "text", "rodata", "data", "bss", "common");
    printf("##########################\n");
    my %totals;
    foreach $object (sort keys %bigdata) {
	printf("%-30s", "$object:");
	if (defined($bigdata{$object}{text}{size})) {
	    printf(" %10s", " $bigdata{$object}{text}{size}");
	    $totals{text} += $bigdata{$object}{text}{size}
	} else { printf(" %10s", " 0"); }
	
	if (defined($bigdata{$object}{rodata}{size})) {
	    printf(" %10s",  " $bigdata{$object}{rodata}{size}");
	    $totals{rodata} += $bigdata{$object}{rodata}{size}
	} else { printf(" %10s",  " 0"); }
	
	if (defined($bigdata{$object}{data}{size})) {
	    printf(" %10s",  " $bigdata{$object}{data}{size}");
	    $totals{data} += $bigdata{$object}{data}{size}
	} else { printf(" %10s",  " 0"); }
	
	if (defined($bigdata{$object}{bss}{size})) {
	    printf(" %10s", " $bigdata{$object}{bss}{size}");
	    $totals{bss} += $bigdata{$object}{bss}{size}
	} else { printf(" %10s", " 0"); }
	
	if (defined($bigdata{$object}{common}{size})) {
	    printf(" %10s", " $bigdata{$object}{common}{size}");
	    $totals{common} += $bigdata{$object}{common}{size}
	} else { printf(" %10s", " 0"); }
	
	print "\n";
    }

    printf("##########################\n");
    printf("%-30s %10s %10s %10s %10s %10s\n", " ", "text", "rodata", "data", "bss", "common");
    printf("%-30s %10s %10s %10s %10s %10s\n", "Totals:", $totals{text}, $totals{rodata}, $totals{data}, $totals{bss}, $totals{common});
    my $final = $totals{text} + $totals{rodata} + $totals{data} + $totals{bss} + $totals{common};
    printf("%-30s %d", "Final:", $final);
    printf(" (%d KB )\n", $final / 1024);
    
    printf("##########################\n");
    printf("As per 'size' utility:\n");
    printf(".text = text + rodata = %d\n", $totals{text} + $totals{rodata});
    printf(".data = data = %d\n", $totals{data});
    printf(".bss = bss + common = %s\n", $totals{bss} + $totals{common});
}

sub print_library($)
{
    my ($library) = @_;
    my $syms;

    if (!defined($bigdata{$library})) {
	print "Error: No such library is in the map file\n";
	exit 1;
    }

    print "Library: $library\n\n";
    if (defined($bigdata{$library}{text}{symbols})) {
	print "text:\n";
	foreach $syms (sort {$b <=> $a} (keys (%{$bigdata{$library}{text}{symbols}}))) {
	    print "$bigdata{$library}{text}{symbols}{$syms}";
	}
	print "\n\n";
    }
    if (defined($bigdata{$library}{rodata}{symbols})) {
	print "rodata:\n";
	foreach $syms (sort {$b <=> $a} (keys (%{$bigdata{$library}{rodata}{symbols}}))) {
	    print "$bigdata{$library}{rodata}{symbols}{$syms}";
	}
	print "\n\n";
    }
    if (defined($bigdata{$library}{data}{symbols})) {
	print "data:\n";
	foreach $syms (sort {$b <=> $a} (keys (%{$bigdata{$library}{data}{symbols}}))) {
	    print "$bigdata{$library}{data}{symbols}{$syms}";
	}
	print "\n\n";
    }
    if (defined($bigdata{$library}{bss}{symbols})) {
	print "bss:\n";
	foreach $syms (sort {$b <=> $a} (keys (%{$bigdata{$library}{bss}{symbols}}))) {
	    print "$bigdata{$library}{bss}{symbols}{$syms}";
	}
	print "\n\n";
    }
    print "common:\n Couldn't infer. Refer to map.txt\n\n";
}

sub print_section($)
{
    my ($sec) = @_;
    my %sec_info;

#    print ":$sec:\n";
    foreach my $l (keys %bigdata) {
#	print "lib:$l:\n";
	if (! defined($bigdata{$l}{$sec}{symbols})) {
	    next;
	}
	foreach my $size (keys %{$bigdata{$l}{$sec}{symbols}}) {
	    $sec_info{$size} .= $bigdata{$l}{$sec}{symbols}{$size};
	}
    }

    foreach my $size (sort {$b <=> $a} (keys %sec_info)) {
	print "$sec_info{$size}";
    }
}

if (defined $options{l}) {
    # print information specific to this library
    print_library($options{l});
} elsif (defined $options{s}) {
    # print information specific to this library
    print_section($options{s});
} else {
    print_all();
}
