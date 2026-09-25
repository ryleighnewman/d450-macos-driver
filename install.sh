#!/bin/sh
set -e
cd "$(dirname "$0")"
F=/usr/libexec/cups/filter/rastertolabeltspl

if [ -e "$F" ] && ! grep -q "needs the original filter" "$F" && file "$F" | grep -q arm64; then
    echo "Your printer driver already runs natively. Nothing to do."
    exit 0
fi

sudo sh -c "
    set -e
    install -m 755 -o root -g wheel rastertolabeltspl $F.new
    xattr -c $F.new
    if [ -e $F ] && ! grep -q 'needs the original filter' $F; then mv -f $F $F.intel; fi
    mv -f $F.new $F
"
echo "Installed. Print a test label."
