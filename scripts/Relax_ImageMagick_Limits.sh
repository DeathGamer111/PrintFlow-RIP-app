#!/bin/bash

# No sudo logic here — assume it's run with elevated privileges

# Desired limits
declare -A LIMITS=(
    [memory]="2GiB"
    [map]="4GiB"
    [disk]="8GiB"
    [width]="10000"
    [height]="10000"
    [area]="10000"
)

# Common policy.xml paths
CANDIDATES=(
    "/etc/ImageMagick-6/policy.xml"
    "/etc/ImageMagick/policy.xml"
    "/usr/local/etc/ImageMagick/policy.xml"
)

for FILE in "${CANDIDATES[@]}"; do
    if [[ -f "$FILE" ]]; then
        echo "Found policy.xml at: $FILE"

        # Backup the file
        cp "$FILE" "$FILE.bak"
        echo "Backed up original file to: $FILE.bak"

        # Update resource limits
		for key in "${!LIMITS[@]}"; do
            desired="${LIMITS[$key]}"
            current=$(grep -oP "<policy domain=\"resource\" name=\"$key\" value=\"\K[^\"]+" "$FILE")

            if [[ -n "$current" ]]; then
                if [[ "$current" =~ ^[0-9]+$ && "$current" -lt "${desired//[^0-9]/}" ]]; then
                    echo "Raising $key from $current to $desired"
                    sed -i "s|<policy domain=\"resource\" name=\"$key\" value=\"[^\"]*\"/>|<policy domain=\"resource\" name=\"$key\" value=\"$desired\"/>|" "$FILE"
                else
                    echo "$key already set to $current (ok)"
                fi
            else
                echo "Adding $key limit"
                sed -i "/<policymap>/a <policy domain=\"resource\" name=\"$key\" value=\"$desired\"/>" "$FILE"
            fi
        done
        
        # Remove or update PDF restrictions
        if grep -q '<policy domain="coder" rights="none" pattern="PDF"' "$FILE"; then
            echo "Enabling PDF support by modifying policy."
            sed -i 's/<policy domain="coder" rights="none" pattern="PDF" \/>/<policy domain="coder" rights="read|write" pattern="PDF" \/>/' "$FILE"
        fi

        echo "✅ Successfully updated ImageMagick resource limits."
        exit 0
    fi
done

echo "❌ Could not find policy.xml in standard locations."
exit 1
