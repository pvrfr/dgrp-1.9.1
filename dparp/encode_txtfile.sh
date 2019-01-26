#!/bin/sh

usage() {
	echo "USAGE: encode_textfile.sh <infile> <arrayname>" >&2
}

infile="$1"
arrayname="$2"

if [ -z "$infile" ]
then
	usage
	exit
fi

if [  \! -f "$infile" ]
then
	echo "FAILED: couldn't find input file: \"$infile\"" >&2
	usage
	exit
fi

echo "/******************************************************"
echo " **                                                    "
echo " **  \"Compiled\" array for contents of the file       "
echo " **  $infile"
echo " **                                                    "
echo " **                                                    "
echo " ******************************************************/"
echo ""

echo "char *$arrayname[] = {"

tmpfile="/tmp/tmpfil$$"
rm -f $tmpfile

cat $infile | sed -e 's/	/        /g' | \
              sed -e 's/"/\\\\"/g' | \
              sed -e 's/^/"/' | \
              sed -e 's/$/"/' \
    > $tmpfile

while read inline
do
	echo "	$inline,"
done < $tmpfile

echo "};"
echo ""

echo "int ${arrayname}_len = sizeof(${arrayname}) / sizeof(char *);"
echo ""
