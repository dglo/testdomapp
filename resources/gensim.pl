#!/usr/bin/perl

# gensim.pl
# John Jacobsen, NPX Designs, Inc., jacobsen\@npxdesigns.com
# Started: Fri Sep 24 16:47:39 2004

package MY_PACKAGE;
use strict;

my $height = 40;

print "USHORT simspe[128] = {\n   ";
foreach my $isamp (0..127) {
    my $x = int(exp(-($isamp-20)**2/3**2)*$height);
    printf "%4d", $x;
    if($isamp < 127) { 
	print ",";
	if(!(($isamp+1)%8)) { print "\n   "; }
    } else {
	print "\n};\n";
    }
}
__END__

