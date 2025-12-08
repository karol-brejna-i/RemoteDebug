#!/bin/bash

# this command has the following switches:
# --help: show help
# --project-path: path to the project
# --version: version number
# --release-notes: release notes
# and the following commands:
# update: update version number and release notes
# create: create a new release
# publish: publish a new release
# check: check if the version number is updated in all files and if the release notes are not empty

# check if --project-path is given as argument; use current dir as default
if [ $# -eq 0 ]
  then
    PROJECT_PATH=.
  else
    PROJECT_PATH=$2
fi

# show help
function help {
  echo "Usage: update_version.sh <command> [<options>]"
  echo "Commands:"
  echo "  update: update version number and release notes"
  echo "  create: create a new release"
  echo "  publish: publish a new release"
  echo "  check: check if the version number is updated in all files and if the release notes are not empty"
  echo "  show: show version number and release notes"
  echo "Options:"
  echo "  --help: show help"
  echo "  --project-path: path to the project"
  echo "  --version: version number"
  echo "  --release-notes: release notes"
}


# function for command check
function check {
    # check if version is updated in all files
    VERSION1=$(grep -oP '(?<=version": ")[0-9.]+' library.json)
    VERSION2=$(grep -oP '(?<=VERSION ")[0-9.]+' src/RemoteDebugCfg.h)
    VERSION3=$(grep -oP '(?<=version=)[0-9.]+' library.properties)

    echo "Version number in library.json:         $VERSION1"
    echo "Version number in src/RemoteDebugCfg.h: $VERSION2"
    echo "Version number in library.properties:   $VERSION3"

    if [ "$VERSION1" != "$VERSION2" ] || [ "$VERSION1" != "$VERSION3" ] || [ "$VERSION2" != "$VERSION3" ]; then
        USE_COLOR=1 # print warning in red
        # print warning in red if color is supported (don't use color otherwise)
        if [ -t 1 ] && [ -t 2 ] && [ -n "$TERM" ] && [ "$TERM" != "dumb" ] && [ "$TERM" != "emacs" ]; then
            RED='\033[0;31m'
            NC='\033[0m' # No Color
        else
            RED=''
            NC=''
        fi
        echo -e "${RED}Warning: version number is not updated in all files!${NC}"


        exit 1
    fi

}


# check if version is given as argument
if [ $# -eq 0 ]
  then
    echo "No version number supplied"
    echo "Usage: update_version.sh <version> [<project_path>]"
    exit 1
fi
VERSION=$1