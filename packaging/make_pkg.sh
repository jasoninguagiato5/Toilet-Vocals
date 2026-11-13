#!/usr/bin/env bash
# Builds dist/ToiletVocals-<version>.pkg from the built AU, VST3 and Standalone app.
# Env: AU, VST3, APP (paths to built bundles)
#      APP_SIGN_ID / INSTALLER_SIGN_ID (optional Developer ID identities)
set -euo pipefail
VERSION="${1:-1.0.0}"
ID=com.porcelainaudio.toiletvocals
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(pwd)/pkgwork"
rm -rf "$WORK" dist && mkdir -p "$WORK" dist

sign_bundle () {
  if [ -n "${APP_SIGN_ID:-}" ]; then
    codesign --force --deep --options runtime --timestamp --sign "$APP_SIGN_ID" "$1"
  else
    codesign --force --deep --sign - "$1"   # ad-hoc: required for Apple Silicon to load it
  fi
  codesign --verify --deep --strict "$1"
}

make_component () {  # name  bundle  install-dir
  local name="$1" bundle="$2" dest="$3"
  local root="$WORK/root-$name"
  mkdir -p "$root$dest"
  ditto "$bundle" "$root$dest/$(basename "$bundle")"
  sign_bundle "$root$dest/$(basename "$bundle")"
  # Component plist written by hand (pkgbuild --analyze doesn't recognise plugin
  # bundles). BundleIsRelocatable=false stops the installer from "relocating"
  # the bundle onto some other copy with the same bundle ID elsewhere on disk.
  cat > "$WORK/$name.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>BundleHasStrictIdentifier</key><true/>
    <key>BundleIsRelocatable</key><false/>
    <key>BundleIsVersionChecked</key><false/>
    <key>BundleOverwriteAction</key><string>upgrade</string>
    <key>RootRelativeBundlePath</key><string>${dest#/}/$(basename "$bundle")</string>
  </dict>
</array>
</plist>
PLIST
  pkgbuild --root "$root" --component-plist "$WORK/$name.plist" \
           --identifier "$ID.$name" --version "$VERSION" --install-location / \
           "$WORK/$name.pkg"
}

make_component au   "$AU"   "/Library/Audio/Plug-Ins/Components"
make_component vst3 "$VST3" "/Library/Audio/Plug-Ins/VST3"
HAS_APP=0
if [ -n "${APP:-}" ] && [ -d "$APP" ]; then
  make_component app "$APP" "/Applications"
  HAS_APP=1
fi

sed -e "s/__VERSION__/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
if [ "$HAS_APP" = 0 ]; then
  # drop the standalone choice if it wasn't built
  python3 - "$WORK/distribution.xml" <<'PY'
import re, sys
p = sys.argv[1]; s = open(p).read()
s = re.sub(r'\s*<line choice="app"/>', '', s)
s = re.sub(r'\s*<choice id="app".*?</choice>', '', s, flags=re.S)
s = re.sub(r'\s*<pkg-ref id="com.porcelainaudio.toiletvocals.app"[^>]*>[^<]*</pkg-ref>', '', s)
open(p, 'w').write(s)
PY
fi

cp "$HERE/welcome.html" "$WORK/"
OUT="dist/ToiletVocals-$VERSION.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
             --resources "$WORK" "$WORK/unsigned.pkg"

if [ -n "${INSTALLER_SIGN_ID:-}" ]; then
  productsign --sign "$INSTALLER_SIGN_ID" "$WORK/unsigned.pkg" "$OUT"
else
  cp "$WORK/unsigned.pkg" "$OUT"
fi
pkgutil --payload-files "$OUT" | grep -E "component|vst3|\.app$" | head -20 || true
ls -la dist
