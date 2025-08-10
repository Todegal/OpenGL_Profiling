#!/usr/bin/env bash

# Safety first: fail on errors
set -euo pipefail

# Loop over files in current directory
for file in *; do
    # Skip if not a regular file
    [ -f "$file" ] || continue

    # Convert CamelCase → snake_case
    snake=$(echo "$file" | sed -E 's/([a-z0-9])([A-Z])/\1_\L\2/g')

    # Convert entire name to lowercase just in case
    snake=$(echo "$snake" | tr 'A-Z' 'a-z')

    # Only rename if it changed
    if [[ "$file" != "$snake" ]]; then
        if [[ -e "$snake" ]]; then
            echo "Skipping '$file' → '$snake' (target exists)"
        else
            mv "$file" "$snake"
            echo "Renamed '$file' → '$snake'"
        fi
    fi
done
