"""Public binding for the pillar firmware frames.

The DLL is the client. This module does not invent samples and it does not
open a serial port unless open_port is called.
"""

import ctypes
import os
import sys

MT_OK = 0
MT_ERR_BAD_ARG = 1
MT_ERR_PORT_IN_USE = 4
MT_ERR_TIMEOUT = 5

MT_MSG_IDENT = 0x01
MT_MSG_DATA = 0x02
MT_MSG_STATUS = 0x03
MT_MOD_HALL_ULTRA = 0x17
MT_KIND_HALL_PATTERN = 1
MT_KIND_HALL = 2
MT_KIND_ULTRASONIC = 3


def dll_path():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", "metatron.dll")


class Identity(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("firmware_id", ctypes.c_char * 16),
        ("module_type", ctypes.c_uint8),
        ("firmware_version", ctypes.c_char * 12),
        ("channel_count", ctypes.c_uint8),
        ("health", ctypes.c_char * 8),
        ("uid", ctypes.c_char * 20),
        ("has_uid", ctypes.c_uint8),
    ]


class Sample(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("firmware_id", ctypes.c_char * 16),
        ("module_type", ctypes.c_uint8),
        ("channel", ctypes.c_uint8),
        ("body_len", ctypes.c_uint16),
        ("body", ctypes.c_uint8 * 120),
        ("firmware_millis", ctypes.c_uint32),
        ("kind", ctypes.c_int32),
        ("sensor_index", ctypes.c_int32),
        ("pattern", ctypes.c_uint8),
        ("active_count", ctypes.c_uint8),
        ("hall_bit", ctypes.c_uint8 * 4),
        ("magnet_detected", ctypes.c_uint8),
        ("hall_state", ctypes.c_uint8),
        ("trigger_count", ctypes.c_uint16),
        ("raw_analog", ctypes.c_uint16),
        ("baseline", ctypes.c_uint16),
        ("threshold", ctypes.c_uint16),
        ("quality", ctypes.c_uint8),
        ("distance_cm", ctypes.c_float),
        ("ultra_valid", ctypes.c_uint8),
        ("stddev_x100", ctypes.c_uint16),
        ("fields_valid", ctypes.c_uint8),
        ("reason", ctypes.c_char * 32),
    ]


class Frame(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("host_unix_ms", ctypes.c_uint64),
        ("crc_window", ctypes.c_int32),
        ("payload_len", ctypes.c_uint16),
        ("raw_len", ctypes.c_uint16),
        ("type", ctypes.c_uint8),
        ("payload", ctypes.c_uint8 * 128),
        ("raw", ctypes.c_uint8 * 133),
    ]


def load():
    path = dll_path()
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    lib = ctypes.CDLL(path)
    lib.mt_sizeof_identity.restype = ctypes.c_size_t
    lib.mt_sizeof_sample.restype = ctypes.c_size_t
    lib.mt_sizeof_frame.restype = ctypes.c_size_t
    if lib.mt_sizeof_identity() != ctypes.sizeof(Identity):
        raise RuntimeError("identity layout does not match the DLL")
    if lib.mt_sizeof_sample() != ctypes.sizeof(Sample):
        raise RuntimeError("sample layout does not match the DLL")
    if lib.mt_sizeof_frame() != ctypes.sizeof(Frame):
        raise RuntimeError("frame layout does not match the DLL")
    lib.mt_parser_new.restype = ctypes.c_void_p
    lib.mt_port_open.restype = ctypes.c_int
    lib.mt_record_create.restype = ctypes.c_int
    return lib


def ident_request(lib):
    out = (ctypes.c_uint8 * 8)()
    n = ctypes.c_size_t()
    rc = lib.mt_ident_request(out, 8, ctypes.byref(n))
    if rc != MT_OK:
        raise RuntimeError("ident request build failed: %s" % rc)
    return bytes(out[: n.value])


def parse_ident(lib, payload):
    identity = Identity()
    buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)
    rc = lib.mt_parse_ident(buf, len(payload), ctypes.byref(identity))
    if rc != MT_OK:
        raise RuntimeError("ident parse failed: %s" % rc)
    return identity


def open_port(lib, name):
    slot = ctypes.c_void_p()
    rc = lib.mt_port_open(name.encode("ascii"), ctypes.byref(slot))
    if rc == MT_ERR_PORT_IN_USE:
        raise RuntimeError("PORT_IN_USE %s" % name)
    if rc != MT_OK:
        raise RuntimeError("open failed %s rc=%s" % (name, rc))
    return slot


def self_test():
    lib = load()
    request = ident_request(lib)
    if request[0] != 0xAA or request[2] != MT_MSG_IDENT or request[-1] != 0x55:
        raise RuntimeError("ident request is not an empty MSG_IDENT frame")
    if lib.mt_build_frame(0x04, None, 0, None, 0, None) != MT_ERR_BAD_ARG:
        raise RuntimeError("calibrate frame was accepted")
    if lib.mt_build_frame(0x09, None, 0, None, 0, None) != MT_ERR_BAD_ARG:
        raise RuntimeError("command frame was accepted")
    text = b'{"id":"PILLAR_01","type":23,"fw":"1.0.0","channels":6,"health":"OK"}'
    identity = parse_ident(lib, text)
    if identity.firmware_id.decode() != "PILLAR_01" or identity.module_type != MT_MOD_HALL_ULTRA:
        raise RuntimeError("ident json did not match firmware fields")
    if lib.mt_hall_bit(0x05, 0) != 1 or lib.mt_hall_bit(0x05, 2) != 1:
        raise RuntimeError("hall bitmap does not match hallUpdatePattern")
    if lib.mt_hall_bit(0x05, 1) != 0 or lib.mt_hall_bit(0x05, 3) != 0:
        raise RuntimeError("hall bitmap set a clear bit")
    path = (ctypes.c_char * 32)()
    rc = lib.mt_port_path(b"COM12", path, 32)
    if rc != MT_OK or path.value != b"\\\\.\\COM12":
        raise RuntimeError("port path mismatch")
    print("python binding checks passed")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        sys.exit(self_test())
    print("usage: python metatron.py --self-test")
    sys.exit(2)
