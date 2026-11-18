"""Check that the linked high-WRAM CD-auth routine fits its copied extent."""
import re
import sys
from pathlib import Path


def check(path):
    lines = Path(path).read_text().splitlines()
    symbols = [
        (i, int(match.group(1), 16), match.group(2))
        for i, line in enumerate(lines)
        if (match := re.fullmatch(r"([0-9a-f]{8}) <([^>]+)>:", line))
    ]
    matches = [n for n, (_, _, name) in enumerate(symbols)
               if name == "_cdc_auth_hram"]
    assert len(matches) == 1
    index = matches[0]
    assert index + 1 < len(symbols)
    begin_line, begin, _ = symbols[index]
    end_line, end, _ = symbols[index + 1]
    assert begin % 4 == 0 and 0 < end - begin <= 0x400
    body = lines[begin_line + 1:end_line]
    assert any("\trts\t" in line for line in body)
    for line in body:
        assert not re.search(r"\t(?:jsr|bsr|jmp)\t", line), line
        for target in re.findall(r"<([^>]+)>", line):
            assert target == "_cdc_auth_hram" or target.startswith(
                "_cdc_auth_hram+"), line
        for value in re.findall(r"!\s+([0-9a-f]+)(?:\s|$)", line):
            assert not 0x02000000 <= int(value, 16) < 0x02020000, line
    print(f"relocated CD routine: {end - begin} bytes, self-contained")


if __name__ == "__main__":
    assert len(sys.argv) == 2
    check(sys.argv[1])
