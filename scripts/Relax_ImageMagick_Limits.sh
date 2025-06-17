#!/bin/bash

# No sudo logic here — assume it's run with elevated privileges

# Common policy.xml paths
CANDIDATES=(
    "/etc/ImageMagick-6/policy.xml"
    "/etc/ImageMagick/policy.xml"
    "/usr/local/etc/ImageMagick/policy.xml"
)

for FILE in "${CANDIDATES[@]}"; do
    if [[ -f "$FILE" ]]; then
        echo "🔍 Found policy.xml at: $FILE"

        # Backup the file
        cp "$FILE" "$FILE.bak"
        echo "📦 Backed up original file to: $FILE.bak"

        # Update resource limits
        sed -i 's/<policy domain="resource" name="memory" value="[^"]*"\/>/<policy domain="resource" name="memory" value="2GiB"\/>/' "$FILE"
        sed -i 's/<policy domain="resource" name="map" value="[^"]*"\/>/<policy domain="resource" name="map" value="4GiB"\/>/' "$FILE"
        sed -i 's/<policy domain="resource" name="disk" value="[^"]*"\/>/<policy domain="resource" name="disk" value="8GiB"\/>/' "$FILE"

        echo "✅ Successfully updated ImageMagick resource limits."
        exit 0
    fi
done

echo "❌ Could not find policy.xml in standard locations."
exit 1
