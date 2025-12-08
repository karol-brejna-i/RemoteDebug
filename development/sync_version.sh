#!/bin/bash
#
# Sync version from src/RemoteDebugCfg.h to library.json and library.properties
#
# Usage: ./development/sync_version.sh
#
# The version in src/RemoteDebugCfg.h is the single source of truth.
# Run this script after updating the version in RemoteDebugCfg.h
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Extract version from RemoteDebugCfg.h
VERSION=$(grep '^#define VERSION' src/RemoteDebugCfg.h | sed 's/.*"\(.*\)".*/\1/')

if [ -z "$VERSION" ]; then
    echo "Error: Could not extract version from src/RemoteDebugCfg.h"
    echo "Make sure there is a line like: #define VERSION \"x.y.z\""
    exit 1
fi

echo "Syncing version: $VERSION"

# Update library.json
if [ -f library.json ]; then
    sed -i "s/\"version\": \".*\"/\"version\": \"$VERSION\"/" library.json
    echo "  ✓ Updated library.json"
else
    echo "  ✗ library.json not found"
fi

# Update library.properties
if [ -f library.properties ]; then
    sed -i "s/^version=.*/version=$VERSION/" library.properties
    echo "  ✓ Updated library.properties"
else
    echo "  ✗ library.properties not found"
fi

echo ""
echo "Done! Version synchronized to $VERSION"
echo ""
echo "Verify the changes:"
echo "  git diff library.json library.properties"
