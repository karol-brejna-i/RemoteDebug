#!/bin/bash

library_json_path="../library.json"
library_ini_path="../library.properties"

#######################
# extract description data from library.json
description=$(jq '.description' "$library_json_path")
# trim leading and trailing quotes
description="${description%\"}"
description="${description#\"}"
# extract first sentence
first_sentence=$(echo "${description}" | cut -d '.' -f 1)
first_sentence+="."
# keep the rest as summary
rest=$(echo "${description}" | cut -d'.' -f 2-)
# remove leading and trailing new line codes
trimmed_left=$(echo "$rest" | sed 's/^[\\n]*//')
trimmed=$(echo "$trimmed_left" | sed 's/[\\n]*$//')

#######################
# use parameters if provided
if [ -z "$1" ]; then
    echo "No arguments provided, using reading sentence and paragraph from library.json"
    sentence=$first_sentence
    paragraph=$trimmed
else
    sentence="$1"
    if [ -z "$2" ]; then
        echo "No description provided, getting it from library.json"
        paragraph=$trimmed
    else
        paragraph="$2"
    fi
fi

#######################
# read library metadata from library.json
# Read data from library.json
name=$(jq -r '.name' "$library_json_path")
version=$(jq -r '.version' "$library_json_path")
repository_url=$(jq -r '.repository.url' "$library_json_path")

# get the authors and maintainers
authors=$(jq -r '.authors | map(if .email then "\(.name) <\(.email)>" else .name end) | join(",")' "$library_json_path")
maintainer=$(jq -r '.authors | map(select(.maintainer == true) | if .email then "\(.name) <\(.email)>" else .name end) | join(",")' "$library_json_path")

#######################
# Generate library.ini content
content="name=$name
version=$version
author=$authors
maintainer=$maintainer
sentence=$sentence
paragraph=$paragraph
category=Communication
url=$repository_url
architectures=*
"

# Add dependencies
depends=$(jq -r '.dependencies | keys_unsorted | map(split("/")[1]) | join(",")' "$library_json_path")

if [ ! -z "$depends" ]; then
  content+="depends=$depends"
fi

echo "$content" > "$library_ini_path"

echo "library.properties generated successfully at: $library_ini_path"
