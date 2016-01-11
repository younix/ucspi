#!/bin/sh

set -eu

cat <&6 >"$1"
