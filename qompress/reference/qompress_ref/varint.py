def encode_uvarint(value):
    value = int(value)
    if value < 0:
        raise ValueError("uvarint cannot encode negative values")
    out = bytearray()
    while True:
        b = value & 0x7F
        value >>= 7
        if value:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)

def decode_uvarint(data, offset):
    value = 0
    shift = 0
    for _ in range(10):
        if offset >= len(data):
            raise ValueError("truncated uvarint")
        b = data[offset]
        offset += 1
        value |= (b & 0x7F) << shift
        if not (b & 0x80):
            return value, offset
        shift += 7
    raise ValueError("uvarint too long")
