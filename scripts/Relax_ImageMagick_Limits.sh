#!/bin/bash
set -euo pipefail

# Must be run with sudo/root.
if [[ $EUID -ne 0 ]]; then
  echo "Please run as root: sudo $0"
  exit 1
fi

# Targets (tune as needed)
MEMORY="8GiB"
MAP="12GiB"
DISK="32GiB"
AREA="512MP"
WIDTH="32KP"
HEIGHT="32KP"

CANDIDATES=(
  "/etc/ImageMagick-6/policy.xml"
  "/etc/ImageMagick/policy.xml"
  "/etc/ImageMagick-7/policy.xml"
  "/usr/local/etc/ImageMagick-6/policy.xml"
  "/usr/local/etc/ImageMagick/policy.xml"
  "/usr/local/etc/ImageMagick-7/policy.xml"
)

ensure_policy () {
  local file="$1"
  local name="$2"
  local value="$3"

  # If policy for this resource exists, replace its value (regardless of existing units/formatting).
  # Otherwise, insert a new policy line just after <policymap> (common structure).
  if grep -Eq "domain=\"resource\"[^>]*name=\"$name\"" "$file"; then
    perl -0777 -i -pe \
      "s/(<policy\\s+domain=\"resource\"[^>]*name=\"$name\"[^>]*value=\")[^\"]*(\"[^>]*\\/?>)/\${1}$value\${2}/g" \
      "$file"
  else
    perl -0777 -i -pe \
      "s/(<policymap>\\s*)/\$1  <policy domain=\"resource\" name=\"$name\" value=\"$value\"\\/>\n/si" \
      "$file"
  fi
}

enable_coder () {
  local file="$1"
  local pattern="$2"

  # If there's a coder deny for this pattern, convert it to read|write.
  if grep -Eq "<policy\\s+domain=\"coder\"[^>]*pattern=\"$pattern\"" "$file"; then
    perl -0777 -i -pe \
      "s/<policy\\s+domain=\"coder\"([^>]*?)pattern=\"$pattern\"([^>]*?)rights=\"none\"([^>]*?)\\/>/<policy domain=\"coder\"\\1pattern=\"$pattern\"\\2rights=\"read|write\"\\3\\/>/g" \
      "$file"
  fi
}

for FILE in "${CANDIDATES[@]}"; do
  if [[ -f "$FILE" ]]; then
    echo "Found policy.xml: $FILE"
    cp -f "$FILE" "$FILE.bak.$(date +%Y%m%d_%H%M%S)"

    ensure_policy "$FILE" "memory" "$MEMORY"
    ensure_policy "$FILE" "map" "$MAP"
    ensure_policy "$FILE" "disk" "$DISK"
    ensure_policy "$FILE" "area" "$AREA"
    ensure_policy "$FILE" "width" "$WIDTH"
    ensure_policy "$FILE" "height" "$HEIGHT"

    # Optional: allow PDF if you need it (keep your original intent)
    enable_coder "$FILE" "PDF"

    echo "Updated limits in: $FILE"
  fi
done

echo "Done. Current ImageMagick policy output (may reflect multiple files depending on version):"
command -v identify >/dev/null 2>&1 && identify -list policy || true

