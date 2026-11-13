#!/bin/sh
# Exercise the actual menu linker script with all four BSS tail alignments.
set -eu

prefix=${SH2_PREFIX:-sh-elf-}
script=${1:-Firm_Saturn/ldscript}
temp=$(mktemp -d "${TMPDIR:-/tmp}/menu-stack.XXXXXXXX")
trap 'rm -rf "$temp"' EXIT

symbol()
{
    address=$(awk -v symbol="$1" '$3 == symbol { print $1 }' "$temp/symbols")
    test -n "$address"
    printf '%d\n' "0x$address"
}

for tail in 4 5 6 7; do
    cat >"$temp/entry.S" <<EOF
.section .text
.global start
start: nop
.section .bss
.balign 4
.space $tail
EOF
    "${prefix}as" --isa=sh2 -big "$temp/entry.S" -o "$temp/entry.o"
    "${prefix}ld" -T "$script" --oformat=elf32-sh -o "$temp/entry.elf" "$temp/entry.o"
    "${prefix}nm" -n "$temp/entry.elf" >"$temp/symbols"
    start=$(symbol __stack_start)
    end=$(symbol __stack_end)
    if [ "$(( (start | end) & 3 ))" -ne 0 ]; then
        echo "BSS tail $tail leaves misaligned R15" >&2
        exit 1
    fi
    test "$((start - $(symbol __bss_start)))" -eq "$(( (tail + 3) & ~3 ))"
    test "$((end - start))" -eq 16384
    test "$end" -eq "$(symbol __bss_end)"
    test "$end" -le "$(( $(symbol __ip_start) + 0x20000 - 4 ))"
done
echo "All four BSS tail alignments retain the 16 KiB stack and aligned R15"
