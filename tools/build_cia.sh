#!/usr/bin/env bash
# Build installable CIA from Gerald ELF plus HOME Menu banner/icon.
set -euo pipefail
cd "$(dirname "$0")/.."
for app in makerom bannertool; do
  if ! command -v "$app" >/dev/null 2>&1; then
    echo "Missing $app. Run tools/install_cia_tools.sh first." >&2
    exit 1
  fi
done
if [[ ! -s gerald.elf || ! -s gerald.3dsx ]]; then
  echo 'First run make to produce gerald.elf and gerald.3dsx.' >&2
  exit 1
fi
mkdir -p build dist
bannertool makebanner -i meta/banner.png -a meta/banner.wav -o build/gerald.bnr
bannertool makesmdh -s 'Gerald the Frog' -l 'Pondside Deluxe' -p 'Gerald Homebrew' -i meta/icon.png -o build/gerald.icn
makerom -f cia -o dist/gerald.cia -target t -exefslogo \
  -elf gerald.elf -rsf app.rsf \
  -DAPP_TITLE='Gerald the Frog' \
  -DAPP_PRODUCT_CODE=CTR-P-GRLD \
  -DAPP_UNIQUE_ID=0xF7A21 \
  -DAPP_ROMFS=romfs \
  -icon build/gerald.icn -banner build/gerald.bnr
cp gerald.3dsx dist/gerald.3dsx
cp meta/icon.png meta/banner.png dist/
test -s dist/gerald.cia
echo 'Build complete: dist/gerald.cia and dist/gerald.3dsx'
