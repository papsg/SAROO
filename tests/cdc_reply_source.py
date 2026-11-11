"""Use the actual CD reply helpers in host register fixtures."""


def command_reply_source(raw):
    code = b''
    for signature in (b'static inline u32 reply_pair(',
                      b'static __attribute__((noinline,noclone)) void command_reply('):
        if signature in raw:
            start = raw.index(signature)
            code += raw[start:raw.index(b'\n}', start) + 2] + b'\n'
    return code.replace(b'noinline,noclone', b'noinline')
