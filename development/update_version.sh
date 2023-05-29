#!/bin/bash
# check if version is given as argument
if [ $# -eq 0 ]
  then
    echo "No version number supplied"
    exit 1
fi
VERSION=$1

# get the path from the second argument or use the default
if [ $# -eq 2 ]
  then
    PROJECT_PATH=$2
  else
    PROJECT_PATH=../
fi

sed -i "s/\"version\": \"[0-9.]\+\"/\"version\": \"$VERSION\"/" $PROJECT_PATH/library.json
sed -i "s/VERSION \"[0-9.]\+\"/VERSION \"$VERSION\"/" $PROJECT_PATH/src/RemoteDebugCfg.h

