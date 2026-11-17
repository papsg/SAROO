"""Select only one SAROO_SAVE_GENERATION branch for source extraction tests."""


def menu_source_variant(source, staged=False):
    out = []
    frames = []
    active = True
    for line in source.splitlines(keepends=True):
        directive = line.strip()
        if directive.startswith((b'#ifdef ', b'#ifndef ', b'#if ')):
            stage = directive in (b'#ifdef SAROO_SAVE_GENERATION',
                                  b'#ifndef SAROO_SAVE_GENERATION')
            frames.append((stage, active, directive.startswith(b'#ifndef ')))
            if stage:
                first = not directive.startswith(b'#ifndef ')
                active = active and (first if staged else not first)
            elif active:
                out.append(line)
        elif directive == b'#else' and frames:
            stage, parent, first_is_default = frames[-1]
            if stage:
                active = parent and (first_is_default if staged else not first_is_default)
            elif active:
                out.append(line)
        elif directive == b'#endif' and frames:
            stage, parent, _ = frames.pop()
            if not stage and active:
                out.append(line)
            active = parent
        elif active:
            out.append(line)
    if frames:
        raise AssertionError('unbalanced menu conditionals')
    return b''.join(out)
