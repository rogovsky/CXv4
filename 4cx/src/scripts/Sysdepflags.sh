#!/bin/sh

######################################################################
#                                                                    #
#  Sysdepflags.sh                                                    #
#    Determines the OS type and some system-dependent parameters     #
#                                                                    #
#  Result:                                                           #
#    Prints a string to the standard output in a format:             #
#      <PARAM>=value {<PARAM>=value}                                 #
#                                                                    #
######################################################################


#=====================================================================
#  Determine the OS type
#
#  Result:
#      $OS becomes one of:
#          "LINUX", "BSDI", "FREEBSD", "OPENBSD",
#          "IRIX", "OSF", "UNIXWARE", "SUNOS", "SOLARIS",
#          "CYGWIN", "INTERIX',
#          or "UNKNOWN"
#
#  Reserved OS names:
#      "NETBSD", "MINIX", "LYNXOS", "BEOS",
#      "ULTRIX", "AIX", "HPUX", "SCO", "QNX", "NEXTSTEP", "UNICOS"
#

UNAME=`uname -s`
OS="UNKNOWN"

if   (echo $UNAME | grep -i "linux"   >/dev/null)
then
    OS="LINUX"
# Cygwin should be as at top as possible, to minimize number of exec()s ;-)
elif (echo $UNAME | grep -i "cygwin" >/dev/null)
then
    OS="CYGWIN"
elif (echo $UNAME | grep -i "interix" >/dev/null)
then
    OS="INTERIX"
elif (echo $UNAME | grep -i "openbsd" >/dev/null)
then
    OS="OPENBSD"
elif (echo $UNAME | grep -i "bsd/386" >/dev/null) ||
     (echo $UNAME | grep -i "bsd/os"  >/dev/null)
then
    OS="BSDI"
elif (echo $UNAME | grep -i "freebsd" >/dev/null)
then
    OS="FREEBSD"
elif (echo $UNAME | grep -i "irix"    >/dev/null)
then
    OS="IRIX"
elif (echo $UNAME | grep -i "osf"     >/dev/null)
then
    OS="OSF"
elif (echo $UNAME | grep    "UNIX_SV" >/dev/null)
then
    OS="UNIXWARE"
elif (echo $UNAME | grep -i "sun"     >/dev/null)
then
    OS="SUNOS"
    OSVER=`uname -r | awk 'BEGIN {FS="."}{print $1}'`
    if test $OSVER -ge 5
    then
        OS="SOLARIS"
    fi
fi


#=====================================================================
#  Determine the CPU type
#
#  Result:
#      $CPU becomes one of:
#          "X86", "X86_64", "SPARC", "MIPS", "ALPHA" or "UNKNOWN"
#
#  Reserved CPU names:
#      "PPC", "ARM", "M68K", "M88K", "VAX"
#

CPUOPT="-m"
if [ $OS = "SOLARIS"  -o  $OS = "IRIX" ]
then
    CPUOPT="-p"
fi

CPU="UNKNOWN"
PLATFORM=`uname $CPUOPT`

if   (echo $PLATFORM | grep -i "i.86"    >/dev/null)
then
    CPU="X86"
elif (echo $PLATFORM | grep -i "x86.64"  >/dev/null)
then
    CPU="X86_64"
# "x86" is reported by Interix.  It is below X86_64 in order not to hide it.
elif (echo $PLATFORM | grep -i "x86"  >/dev/null)
then
    CPU="X86"
elif (echo $PLATFORM | grep -i "sparc"   >/dev/null)
then
    CPU="SPARC"
elif (echo $PLATFORM | grep -i "sun"     >/dev/null)
then
    CPU="SPARC"
elif (echo $PLATFORM | grep -i "mips"    >/dev/null)
then
    CPU="MIPS"
elif (echo $PLATFORM | grep -i "alpha"   >/dev/null)
then
    CPU="ALPHA"
fi


#=====================================================================
# Finally, print the flags

echo "<OS>=$OS" "<CPU>=$CPU"
