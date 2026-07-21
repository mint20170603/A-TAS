#!/usr/bin/env bash
set -euo pipefail

version=${1:-}
if [[ ! $version =~ ^[0-9]{12}$ ]]; then
    echo "Usage: $0 <12-digit-version>" >&2
    exit 1
fi

source_version=$(sed -nE 's/^[[:space:]]*#define[[:space:]]+A_TAS_VERSION[[:space:]]+([0-9]+).*$/\1/p' 'src/A-TAS 4.0.cpp')
if [[ $source_version != "$version" ]]; then
    echo "A_TAS_VERSION ($source_version) does not match release version ($version)." >&2
    exit 1
fi

release_paths=(
    7z.dll
    7z.exe
    A-TAS-Manager.exe
    CHANGELOG.md
    LICENSE.txt
    README.md
    libavz.dll
    Start-A-TAS.cmd
    update.ps1
    src
)

for path in "${release_paths[@]}"; do
    [[ -e $path ]] || { echo "Missing release path: $path" >&2; exit 1; }
done

dist=$(mktemp -d)
trap 'rm -rf "$dist"' EXIT
cp -a "${release_paths[@]}" "$dist/"

manifest="$dist/manifest.sha256"
: > "$manifest"
while IFS= read -r -d '' relative; do
    hash=$(sha256sum "$dist/$relative" | cut -d ' ' -f 1 | tr '[:lower:]' '[:upper:]')
    printf '%s  %s\n' "$hash" "$relative" >> "$manifest"
done < <(cd "$dist" && find . -type f ! -name manifest.sha256 -printf '%P\0' | sort -z)

cp "$manifest" manifest.sha256
rm -f A-TAS.zip
(cd "$dist" && zip -q -r "$OLDPWD/A-TAS.zip" .)
