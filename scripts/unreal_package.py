"""Bounded, read-only UE1 package table and embedded script text reader."""
import struct
from pathlib import Path


class Reader:
    def __init__(self, data, pos=0):
        self.data, self.pos = data, pos

    def take(self, count):
        if count < 0 or self.pos < 0 or self.pos + count > len(self.data):
            raise ValueError('Package read outside file')
        data = self.data[self.pos:self.pos+count]
        self.pos += count
        return data

    def unpack(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def compact(self):
        first = self.take(1)[0]
        value = first & 0x3f
        more, shift = first & 0x40, 6
        for n in range(4):
            if not more:
                return -value if first & 0x80 else value
            byte = self.take(1)[0]
            value |= (byte & (0x1f if n == 3 else 0x7f)) << shift
            more, shift = byte & 0x80, shift + 7
        if more:
            raise ValueError('Invalid compact index')
        return -value if first & 0x80 else value

    def string(self):
        count = self.compact()
        if abs(count) > 16000000:
            raise ValueError('Unreasonable string')
        raw = self.take(abs(count) * (2 if count < 0 else 1))
        if count and not raw.endswith(b'\0\0' if count < 0 else b'\0'):
            raise ValueError('Unterminated package string')
        return raw[:-2 if count < 0 else -1].decode('utf-16-le' if count < 0 else 'cp1252') if count else ''


class Package:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        r = Reader(self.data)
        magic, version, self.flags, nc, no, ec, eo, ic, io = r.unpack('<9I')
        if magic != 0x9e2a83c1 or (version & 0xffff) != 79:
            raise ValueError('This reader is verified only for local HP2 version 79 packages')
        if max(nc, ec, ic) > 1000000:
            raise ValueError('Unreasonable package table size')
        self.names = []
        r = Reader(self.data, no)
        for _ in range(nc):
            self.names.append(r.string())
            r.take(4)
        self.imports = []
        r = Reader(self.data, io)
        for _ in range(ic):
            package, klass = self.name(r.compact()), self.name(r.compact())
            outer = r.unpack('<i')[0]
            self.imports.append(dict(package=package, klass=klass, outer=outer, name=self.name(r.compact())))
        self.exports = []
        r = Reader(self.data, eo)
        for _ in range(ec):
            klass, parent = r.compact(), r.compact()
            outer = r.unpack('<i')[0]
            name = self.name(r.compact())
            flags = r.unpack('<I')[0]
            size = r.compact()
            offset = r.compact() if size else 0
            if size < 0 or offset < 0 or offset + size > len(self.data):
                raise ValueError('Export outside package')
            self.exports.append(dict(klass=klass, parent=parent, outer=outer, name=name, flags=flags, size=size, offset=offset))
        for row in self.exports:
            row['class_name'] = self.object_name(row['klass'])

    def name(self, index):
        if not 0 <= index < len(self.names):
            raise ValueError('Invalid name reference')
        return self.names[index]

    def object_name(self, index):
        if index == 0:
            return 'None'
        values, pos = (self.exports, index-1) if index > 0 else (self.imports, -index-1)
        if not 0 <= pos < len(values):
            raise ValueError('Invalid object reference')
        return values[pos]['name']

    def scripts(self):
        for row in self.exports:
            if row['class_name'] != 'TextBuffer':
                continue
            r = Reader(self.data[row['offset']:row['offset']+row['size']])
            if self.name(r.compact()) != 'None':
                raise ValueError('Unexpected text-buffer property prefix')
            top, pos = r.unpack('<ii')
            text = r.string()
            if r.pos != len(r.data):
                raise ValueError('Unexpected text-buffer suffix')
            yield self.object_name(row['outer']), text
