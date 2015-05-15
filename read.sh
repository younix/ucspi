#!/bin/sh

set -eu

echo "cat /dev/fd/${1:-6} > ${2:-/dev/fd/1}"
cat /dev/fd/${1:-6} > ${2:-/dev/fd/1}
