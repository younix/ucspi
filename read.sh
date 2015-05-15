#!/bin/sh

set -eu

cat "/dev/fd/${1:-6}" > "${2:-/dev/fd/1}"
