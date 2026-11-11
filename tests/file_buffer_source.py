"""Extract the MCU transfer helper with a host physical-address boundary."""


def file_buffer_source(raw):
    start = raw.find(b'int file_buffer_io(')
    if start < 0:
        return b''
    code = raw[start:raw.index(b'\n}', start) + 2]
    assert code.count(b'(void *)physical') == 2
    return code.replace(b'(void *)physical', b'FILE_TEST_POINTER(physical)') + b'\n'


def transfer_buffer_source(handler, module):
    if b'file_buffer_io(' not in handler:
        return b''
    code = file_buffer_source(module)
    assert code
    return b'#define HAS_MAPPED_FILE_BUFFER 1\n' + code
