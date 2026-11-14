#!/bin/bash
set -e
export PATH="/opt/pspdev/bin:$PATH"
cd /mnt/c/Projects/hcr-psp
make -f Makefile -j4
ls -lh EBOOT.PBP
