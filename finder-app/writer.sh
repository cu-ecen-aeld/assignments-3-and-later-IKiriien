#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Usage $0 <writefile> <writestr>"
	exit 1
fi

writefile=$1
writestr=$2

dir=$(dirname $writefile)
if ! mkdir -p $dir; then
	echo "Error: Could not create directory path '$dir'."
	exit 1
fi

if ! echo $writestr > $writefile; then
	echo "Error: Could not write to file '$writefile'."
	exit 1
fi
