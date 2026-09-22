"""Read-only PE/COFF analysis using Python's standard library; never loads a DLL."""
import hashlib
import json
from pathlib import Path
import struct
import sys
from project_paths import ROOT, inside, write_text


class PE:
    def __init__(self, data):
        self.data = data
        if self.take(0, 2) != b"MZ":
            raise ValueError("DOS signature missing")
        pe = self.u32(0x3c)
        if self.take(pe, 4) != b"PE\0\0":
            raise ValueError("PE signature missing")
        self.machine, count, self.timestamp = self.unpack("<HHI", pe + 4)
        opt_size = self.u16(pe + 20)
        opt = pe + 24
        magic = self.u16(opt)
        if magic not in (0x10b, 0x20b):
            raise ValueError("Unsupported optional header")
        self.bits = 32 if magic == 0x10b else 64
        self.image_base = self.u32(opt + 28) if self.bits == 32 else self.unpack("<Q", opt + 24)[0]
        self.entry_rva = self.u32(opt + 16)
        self.header_size = self.u32(opt + 60)
        dd = opt + (96 if self.bits == 32 else 112)
        nd = self.u32(dd - 4)
        if dd + min(nd, 16) * 8 > opt + opt_size:
            raise ValueError("Data directory outside optional header")
        self.directories = [self.unpack("<II", dd + n * 8) for n in range(min(nd, 16))]
        self.sections = []
        for n in range(count):
            off = opt + opt_size + n * 40
            name = self.take(off, 8).split(b"\0")[0].decode("ascii", "replace")
            vs, va, rs, rp = self.unpack("<IIII", off + 8)
            self.take(rp, rs)
            self.sections.append(dict(name=name, virtual_size=vs, rva=va, raw_size=rs, raw_offset=rp))

    def take(self, off, size):
        if off < 0 or size < 0 or off + size > len(self.data):
            raise ValueError("PE range outside file")
        return self.data[off:off + size]

    def unpack(self, fmt, off):
        return struct.unpack(fmt, self.take(off, struct.calcsize(fmt)))

    def u16(self, off):
        return self.unpack("<H", off)[0]

    def u32(self, off):
        return self.unpack("<I", off)[0]

    def offset(self, rva):
        if rva < self.header_size:
            self.take(rva, 1)
            return rva
        for section in self.sections:
            delta = rva - section["rva"]
            if 0 <= delta < section["raw_size"]:
                return section["raw_offset"] + delta
        raise ValueError(f"Unmapped RVA {rva:#x}")

    def string(self, rva):
        off = self.offset(rva)
        end = self.data.find(b"\0", off, min(len(self.data), off + 8192))
        if end < 0:
            raise ValueError("Unterminated PE string")
        return self.data[off:end].decode("ascii", "replace")

    def directory(self, index):
        return self.directories[index] if index < len(self.directories) else (0, 0)

    def exports(self):
        rva, size = self.directory(0)
        if not rva:
            return []
        off = self.offset(rva)
        base, functions, names, addresses, pointers, ordinals = self.unpack("<IIIIII", off + 16)
        if max(functions, names) > 1000000:
            raise ValueError("Unreasonable export count")
        result = []
        for i in range(names):
            name_rva = self.u32(self.offset(pointers) + i * 4)
            ordinal = self.u16(self.offset(ordinals) + i * 2)
            if ordinal >= functions:
                raise ValueError("Export ordinal out of range")
            address = self.u32(self.offset(addresses) + ordinal * 4)
            result.append(dict(name=self.string(name_rva), ordinal=base + ordinal,
                               rva=f"0x{address:08x}",
                               forwarder=self.string(address) if rva <= address < rva + size else None))
        return result

    def imports(self):
        rva, size = self.directory(1)
        if not rva:
            return []
        result = []
        off = self.offset(rva)
        for i in range(min(size // 20, 4096)):
            original, stamp, forward, name, first = self.unpack("<IIIII", off + i * 20)
            if not any((original, stamp, forward, name, first)):
                return result
            thunk = self.offset(original or first)
            symbols = []
            for n in range(100000):
                value = self.unpack("<I" if self.bits == 32 else "<Q", thunk + n * (self.bits // 8))[0]
                if value == 0:
                    break
                if value & (1 << (self.bits - 1)):
                    symbols.append({"ordinal": value & 0xffff})
                else:
                    symbols.append({"name": self.string(value + 2)})
            else:
                raise ValueError("Unterminated import table")
            result.append(dict(dll=self.string(name), symbols=symbols))
        raise ValueError("Unterminated import descriptors")


def inspect(path):
    path = inside(path)
    data = path.read_bytes()
    pe = PE(data)
    return dict(path=str(path.relative_to(ROOT)), bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                machine=f"0x{pe.machine:04x}", bits=pe.bits, coff_timestamp=pe.timestamp,
                entry_rva=f"0x{pe.entry_rva:08x}", sections=pe.sections,
                exports=pe.exports(), imports=pe.imports())


def main():
    config = json.loads(inside("config/project.json").read_text(encoding="utf-8-sig"))
    system = inside(Path(config["game_executable"]).parent)
    results = []
    for path in sorted(system.iterdir()):
        if path.suffix.lower() in (".dll", ".exe"):
            results.append(inspect(path))
    output = sys.argv[1] if len(sys.argv) > 1 else "logs/pe-inventory.json"
    write_text(output, json.dumps(results, indent=2) + "\n")
    for item in results:
        print(f'{item["path"]}: {item["bits"]}-bit, {len(item["exports"])} named exports, SHA256 {item["sha256"]}')


if __name__ == "__main__":
    main()
