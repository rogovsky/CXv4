#!/usr/bin/perl -w

use strict;


$#ARGV == 3  ||  die "Usage: $0 -o OUTFILE.h INFILE VARNAME\n";

my $OUTFILE = $ARGV[1];
my $INFILE  = $ARGV[2];
my $VARNAME = $ARGV[3];

my $c;
my $a;

open(SRC, "<" . $INFILE)   ||  die "$0: can't open $INFILE for input\n";
open(DST, ">" . $OUTFILE)  ||  die "$0: can't open $OUTFILE for output\n";

print DST "char *$VARNAME=\n";

while (<SRC>)
{
    chomp;
    print DST "    \"";
    foreach $c (split //)
    {
        $a = ord($c);
        if    ($c eq "\\") {print DST "\\\\";}
        elsif ($c eq "\"") {print DST "\\\"";}
        elsif ($c eq "\'") {print DST "\\\'";}
        elsif ($c eq "\a") {print DST "\\a";}
        elsif ($c eq "\b") {print DST "\\b";}
        elsif ($c eq "\f") {print DST "\\f";}
        elsif ($c eq "\r") {print DST "\\r";}
        elsif ($c eq "\t") {print DST "\t";}
        elsif ($a eq 11)   {print DST "\\v";}
        elsif ($a eq 127  ||
               $a <  32)   {printf DST "\\x%02x", $a;}
        else               {print DST $c;}
                  
    }
    print DST "\\n\"\n";
}

print DST ";\n";

close DST;
close SRC;
