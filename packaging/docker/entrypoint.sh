#!/bin/sh
set -eu

if [ ! -e /config/aria2.conf ]; then
  cp /usr/share/aria2-next/aria2.conf /config/aria2.conf
fi

exec aria2-next "$@"
