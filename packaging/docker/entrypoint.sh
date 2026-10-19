#!/bin/sh
set -eu

fatal() {
  echo "aria2-next-entrypoint: $*" >&2
  exit 1
}

validate_id() {
  name=$1
  value=$2

  case "$value" in
    ''|*[!0-9]*)
      fatal "$name must be a non-negative numeric ID"
      ;;
  esac
}

PUID=${PUID:-}
PGID=${PGID:-}
validate_id PUID "$PUID"
validate_id PGID "$PGID"

if [ "$PUID" -eq 0 ] || [ "$PGID" -eq 0 ]; then
  echo "aria2-next-entrypoint: warning: running with root UID or GID grants elevated access to mounted host paths" >&2
fi

mkdir -p /downloads /config /var/lib/aria2-next
chown "$PUID:$PGID" /downloads
chown -R "$PUID:$PGID" /config /var/lib/aria2-next

for path in /downloads /config /var/lib/aria2-next; do
  gosu "$PUID:$PGID" test -w "$path" ||
    fatal "$path is not writable by $PUID:$PGID"
done

gosu "$PUID:$PGID" touch /var/lib/aria2-next/session.txt

if [ ! -e /config/aria2.conf ]; then
  gosu "$PUID:$PGID" cp /usr/share/aria2-next/aria2.conf /config/aria2.conf
fi

gosu "$PUID:$PGID" test -r /config/aria2.conf ||
  fatal "/config/aria2.conf is not readable by $PUID:$PGID"

# gosu sets HOME from the passwd entry, which a numeric PUID usually lacks
# (HOME=/), so the engine's state directory is given after the switch.
exec gosu "$PUID:$PGID" env HOME=/var/lib/aria2-next aria2-next "$@"
