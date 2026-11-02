#!/usr/bin/env python3
"""Convert a Full Tilt! Pinball .MDS (RIFF MIDS) music file to a standard .MID.

Python port of midi::MdsToMidi from the SpaceCadetPinball decompilation.
Usage: mds2mid.py TABA1.MDS TABA1.mid
"""
import struct
import sys


def var_len(value):
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(out))


def mds_to_midi(data):
    riff, file_size, mids, fmt, fmt_size = struct.unpack_from("<4sI4s4sI", data, 0)
    if riff != b"RIFF" or mids != b"MIDS" or fmt != b"fmt ":
        raise ValueError("not a RIFF MIDS file")
    time_format, _max_buffer, flags = struct.unpack_from("<III", data, 20)
    stream_id_used = flags == 0

    pos = 20 + fmt_size
    tag, _data_size, block_count = struct.unpack_from("<4sII", data, pos)
    if tag != b"data":
        raise ValueError("missing data chunk")
    pos += 12

    words_per_event = 3 if stream_id_used else 2
    events = []
    for _ in range(block_count):
        tick, buf_size = struct.unpack_from("<II", data, pos)
        pos += 8
        for i in range(buf_size // (4 * words_per_event)):
            words = struct.unpack_from("<%dI" % words_per_event, data, pos + i * 4 * words_per_event)
            tick += words[0]
            events.append((tick, words[-1]))
        pos += buf_size

    events.sort(key=lambda e: e[0])  # stable, like the original

    track = bytearray()
    prev = 0
    for tick, ev in events:
        track += var_len(tick - prev)
        prev = tick
        kind = ev >> 24
        if kind == 0:  # short MIDI message, little-endian packed
            raw = struct.pack("<I", ev)
            short = (ev & 0xF0) in (0xC0, 0xD0)
            track += raw[:2] if short else raw[:3]
        elif kind == 1:  # tempo change
            track += b"\xFF\x51\x03" + struct.pack(">I", ev)[1:]
        else:
            raise ValueError("unknown MIDS event type %d" % kind)
    track += b"\x00\xFF\x2F\x00"

    header = b"MThd" + struct.pack(">IhHH", 6, 0, 1, time_format & 0xFFFF)
    return header + b"MTrk" + struct.pack(">I", len(track)) + bytes(track)


if __name__ == "__main__":
    with open(sys.argv[1], "rb") as f:
        midi = mds_to_midi(f.read())
    with open(sys.argv[2], "wb") as f:
        f.write(midi)
