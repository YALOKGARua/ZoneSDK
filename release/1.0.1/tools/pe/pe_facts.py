import json
import struct
import sys


class Image:
    def __init__(self, path):
        with open(path, "rb") as handle:
            self.data = handle.read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        self.machine, self.count = struct.unpack_from("<HH", self.data, pe + 4)
        self.stamp = struct.unpack_from("<I", self.data, pe + 8)[0]
        optional = pe + 24
        self.magic = struct.unpack_from("<H", self.data, optional)[0]
        self.base = struct.unpack_from("<Q", self.data, optional + 24)[0]
        self.size = struct.unpack_from("<I", self.data, optional + 56)[0]
        directories = optional + 112
        self.dirs = [struct.unpack_from("<II", self.data, directories + 8 * i) for i in range(16)]
        header = struct.unpack_from("<H", self.data, pe + 20)[0]
        table = optional + header
        self.sections = []
        for i in range(self.count):
            at = table + 40 * i
            name = self.data[at:at + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.data, at + 8)
            self.sections.append((name, va, max(vsize, rsize), raw, rsize))

    def offset(self, rva):
        for name, va, size, raw, rsize in self.sections:
            if va <= rva < va + size:
                return raw + (rva - va)
        raise ValueError("rva outside sections: 0x%X" % rva)

    def cstr(self, rva):
        at = self.offset(rva)
        end = self.data.index(b"\0", at)
        return self.data[at:end].decode("utf-8", "replace")

    def exports(self):
        rva, size = self.dirs[0]
        if not rva:
            return {}
        at = self.offset(rva)
        names, funcs, ords = struct.unpack_from("<III", self.data, at + 24)[0], struct.unpack_from("<I", self.data, at + 28)[0], struct.unpack_from("<I", self.data, at + 36)[0]
        count = struct.unpack_from("<I", self.data, at + 24)[0]
        func_table = struct.unpack_from("<I", self.data, at + 28)[0]
        name_table = struct.unpack_from("<I", self.data, at + 32)[0]
        ord_table = struct.unpack_from("<I", self.data, at + 36)[0]
        out = {}
        for i in range(count):
            name = self.cstr(struct.unpack_from("<I", self.data, self.offset(name_table) + 4 * i)[0])
            ordinal = struct.unpack_from("<H", self.data, self.offset(ord_table) + 2 * i)[0]
            target = struct.unpack_from("<I", self.data, self.offset(func_table) + 4 * ordinal)[0]
            value = None
            try:
                pointer = struct.unpack_from("<Q", self.data, self.offset(target))[0]
                if self.base <= pointer < self.base + self.size:
                    value = self.cstr(pointer - self.base)
            except (ValueError, struct.error):
                value = None
            out[name] = value
        return out

    def imports(self):
        rva, size = self.dirs[1]
        out = []
        if not rva:
            return out
        at = self.offset(rva)
        while True:
            name_rva = struct.unpack_from("<I", self.data, at + 12)[0]
            if not name_rva:
                break
            out.append(self.cstr(name_rva).lower())
            at += 20
        return sorted(set(out))

    def resource(self, kind):
        rva, size = self.dirs[2]
        if not rva:
            return None
        root = self.offset(rva)

        def entries(directory):
            named, ids = struct.unpack_from("<HH", self.data, directory + 12)
            for i in range(named + ids):
                name, target = struct.unpack_from("<II", self.data, directory + 16 + 8 * i)
                yield name, target

        for name, target in entries(root):
            if name != kind:
                continue
            level2 = root + (target & 0x7FFFFFFF)
            for _, t2 in entries(level2):
                level3 = root + (t2 & 0x7FFFFFFF)
                for _, t3 in entries(level3):
                    data_rva, data_size = struct.unpack_from("<II", self.data, root + t3)
                    at = self.offset(data_rva)
                    return self.data[at:at + data_size]
        return None


def wide(blob, at):
    end = at
    while blob[end:end + 2] != b"\0\0":
        end += 2
    return blob[at:end].decode("utf-16-le"), end + 2


def align(at):
    return (at + 3) & ~3


def version(blob):
    if not blob:
        return {}
    facts = {"strings": {}}

    def walk(at, depth):
        length, value_length, kind = struct.unpack_from("<HHH", blob, at)
        key, after = wide(blob, at + 6)
        value_at = align(after)
        end = at + length
        if depth == 0 and value_length >= 52:
            sig, ver, ms, ls, pms, pls, mask, flags = struct.unpack_from("<IIIIIIII", blob, value_at)
            facts["file_version"] = "%d.%d.%d.%d" % (ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF)
            facts["product_version"] = "%d.%d.%d.%d" % (pms >> 16, pms & 0xFFFF, pls >> 16, pls & 0xFFFF)
            facts["file_flags"] = "0x%X" % flags
            child = align(value_at + value_length)
        elif depth == 3:
            text = blob[value_at:value_at + value_length * 2].decode("utf-16-le").rstrip("\0") if kind == 1 else ""
            facts["strings"][key] = text
            return
        else:
            child = align(value_at + (value_length * 2 if kind == 1 else value_length))
        while child < end:
            sub_length = struct.unpack_from("<H", blob, child)[0]
            if not sub_length:
                break
            if key in ("VS_VERSION_INFO", "StringFileInfo") or depth == 2:
                walk(child, depth + 1)
            child = align(child + sub_length)

    walk(0, 0)
    return facts


def facts(path):
    image = Image(path)
    return {
        "path": path,
        "machine": "0x%X" % image.machine,
        "exports": image.exports(),
        "imports": image.imports(),
        "version": version(image.resource(16)),
        "sections": [s[0] for s in image.sections],
    }


def main():
    out = {}
    for label, path in zip(sys.argv[1::2], sys.argv[2::2]):
        out[label] = facts(path)
    labels = list(out)
    if len(labels) == 2:
        a, b = out[labels[0]], out[labels[1]]
        va = dict(a["version"].get("strings", {}))
        vb = dict(b["version"].get("strings", {}))
        extra = sorted(set(vb) - set(va))
        same_strings = all(va.get(k) == vb.get(k) for k in set(va) | set(vb) if k != "SpecialBuild")
        out["compare"] = {
            "exports_same": a["exports"] == b["exports"],
            "exports_exactly_name_description": sorted(a["exports"]) == ["DESCRIPTION", "NAME"] and sorted(b["exports"]) == ["DESCRIPTION", "NAME"],
            "imports_same": a["imports"] == b["imports"],
            "imports_only_in_second": sorted(set(b["imports"]) - set(a["imports"])),
            "imports_only_in_first": sorted(set(a["imports"]) - set(b["imports"])),
            "version_strings_same_except_special_build": same_strings,
            "version_keys_only_in_second": extra,
            "file_version_same": a["version"].get("file_version") == b["version"].get("file_version"),
            "file_flags": [a["version"].get("file_flags"), b["version"].get("file_flags")],
        }
    print(json.dumps(out, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
