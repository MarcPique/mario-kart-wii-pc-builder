"""Read existing font coverage and transform message text without touching BMG commands."""
import struct


def message_parts(data):
    if len(data) % 2:
        raise ValueError('Odd UTF-16 message length')
    result = []
    start = offset = 0
    while offset < len(data):
        code = struct.unpack_from('>H', data, offset)[0]
        if code < 32 and code not in (9, 10, 13):
            if offset > start:
                result.append(data[start:offset].decode('utf-16-be'))
            if code == 0:
                if any(data[offset:]):
                    raise ValueError('Data after message terminator')
                return result
            if code != 0x1a:
                result.append(data[offset:offset + 2])
                offset += 2
                start = offset
                continue
            if offset + 4 > len(data):
                raise ValueError('Truncated BMG command')
            size = data[offset + 2]
            if size < 4 or size % 2 or offset + size > len(data):
                raise ValueError('Invalid BMG command size')
            result.append(data[offset:offset + size])
            offset += size
            start = offset
        else:
            offset += 2
    raise ValueError('Unterminated BMG message')


def transform_message(data, translate):
    output = bytearray()
    for part in message_parts(data):
        if isinstance(part, bytes):
            output.extend(part)
        else:
            translated = translate(part)
            if any(ord(c) < 32 and c not in '\r\n\t' for c in translated):
                raise ValueError('Translation injected a message command')
            output.extend(translated.encode('utf-16-be'))
    output.extend(b'\0\0')
    return bytes(output)


def font_characters(data):
    if len(data) < 16 or data[:4] != b'RFNT' or data[4:6] != b'\xfe\xff':
        raise ValueError('Unsupported font header')
    size, offset, sections = struct.unpack_from('>IHH', data, 8)
    if size != len(data) or offset < 16:
        raise ValueError('Invalid font bounds')
    codes = set()
    for _ in range(sections):
        if offset + 8 > len(data):
            raise ValueError('Truncated font section')
        tag, length = struct.unpack_from('>4sI', data, offset)
        end = offset + length
        if length < 8 or end > len(data):
            raise ValueError('Invalid font section')
        if tag == b'CMAP':
            if length < 22:
                raise ValueError('Truncated character map')
            first, last, kind = struct.unpack_from('>HHH', data, offset + 8)
            if last < first:
                raise ValueError('Invalid character range')
            if kind == 0:
                codes.update(range(first, last + 1))
            elif kind == 1:
                count = last - first + 1
                if 20 + count * 2 > length:
                    raise ValueError('Truncated character table')
                codes.update(first + i for i in range(count)
                             if struct.unpack_from('>H', data, offset + 20 + i * 2)[0] != 65535)
            elif kind == 2:
                count = struct.unpack_from('>H', data, offset + 20)[0]
                if 22 + count * 4 > length:
                    raise ValueError('Truncated character pairs')
                codes.update(struct.unpack_from('>H', data, offset + 22 + i * 4)[0]
                             for i in range(count))
            else:
                raise ValueError('Unsupported character map type')
        offset = end
    return codes


def visible_characters(message):
    return {ord(c) for part in message_parts(message) if isinstance(part, str)
            for c in part if ord(c) > 32}
