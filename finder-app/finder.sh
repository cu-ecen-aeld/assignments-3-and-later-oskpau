#!/bin/bash

set -e
set -u

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <arg1> <arg2>"
  exit 1
fi

if [ ! -d "$1" ]; then
	echo "Error: '$1' is not a valid directory."
	exit 1
fi

FILESDIR=$1
SEARCHSTR=$2

echo "FILESDIR: $FILESDIR"
echo "SEARCHSTR: $SEARCHSTR"

X=$(grep -hc $SEARCHSTR $(grep -lwr $SEARCHSTR $FILESDIR) | awk 'END { print NR }')
Y=$(grep -hc $SEARCHSTR $(grep -lwr $SEARCHSTR $FILESDIR) | awk '{ sum += $1 } END { print sum }')

echo "The number of files are $X and the number of matching lines are $Y"
