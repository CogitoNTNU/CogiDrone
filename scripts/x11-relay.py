#!/usr/bin/env python3
"""Relay the WSLg X server into a folder Docker Desktop can bind-mount.

Docker Desktop can't mount WSLg's /mnt/wslg into containers, but it can mount
normal WSL paths. This listens on ~/.cogidrone/X11-unix/X0 and forwards every
connection to WSLg's real socket, so Gazebo in the container can open windows.
Started automatically by scripts/sim.sh on WSL; not needed on Linux.
"""

import os
import selectors
import socket
import sys
import threading

TARGET = "/mnt/wslg/.X11-unix/X0"
LISTEN = os.path.expanduser("~/.cogidrone/X11-unix/X0")


def pipe(a: socket.socket, b: socket.socket) -> None:
    sel = selectors.DefaultSelector()
    sel.register(a, selectors.EVENT_READ, b)
    sel.register(b, selectors.EVENT_READ, a)
    try:
        while True:
            for key, _ in sel.select():
                data = key.fileobj.recv(65536)
                if not data:
                    return
                key.data.sendall(data)
    except OSError:
        pass
    finally:
        a.close()
        b.close()


def main() -> None:
    os.makedirs(os.path.dirname(LISTEN), exist_ok=True)
    if os.path.exists(LISTEN):
        os.unlink(LISTEN)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(LISTEN)
    os.chmod(LISTEN, 0o777)
    server.listen(64)
    print(f"x11-relay: {LISTEN} -> {TARGET}", file=sys.stderr)
    while True:
        client, _ = server.accept()
        upstream = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            upstream.connect(TARGET)
        except OSError as e:
            print(f"x11-relay: cannot reach WSLg: {e}", file=sys.stderr)
            client.close()
            continue
        threading.Thread(target=pipe, args=(client, upstream), daemon=True).start()


if __name__ == "__main__":
    main()
