"""Read one pillar port using the firmware ident request.

Usage: python examples/read_pillar.py COM3 [record.bin]

The COM name is where Windows enumerated the port. The pillar name is the
firmware id inside the IDENT frame. This script is not run by the build.
"""

import ctypes
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
import metatron


def main():
    if len(sys.argv) < 2:
        print("usage: python examples/read_pillar.py COM3 [record.bin]")
        return 2
    lib = metatron.load()
    port = metatron.open_port(lib, sys.argv[1])
    record = ctypes.c_void_p()
    if len(sys.argv) >= 3:
        rc = lib.mt_record_create(sys.argv[2].encode("utf-8"), ctypes.byref(record))
        if rc != metatron.MT_OK:
            raise RuntimeError("record create failed: %s" % rc)
    request = metatron.ident_request(lib)
    buf = (ctypes.c_uint8 * len(request)).from_buffer_copy(request)
    rc = lib.mt_port_write(port, buf, len(request))
    if rc != metatron.MT_OK:
        raise RuntimeError("ident write failed: %s" % rc)
    parser = lib.mt_parser_new()
    frame = metatron.Frame()
    chunk = (ctypes.c_uint8 * 256)()
    deadline = time.time() + 5.0
    frames = 0
    identity = None
    try:
        while time.time() < deadline:
            nread = ctypes.c_size_t()
            rc = lib.mt_port_read(port, chunk, 256, ctypes.byref(nread), 200)
            if rc == metatron.MT_ERR_TIMEOUT:
                continue
            if rc != metatron.MT_OK:
                raise RuntimeError("read failed: %s" % rc)
            host_ms = lib.mt_now_unix_ms()
            got = ctypes.c_int()
            rc = lib.mt_parser_push(parser, chunk, nread.value, host_ms, ctypes.byref(frame), ctypes.byref(got))
            if rc != metatron.MT_OK:
                raise RuntimeError("parse failed: %s" % rc)
            while got.value:
                frames += 1
                if record.value:
                    lib.mt_record_write(record, ctypes.byref(frame))
                if frame.type == metatron.MT_MSG_IDENT:
                    identity = metatron.parse_ident(lib, bytes(frame.payload[: frame.payload_len]))
                    print("IDENT %s type=0x%02X fw=%s channels=%s host_ms=%s" % (
                        identity.firmware_id.decode(),
                        identity.module_type,
                        identity.firmware_version.decode(),
                        identity.channel_count,
                        frame.host_unix_ms,
                    ))
                elif frame.type == metatron.MT_MSG_DATA:
                    sample = metatron.Sample()
                    if identity is None:
                        firmware_id = b""
                        module_type = 0
                        has_ident = 0
                    else:
                        firmware_id = identity.firmware_id
                        module_type = identity.module_type
                        has_ident = 1
                    rc = lib.mt_parse_data(
                        firmware_id,
                        module_type,
                        has_ident,
                        frame.payload,
                        frame.payload_len,
                        ctypes.byref(sample),
                    )
                    if rc != metatron.MT_OK:
                        raise RuntimeError("data parse failed: %s" % rc)
                    print("DATA channel=%s kind=%s firmware_millis=%s host_ms=%s" % (
                        sample.channel,
                        sample.kind,
                        sample.firmware_millis,
                        frame.host_unix_ms,
                    ))
                    if sample.kind == metatron.MT_KIND_HALL_PATTERN and sample.fields_valid:
                        print("HALL bits %s %s %s %s" % (
                            sample.hall_bit[0], sample.hall_bit[1],
                            sample.hall_bit[2], sample.hall_bit[3],
                        ))
                else:
                    print("FRAME type=0x%02X bytes=%s host_ms=%s" % (
                        frame.type, frame.payload_len, frame.host_unix_ms))
                got = ctypes.c_int()
                rc = lib.mt_parser_push(parser, None, 0, host_ms, ctypes.byref(frame), ctypes.byref(got))
                if rc != metatron.MT_OK:
                    raise RuntimeError("parse failed: %s" % rc)
    finally:
        lib.mt_parser_free(parser)
        if record.value:
            lib.mt_record_close(record)
        lib.mt_port_close(port)
    print("frames %s" % frames)
    return 0


if __name__ == "__main__":
    sys.exit(main())
