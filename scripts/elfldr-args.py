#!/usr/bin/env python3
"""Prints ARGS encoded for an elfldr v0.7 launch line (file:PATH?args=...).

elfldr v0.7 splits args on spaces and drops a backslash before any character
(socksrv.c args_split). The URI ends at a newline and the args value at & or
#, so only %, &, # and control characters are percent-encoded. Its decoder
skips a %XX in the last three characters (uri.c uri_decode), so a trailing
backslash, which args_decode removes, keeps one off the end. The whole line
must fit in 1024 bytes (PATH_MAX). app/src/conf.c elfldr_line does the same.
"""
import sys

parts = [a.replace("\\", "\\\\").replace(" ", "\\ ") for a in sys.argv[1:]]
out = "".join(c if c not in "%&#" and ord(c) >= 32 else "".join("%%%02X" % b for b in c.encode()) for c in " ".join(parts))
print(out + "\\" if out[-3:-2] == "%" else out)
