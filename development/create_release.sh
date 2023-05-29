#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "No version number supplied"
    exit 1
fi
VERSION=$1

# get the path from the second argument or use the default
if [ $# -eq 2 ]
  then
    PATH=$2
  else
    PATH=../
fi


VERSION_NO=v${VERSION}
read -r -d '' RELEASE_NOTES << EOM
First Line Text
Second Line Text
Third Line Text
EOM

gh release create $VERSION --title "$VERSION" --notes "$RELEASE_NOTES"
