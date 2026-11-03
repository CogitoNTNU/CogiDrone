#!/usr/bin/env python3
"""3D viewer for a flight run-log (format: README.md in this folder).

    python3 tools/flight_viewer/view.py                  # ~/flight_runs/latest
    python3 tools/flight_viewer/view.py RUN_DIR [--port 8765] [--no-browser] [--rebuild-terrain]
        [--background auto|sim|kartverket|global|osm|flat] [--anchor LAT,LON[,ALT]]

Builds the terrain background if the run has none (terrain.py, cached), serves the viewer and
the run on http://127.0.0.1:PORT/ and opens the browser. Ctrl-C stops the server.
GET /api/terrain?source=global builds another background into RUN/terrain_<source>/ (the
viewer's Background menu).
"""

import argparse
import http.server
import json
import os
import shutil
import socketserver
import subprocess
import sys
import threading
import webbrowser

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import terrain


class Handler(http.server.SimpleHTTPRequestHandler):
    """/ -> the viewer (this folder), /run/... -> the run directory."""

    run_dir = ""
    opts = {}
    build_lock = threading.Lock()

    def do_GET(self):
        if self.path.startswith("/api/terrain"):
            return self.api_terrain()
        return super().do_GET()

    def api_terrain(self):
        from urllib.parse import parse_qs, urlparse
        q = parse_qs(urlparse(self.path).query)
        src = q.get("source", ["auto"])[0]
        res = {"ok": False}
        if src not in terrain.SOURCES:
            res["error"] = f"unknown source {src}"
        else:
            name = "terrain" if src == self.opts.get("background", "auto") else f"terrain_{src}"
            try:
                with self.build_lock:
                    _, tj = terrain.build(self.run_dir, self.opts.get("margin", 150.0), q.get("force", ["0"])[0] == "1",
                                          source=src, anchor=self.opts.get("anchor"), out_name=name, quiet=True)
                res = {"ok": True, "dir": name + "/", "kind": tj["kind"], "errors": tj.get("errors", [])}
            except Exception as ex:
                res["error"] = str(ex)[:300]
        body = json.dumps(res).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def translate_path(self, path):
        p = path.split("?", 1)[0].split("#", 1)[0]
        if p.startswith("/run/"):
            base, rel = self.run_dir, p[len("/run/"):]
        else:
            base, rel = HERE, p.lstrip("/") or "index.html"
        full = os.path.realpath(os.path.join(base, *[s for s in rel.split("/") if s not in ("", ".", "..")]))
        return full if full.startswith(os.path.realpath(base)) else os.path.join(base, "__nope__")

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")  # the run in 'latest' changes under the same URL
        super().end_headers()

    def log_message(self, fmt, *args):
        if args and str(args[1]) not in ("200", "304"):
            sys.stderr.write("%s\n" % (fmt % args))


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def open_browser(url):
    for exe in ("chromium", "chromium-browser", "google-chrome", "firefox"):
        if shutil.which(exe):
            subprocess.Popen([exe, url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            return exe
    webbrowser.open(url)
    return "default browser"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", nargs="?", default=os.path.expanduser("~/flight_runs/latest"))
    ap.add_argument("--port", type=int, default=8765, help="0 = any free port")
    ap.add_argument("--host", default="127.0.0.1", help="0.0.0.0 to view from another machine")
    ap.add_argument("--no-browser", action="store_true")
    ap.add_argument("--rebuild-terrain", action="store_true")
    ap.add_argument("--margin", type=float, default=150.0, help="terrain around the flown area, m")
    ap.add_argument("--background", default="auto", choices=terrain.SOURCES,
                    help="map source: auto (sim -> Kartverket in Norway -> global -> flat), sim, kartverket, "
                         "global (AWS heights + Esri imagery), osm, flat (offline grid)")
    ap.add_argument("--anchor", default="", help="LAT,LON[,ALT] of local (0, 0): for logs without lat/lon "
                    "(no GPS / VIO), or to move a run (overrides meta.origin; meta.anchor works too)")
    a = ap.parse_args()
    run = os.path.abspath(os.path.expanduser(a.run))
    if not os.path.exists(os.path.join(run, "meta.json")):
        sys.exit(f"{run}: no meta.json, not a flight run-log (see {HERE}/README.md)")
    anchor = terrain.parse_anchor(a.anchor)
    try:
        terrain.build(run, a.margin, a.rebuild_terrain, source=a.background, anchor=anchor)
    except Exception as ex:  # the viewer still works without a background
        print(f"terrain failed: {ex}")
    Handler.run_dir = run
    Handler.opts = {"background": a.background, "anchor": anchor, "margin": a.margin}
    try:
        srv = Server((a.host, a.port), Handler)
    except OSError:
        srv = Server((a.host, 0), Handler)
    url = f"http://{'127.0.0.1' if a.host == '0.0.0.0' else a.host}:{srv.server_address[1]}/"
    print(f"flight viewer: {url}  (run {run})  Ctrl-C to stop")
    if not a.no_browser:
        threading.Timer(0.3, open_browser, (url,)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        srv.server_close()


if __name__ == "__main__":
    main()
