#!/bin/sh

set -e
set -u

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <arg1> <arg2>"
  exit 1
fi

dir=$(dirname "$1")
file=$(basename "$1")

if [ ! -d "$1" ]; then
	mkdir -p "$dir"
	touch "$dir/$file"
fi

if [ ! -f "$1" ]; then
	echo "File does not exist and could not be created"
	exit 1
fi

WRITEFILE=$1
WRITESTR=$2

echo "$WRITESTR" > "$WRITEFILE"
