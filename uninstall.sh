#!/bin/sh
set -e
F=/usr/libexec/cups/filter/rastertolabeltspl

if [ -e "$F.intel" ]; then
    sudo mv -f "$F.intel" "$F"
    echo "Removed. The original driver is back."
elif [ -e "$F" ] && grep -q "needs the original filter" "$F"; then
    sudo rm -f "$F"
    echo "Removed."
else
    echo "Nothing to remove."
fi
