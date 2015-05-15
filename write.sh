#!/bin/sh

set -eu

/usr/bin/env > /dev/fd/${1:-7}
