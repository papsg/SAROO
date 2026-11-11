"""Extract the actual load-disc arm and its reply for host fixtures."""


def load_case(raw):
    start = raw.index(b'case SSCMD_LOADDISC:')
    case = raw[start:raw.index(b'\tcase SSCMD_CHECK:', start)]
    if b'goto save_reply;' in case:
        reply = raw.index(b'    save_reply:', start)
        case += raw[reply:raw.index(b'\tcase SSCMD_STARTUP:', reply)]
    return case
