#!/usr/bin/env python3
"""Static file server with HTTP Range support, for phone tests that play video from the PC.

python3 -m http.server sends a whole file for every request; Safari then buffers an mp4 as one stream and its
network process re-grows one buffer per chunk (~140 % of a core during a 1080p clip, videoprof 2026-11-03).
Real sites answer Range requests; this one does too (single ranges, as AVFoundation asks).

    tools/sunfish-httpd.py [--port 8077] [--dir work/stage0]
Requests are logged like http.server's (beacons included), to stderr.
"""
import argparse
import functools
import http.server
import os
import re


class RangeHandler(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        rng = self.headers.get("Range")
        path = self.translate_path(self.path)
        if not rng or os.path.isdir(path) or not os.path.isfile(path):
            return super().send_head()
        m = re.match(r"bytes=(\d*)-(\d*)$", rng.strip())
        size = os.path.getsize(path)
        if not m or (m.group(1) == "" and m.group(2) == ""):
            return super().send_head()
        if m.group(1) == "":  # suffix range: the last N bytes
            start, end = max(0, size - int(m.group(2))), size - 1
        else:
            start = int(m.group(1))
            end = int(m.group(2)) if m.group(2) else size - 1
        if start >= size:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.end_headers()
            return None
        end = min(end, size - 1)
        f = open(path, "rb")
        f.seek(start)
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(end - start + 1))
        self.end_headers()
        self._left = end - start + 1
        return f

    def end_headers(self):
        self.send_header("Accept-Ranges", "bytes")
        super().end_headers()

    def copyfile(self, source, outputfile):
        left = getattr(self, "_left", None)
        if left is None:
            return super().copyfile(source, outputfile)
        while left > 0:
            buf = source.read(min(left, 1 << 16))
            if not buf:
                break
            outputfile.write(buf)
            left -= len(buf)
        self._left = None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8077)
    ap.add_argument("--dir", default="work/stage0")
    a = ap.parse_args()
    handler = functools.partial(RangeHandler, directory=a.dir)
    http.server.ThreadingHTTPServer(("0.0.0.0", a.port), handler).serve_forever()


if __name__ == "__main__":
    main()
