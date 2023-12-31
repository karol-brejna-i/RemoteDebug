#!/bin/bash
if [ $# -eq 0 ]
  then
    echo "No version number supplied"
    exit 1
fi
VERSION=$1

VERSION_NO=v${VERSION}
read -r -d '' RELEASE_NOTES << EOM
* Clean simple example a bit
* Add library.properties (for Arduino IDE)
* Bump version to 4.0.1
EOM

gh repo set-default karol-brejna-i/RemoteDebug
gh release create $VERSION --title "$VERSION" --notes "$RELEASE_NOTES"
