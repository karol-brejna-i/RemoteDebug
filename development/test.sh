#!/bin/bash

# Default values
project_path=""
version=""
release_notes=""

# Function to show help
show_help() {
    echo "Usage: ./test.sh [OPTIONS] COMMAND"
    echo "Options:"
    echo "  --help              Show help"
    echo "  --project-path      Path to the project"
    echo "  --version           Version number"
    echo "  --release-notes     Release notes"
    echo ""
    echo "Commands:"
    echo "  update              Update version number and release notes"
    echo "  create              Create a new release"
    echo "  publish             Publish a new release"
    echo "  check               Check if the version number is updated in all files and if the release notes are not empty"
}

# Function to update version number and release notes
update() {
    # Add your logic here to update the version number and release notes
    echo "Updating version number and release notes..."
}

# Function to create a new release
create() {
    # Add your logic here to create a new release
    echo "Creating a new release..."
}

# Function to publish a new release
publish() {
    # Add your logic here to publish a new release
    echo "Publishing a new release..."
}

# Function to check version number and release notes
check() {
    echo "Checking version number and release notes..."
    validation_result=1

    # check version number
    VERSION1=$(grep -oP '(?<=version": ")[0-9.]+' ${project_path}/library.json)
    VERSION2=$(grep -oP '(?<=VERSION ")[0-9.]+' ${project_path}/src/RemoteDebugCfg.h)
    VERSION3=$(grep -oP '(?<=version=)[0-9.]+' ${project_path}/library.properties)

    print_message "Version number in library.json:         $VERSION1" $GREY
    print_message "Version number in src/RemoteDebugCfg.h: $VERSION2" $GREY
    print_message "Version number in library.properties:   $VERSION3" $GREY

    if [ "$VERSION1" != "$VERSION2" ] || [ "$VERSION1" != "$VERSION3" ] || [ "$VERSION2" != "$VERSION3" ]; then
        print_message "Warning: version number is not updated in all files!" $RED
        validation_result=0
    fi

    version=$VERSION1
    escaped_version=$(echo "$version" | sed 's/\./\\./g')
    RELEASE_NOTES=$(grep -oPzo "(?s)(?<=## \[$escaped_version).*?(?=\## \[|$)" ${project_path}/CHANGELOG.md | sed -n '2,$p')

    if [ -z "$RELEASE_NOTES" ]; then
        print_message "Warning: release notes are empty!" $RED
        validation_result=0
    else
        print_message "Release notes:" $GREY
        print_message "$RELEASE_NOTES" $GREY
    fi

    if [ $validation_result -eq 1 ]; then
        print_message "Version number and release notes are valid." $GREEN
    else
        print_message "Version number and release notes are not valid." $RED
        exit 1
    fi

}

# this function gets basic color codes for printing in terminal (if the terminal supports them):
# RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE, NC (No Color)
get_color_definitions() {
    if [ -t 1 ] && [ -t 2 ] && [ -n "$TERM" ] && [ "$TERM" != "dumb" ] && [ "$TERM" != "emacs" ]; then
        RED='\033[0;31m'
        GREEN='\033[0;32m'
        YELLOW='\033[0;33m'
        BLUE='\033[0;34m'
        MAGENTA='\033[0;35m'
        CYAN='\033[0;36m'
        WHITE='\033[0;37m'
        GREY='\033[0;90m'
        NC='\033[0m' # No Color
    else
        RED=''
        GREEN=''
        YELLOW=''
        BLUE=''
        MAGENTA=''
        CYAN=''
        WHITE=''
        GREY=''
        NC=''
    fi
}

print_message() {
    if [ -z "$2" ]; then
        echo "$1"
    else
        echo -e "$2$1${NC}"
    fi
}


# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help)
            show_help
            exit 0
            ;;
        --project-path)
            project_path="$2"
            shift 2
            ;;
        --version)
            version="$2"
            shift 2
            ;;
        --release-notes)
            release_notes="$2"
            shift 2
            ;;
        *)
            command="$1"
            shift
            ;;
    esac
done

# Get color definitions
get_color_definitions
# message constants
MSG_NO_COMMAND="${RED}No command specified.${NC}"
MSG_NO_VERSION="${RED}Version number is required.${NC}"

if [ -z "$command" ]; then
    echo -e $MSG_NO_COMMAND
    show_help
    exit 1
fi

print_message "Running $command command."
print_message "Using the following options:" $GREY
print_message " project path: $project_path" $GREY
print_message " version: $version" $GREY
print_message " release notes: $release_notes" $GREY


# Execute the specified command
case "$command" in
    update)
        # Check if version number is given
        if [ -z "$version" ]; then
            echo -e $MSG_NO_VERSION
            exit 1
        fi
        update
        ;;
    create)
        create
        ;;
    publish)
        publish
        ;;
    check)
        check
        ;;
    *)
        echo "Invalid usage. Use --help for usage instructions."
        exit 1
        ;;
esac
