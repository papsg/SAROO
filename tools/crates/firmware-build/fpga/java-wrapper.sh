#!/bin/bash
# paps-g: preserve the original vendor JRE and one interpreter for IP helpers.
if test "${SAROO_QEMU:-0}" = 1; then
    exec /saroo/qemu-x86_64-static /saroo/original-jre/bin/java "$@"
fi
exec /saroo/original-jre/bin/java "$@"
