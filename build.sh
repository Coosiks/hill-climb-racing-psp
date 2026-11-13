#!/bin/bash
set -e
export PSPDEV=/opt/pspdev
export PATH=/opt/pspdev/bin:$PATH
export PSPSDK=$(psp-config --pspsdk-path)

cd "$(dirname "$0")"

echo "Cleaning..."
make clean
echo "Building HCR PSP..."
make -j4 "$@"
