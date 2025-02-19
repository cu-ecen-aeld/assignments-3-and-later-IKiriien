#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Usage: $0 <filesdir> <searchstr>"
	exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d $filesdir ]; then
	echo "Error: '$filesdir' is not a directory."
	exit 1
fi

files=$(find $filesdir -type f | wc -l)
matches=$(grep -rFh $searchstr $filesdir | wc -l)

echo "The number of files are $files and the number of matching lines are $matches"
