#!/usr/bin/env python3
"""Pull tuner data from the micromouse over WiFi (tuner build, TUNER_WIFI 1).

  python tools/mouse_log.py find     locate the mouse on this network
  python tools/mouse_log.py dump     save everything once:   logs/dump-<time>.txt
  python tools/mouse_log.py watch    record every test live:  logs/session-<time>/

The laptop must be on the same network as the mouse: the hotspot set in
TUNER_HOTSPOT_SSID (the laptop keeps its internet), or the mouse's own
MICROMOUSE-TUNER network. Add --host 192.168.x.y to skip the search.

watch writes, per test the tuner runs:
  run-NNN-<test>.txt   values changed since the previous test, the result rows,
                       and for maze runs the map and the full cell-by-cell log
  run-NNN-trace.csv    the whole trace (stitched: longer than the mouse's 5 s buffer)
  session.md           one line per test
Standard library only.
"""

import argparse
import datetime as dt
import json
import socket
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOGS = ROOT / "logs"
HOST_CACHE = LOGS / ".mouse_host"

# Tests that move the mouse and record a trace (tbExecute in mouse-firmware.ino).
MOTION_TASKS = {"dir", "enc", "straight", "turnL", "turnR", "uturn", "seq", "follow", "maze"}


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------
def get(host, path, timeout=3.0, text=False):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
        data = r.read().decode("utf-8", "replace")
    return data if text else json.loads(data)


def is_mouse(host, timeout=0.8):
    try:
        s = get(host, "/api/state", timeout)
        return isinstance(s, dict) and "tof" in s and "busy" in s
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Finding the mouse
# ---------------------------------------------------------------------------
def local_ipv4():
    ips = set()
    try:
        ips.update(socket.gethostbyname_ex(socket.gethostname())[2])
    except OSError:
        pass
    for probe in ("8.8.8.8", "192.168.4.1"):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect((probe, 80))
            ips.add(s.getsockname()[0])
            s.close()
        except OSError:
            pass
    return sorted(ip for ip in ips if not ip.startswith(("127.", "169.254.", "0.")))


def remember(host):
    LOGS.mkdir(parents=True, exist_ok=True)
    HOST_CACHE.write_text(host)


def find(explicit=None, quiet=False):
    candidates = [explicit] if explicit else []
    if HOST_CACHE.exists():
        candidates.append(HOST_CACHE.read_text().strip())
    candidates += ["micromouse.local", "192.168.4.1"]
    for h in candidates:
        if h and is_mouse(h):
            remember(h)
            return h
    if explicit:
        return None
    for ip in local_ipv4():
        base = ip.rsplit(".", 1)[0]
        if not quiet:
            print(f"searching {base}.1-254 ...", flush=True)
        hosts = [f"{base}.{i}" for i in range(1, 255) if f"{base}.{i}" != ip]
        with ThreadPoolExecutor(64) as ex:
            futures = {ex.submit(is_mouse, h): h for h in hosts}
            for f in as_completed(futures):
                if f.result():
                    h = futures[f]
                    remember(h)
                    return h
    return None


def need_host(args):
    host = find(args.host)
    if not host:
        sys.exit("Mouse not found. Is the tuner build running, and is this laptop on the same "
                 "WiFi (the hotspot, or MICROMOUSE-TUNER)? Try --host <ip>.")
    return host


# ---------------------------------------------------------------------------
# Formatting
# ---------------------------------------------------------------------------
def stamp():
    return dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def values(host):
    return {p["id"]: p["v"] for p in get(host, "/api/params", 5)["params"]}


def value_changes(before, after):
    return [f"{k} {before[k]:g} -> {after[k]:g}" for k in after if k in before and before[k] != after[k]]


def maze_ascii(m):
    """The map floodfill built, drawn like the tuner page draws it (mirrored
    when the maze opens to the LEFT, so it matches the real maze)."""
    n = m.get("n") or 16
    w = m["w"]
    mir = m.get("mir") == 1

    def edge(x, y, d):                       # d: 0 N, 1 E, 2 S, 3 W -> 'W' wall, 'O' open, '?' unknown
        if (d == 0 and y == n - 1) or (d == 1 and x == n - 1) or (d == 2 and y == 0) or (d == 3 and x == 0):
            return "W"
        b = int(w[(y * 16 + x) * 2:(y * 16 + x) * 2 + 2], 16)
        if b & (1 << d):
            return "W" if b & (16 << d) else "O"
        return "?"

    lx = (lambda c: n - 1 - c) if mir else (lambda c: c)
    east, west = (3, 1) if mir else (1, 3)
    h = m.get("h", 0)
    if mir and h in (1, 3):
        h = 4 - h
    arrow = "^>v<"[h]
    horiz = {"W": "---", "O": "   ", "?": " . "}
    vert = {"W": "|", "O": " ", "?": ":"}
    lines = []
    for y in range(n - 1, -1, -1):
        top, mid = "+", ""
        for c in range(n):
            x = lx(c)
            top += horiz[edge(x, y, 0)] + "+"
            mid += vert[edge(x, y, west)]
            mid += f" {arrow} " if (x == m.get("x") and y == m.get("y")) else "   "
        mid += vert[edge(lx(n - 1), y, east)]
        lines += [top, mid]
    lines.append("+" + "".join(horiz[edge(lx(c), 0, 2)] + "+" for c in range(n)))
    lines.append("--- wall, blank open, '.' / ':' never seen; arrow = mouse; start is the bottom "
                 + ("right" if mir else "left") + " cell")
    return "\n".join(lines)


def row_line(r):
    return "  " + "  ".join(f"{k}={v}" for k, v in r.items())


def result_summary(res):
    """One line for session.md from a result (tbResEnd JSON)."""
    if not res or res.get("type") in (None, "none"):
        return ""
    rows = res.get("rows") or []
    parts = []
    for side in ("L", "R"):
        degs = [r["deg"] for r in rows if r.get("c") == side and r.get("ok") and "deg" in r]
        if degs:
            peak = max(r.get("rate", 0) for r in rows if r.get("c") == side)
            ms = max(r.get("ms", 0) for r in rows if r.get("c") == side)
            parts.append(f"{side} avg {sum(degs) / len(degs):.1f} ({min(degs):.1f}..{max(degs):.1f}) "
                         f"peak {peak} deg/s, slowest {ms} ms")
    cells = [r for r in rows if r.get("c") == "F" and r.get("ok")]
    if cells:
        mm = sum((r["mmL"] + r["mmR"]) / 2 for r in cells) / len(cells)
        parts.append(f"{len(cells)} cells, avg {mm:.1f} mm, last heading {cells[-1]['yaw']:.1f} deg")
    failed = [r for r in rows if r.get("ok") == 0]
    if failed:
        parts.append("FAILED: " + "; ".join(str(r.get("why", "")) for r in failed))
    if res.get("type") == "capture":
        parts.append(" ".join(f"{r['s']}={r['cal']:.0f}" for r in rows if r.get("n")))
    if res.get("note"):
        parts.append(res["note"])
    return " | ".join(parts)


def trace_rows(csv_text):
    lines = csv_text.strip().splitlines()
    if not lines:
        return "", []
    header, out = lines[0], []
    for line in lines[1:]:
        try:
            out.append((int(line.split(",", 1)[0]), line))
        except ValueError:
            pass
    return header, out


# ---------------------------------------------------------------------------
# dump
# ---------------------------------------------------------------------------
def cmd_dump(args):
    host = need_host(args)
    state = get(host, "/api/state")
    export = get(host, "/api/export", text=True)
    result = get(host, "/api/result")["r"]
    maze = get(host, "/api/maze")
    trace = get(host, "/api/trace", 10, text=True)
    text = "\n".join([
        f"# micromouse dump {dt.datetime.now():%Y-%m-%d %H:%M:%S} from {host}",
        "", "[state]", json.dumps(state),
        "", "[values]", export.strip(),
        "", "[result]", result_summary(result), *[row_line(r) for r in result.get("rows", [])],
        json.dumps(result),
        "", "[maze]", maze_ascii(maze),
        "", "[maze log]", *maze.get("log", []),
        "", "[trace]", trace.strip(), "",
    ])
    LOGS.mkdir(parents=True, exist_ok=True)
    path = LOGS / f"dump-{stamp()}.txt"
    path.write_text(text, encoding="utf-8")
    print(path)


# ---------------------------------------------------------------------------
# watch
# ---------------------------------------------------------------------------
class Run:
    def __init__(self, number, task):
        self.number = number
        self.task = task
        self.started = dt.datetime.now()
        self.changes = []
        self.trace_header = ""
        self.trace = []
        self.last_t = -1
        self.log = []
        self.log_count = 0
        self.last_trace_poll = 0.0
        self.last_maze_poll = 0.0

    def add_trace(self, host):
        try:
            header, rows = trace_rows(get(host, "/api/trace", 10, text=True))
        except Exception:
            return
        if header:
            self.trace_header = header
        for t, line in rows:
            if t > self.last_t:
                self.trace.append(line)
                self.last_t = t

    def add_maze_log(self, host, count):
        try:
            m = get(host, "/api/maze", 5)
        except Exception:
            return None
        new = count - self.log_count
        if new > 0:
            lines = m.get("log", [])
            if new > len(lines):
                self.log.append(f"... {new - len(lines)} log lines lost (poll too slow)")
                new = len(lines)
            self.log += lines[len(lines) - new:]
            self.log_count = count
        return m


def cmd_watch(args):
    host = need_host(args)
    session = LOGS / f"session-{stamp()}"
    session.mkdir(parents=True, exist_ok=True)
    (LOGS / "latest-session.txt").write_text(str(session))
    summary = open(session / "session.md", "a", encoding="utf-8")

    def note(line):
        print(line, flush=True)
        summary.write(line + "\n")
        summary.flush()

    prev_values = values(host)
    (session / "values-start.txt").write_text(get(host, "/api/export", text=True), encoding="utf-8")
    state = get(host, "/api/state")
    res_seq, was_busy, run, number = state["res"], bool(state["busy"]), None, 0
    note(f"# session {session.name} on {host} (Ctrl+C to stop)")

    while True:
        try:
            s = get(host, "/api/state", 2)
        except Exception:
            print("no reply from the mouse, retrying ...", flush=True)
            time.sleep(1.0)
            continue
        busy, now = bool(s["busy"]), time.time()

        if busy and not was_busy:
            number += 1
            run = Run(number, s.get("task", "?"))
            try:
                cur = values(host)
                run.changes = value_changes(prev_values, cur)
                prev_values = cur
            except Exception:
                pass

        if run and busy and run.task in MOTION_TASKS:
            if now - run.last_trace_poll > 2.0:
                run.add_trace(host)
                run.last_trace_poll = now
            if run.task == "maze" and s.get("log", 0) != run.log_count and now - run.last_maze_poll > 0.8:
                run.add_maze_log(host, s.get("log", 0))
                run.last_maze_poll = now

        if run and not busy:
            maze = None
            if run.task in MOTION_TASKS:
                run.add_trace(host)
            if run.task == "maze":
                maze = run.add_maze_log(host, s.get("log", 0))
            try:
                result = get(host, "/api/result")["r"]
            except Exception:
                result = {}
            secs = (dt.datetime.now() - run.started).total_seconds()
            name = f"run-{run.number:03d}-{run.task}"
            body = [f"# run {run.number}: {run.task}, started {run.started:%H:%M:%S}, {secs:.1f} s",
                    "# values changed since the previous test: " + (", ".join(run.changes) or "none"),
                    "", "[result]", result_summary(result), *[row_line(r) for r in result.get("rows", [])],
                    json.dumps(result)]
            if maze:
                body += ["", "[maze]", maze_ascii(maze), "", "[maze log]", *run.log]
            (session / f"{name}.txt").write_text("\n".join(body) + "\n", encoding="utf-8")
            if run.trace:
                (session / f"run-{run.number:03d}-trace.csv").write_text(
                    run.trace_header + "\n" + "\n".join(run.trace) + "\n", encoding="utf-8")
            changed = f"  [changed: {', '.join(run.changes)}]" if run.changes else ""
            note(f"{run.number:03d} {run.started:%H:%M:%S} {run.task}: {result_summary(result)}{changed}")
            run = None
            res_seq = s["res"]
        elif not busy and s["res"] != res_seq:
            # quick command (save, zero yaw, ...) finished between two polls
            try:
                result = get(host, "/api/result")["r"]
                note(f"--- {dt.datetime.now():%H:%M:%S} {result.get('type', '?')}: {result_summary(result)}")
            except Exception:
                pass
            res_seq = s["res"]

        was_busy = busy
        time.sleep(0.4)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["find", "dump", "watch"])
    ap.add_argument("--host", help="mouse address, e.g. 192.168.43.57 (skips the search)")
    args = ap.parse_args()
    if args.command == "find":
        print(need_host(args))
    elif args.command == "dump":
        cmd_dump(args)
    else:
        try:
            cmd_watch(args)
        except KeyboardInterrupt:
            print("\nstopped")


if __name__ == "__main__":
    main()
