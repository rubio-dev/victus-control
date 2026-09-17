#!/bin/bash
# Fetch the stock hp-wmi driver for a kernel series and add board 8BC8 to it.
#
# The driver source is not kept in this repository, the same way the upstream
# out-of-tree module is not: it is the kernel's own file and it belongs to the
# kernel. Only the four-line difference lives here.
#
# Usage: ./fetch-source.sh [kernel-version]      (defaults to the running one)

set -euo pipefail

cd "$(dirname "$0")"

VERSION="${1:-$(uname -r | cut -d- -f1)}"
URL="https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/plain/drivers/platform/x86/hp/hp-wmi.c?h=v${VERSION}"

echo "Fetching hp-wmi.c for Linux ${VERSION}"
curl -fsSL -o hp-wmi.c "$URL"

echo "Adding board 8BC8"
if ! patch -p1 --forward < 8bc8.patch; then
    echo
    echo "The patch did not apply. The board table has been restructured before:"
    echo "in 7.2.5 the entry belongs in victus_s_thermal_profile_boards[], a"
    echo "dmi_system_id[] carrying per-board driver_data, while later trees use"
    echo "hp_wmi_feature_boards[] with *_board_params. Check where it goes in"
    echo "this version and adjust 8bc8.patch." >&2
    exit 1
fi

echo "Done. Build with: make"
