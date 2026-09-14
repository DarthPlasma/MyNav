#!/usr/bin/env python3
"""
MyNAV build tool.

A small local web page to build the firmware for the project's targets: pick a target, build,
download the .hex. INAV has no per-feature build options (ADS-B is on for every GPS target), so
there is nothing else to choose. It drives CMake in <repo>/build, configuring it on first use.

Run:  python3 mynav-tools/build-tool.py [--toolchain-bin DIR]    then open http://localhost:8792

--toolchain-bin (or the MYNAV_TOOLCHAIN_BIN environment variable) is an arm-none-eabi toolchain
other than the one INAV downloads into tools/. Its version is not checked, otherwise INAV's CMake
would download its own. cmake is taken from tools/cmake-venv when present, else from PATH.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import http.server

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # repo root = parent of mynav-tools/
BUILD_DIR = os.path.join(REPO, "build")
PORT = 8792
TIMEOUT_S = 1800

# The MyTAflight targets that exist in INAV under the same name (KAKUTEH7 also covers the Kakute H7 V1.3),
# then the MyTAflight boards matched pin by pin against their Betaflight configs.
TARGETS = ["DAKEFPVH743", "TMOTORVELOXF7V2", "KAKUTEH7", "MAMBAH743", "MAMBAH743_2022B",
           "IFLIGHT_BLITZ_H7_PRO", "MATEKH743", "SPEEDYBEEF405V4", "FLYWOOH743PRO", "GEPRC_TAKER_H743",
           "GEPRCF745_BT_HD", "KAKUTEF7", "FLYWOOF411", "SPEEDYBEEF405V3", "SPEEDYBEEF405MINI",
           "SPEEDYBEEF7V3", "ZEEZF7", "ZEEZF7V2", "ZEEZF7V3", "IFLIGHT_BLITZ_F7_PRO",
           "IFLIGHT_BLITZ_F7_AIO", "IFLIGHT_BLITZ_F722", "IFLIGHT_H743_AIO_V2",
           "SPEEDYBEEF745AIO",    # Betaflight SPEEDYBEE_F745_AIO: the same board
           "FLYWOOF722PRO",       # Betaflight FLYWOOF722PROV2: INAV's target already has the V2 gyro
           "FOXEERF745AIO",       # Betaflight FOXEERF745V4_AIO
           "SPEEDYBEEF405AIOV2",  # MyNAV variant of SPEEDYBEEF405AIO
           "FLYWOOF745AIOV2",     # MyNAV variant of FLYWOOF745 (Explorer LR 4" V2 HD)
           "TMOTORF7_AIO"]        # MyNAV variant of TMOTORF7

# "FLASH1:  654319 B  1792 KB  35.66%" from the linker's --print-memory-usage
MEMORY_REGION = re.compile(r"^\s*(FLASH\w*):\s+([\d.]+)\s*(B|KB|MB)\s+([\d.]+)\s*(B|KB|MB)\s+([\d.]+)%", re.M)
UNITS = {"B": 1, "KB": 1024, "MB": 1024 * 1024}


def defined_targets():
    names = set()
    for path in glob.glob(os.path.join(REPO, "src", "main", "target", "*", "CMakeLists.txt")):
        with open(path) as f:
            names.update(re.findall(r"^\s*target_\w+\(\s*(\w+)", f.read(), flags=re.M))
    return names


def cmake_command():
    venv_cmake = os.path.join(REPO, "tools", "cmake-venv", "bin", "cmake")
    return venv_cmake if os.path.isfile(venv_cmake) else "cmake"


def flash_usage(log):
    # The firmware lives in the largest FLASH region; FLASH_CONFIG holds the settings
    regions = [(float(used) * UNITS[unit], name, percent)
               for name, used, unit, _size, _size_unit, percent in MEMORY_REGION.findall(log)
               if name != "FLASH_CONFIG"]
    if not regions:
        return ""
    _used, name, percent = max(regions)
    return "%s%% of %s" % (percent, name)


def find_hex(target):
    pattern = re.compile(r"^inav_[^_]+_" + re.escape(target) + r"\.hex$")
    hexes = [path for path in glob.glob(os.path.join(BUILD_DIR, "inav_*.hex")) if pattern.match(os.path.basename(path))]
    return os.path.basename(max(hexes, key=os.path.getmtime)) if hexes else ""


def run_build(target, toolchain_bin):
    if target not in TARGETS:
        return {"ok": False, "cmd": "", "log": "Unknown target."}

    cmake = cmake_command()
    # The build's own steps run cmake and the toolchain by name
    path = [os.path.dirname(cmake)] if os.path.isabs(cmake) else []
    if toolchain_bin:
        path.insert(0, toolchain_bin)
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(path + [env.get("PATH", "")])
    steps = []

    if not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt")):
        configure = [cmake, "-S", REPO, "-B", BUILD_DIR, "-DCMAKE_BUILD_TYPE=Release"]
        if toolchain_bin:
            configure.append("-DCOMPILER_VERSION_CHECK=OFF")
        steps.append(configure)
    steps.append([cmake, "--build", BUILD_DIR, "--target", target, "-j", str(os.cpu_count() or 4)])

    log = ""
    shown = "\n".join("$ " + " ".join(step) for step in steps)
    for step in steps:
        try:
            proc = subprocess.run(step, cwd=REPO, env=env, capture_output=True, text=True, timeout=TIMEOUT_S)
        except subprocess.TimeoutExpired:
            return {"ok": False, "cmd": shown, "log": "Timed out after %d s." % TIMEOUT_S}
        log += proc.stdout + proc.stderr
        if proc.returncode != 0:
            return {"ok": False, "cmd": shown, "log": "\n".join(log.strip().splitlines()[-60:])}

    tail = "\n".join(log.strip().splitlines()[-40:])
    return {"ok": True, "cmd": shown, "log": tail, "flash": flash_usage(log), "hex": find_hex(target)}


PAGE = r"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>MyNAV Build Tool</title>
<style>
 :root{--bg:#12151c;--panel:#1b2029;--line:#2c333f;--fg:#e6e9ef;--muted:#8b93a3;--accent:#4ea1ff;--ok:#3ad29f;--err:#ff5d5d;}
 *{box-sizing:border-box} body{margin:0;font:14px/1.45 -apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
 header{padding:14px 18px;border-bottom:1px solid var(--line)} header h1{margin:0;font-size:16px} header p{margin:4px 0 0;color:var(--muted);font-size:12px}
 main{max-width:900px;margin:0 auto;padding:16px}
 .panel{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px;margin-bottom:16px}
 h2{margin:0 0 10px;font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
 label{font-size:13px} select{background:#0c0f15;color:var(--fg);border:1px solid var(--line);border-radius:5px;padding:6px 8px;font:inherit}
 .row{display:flex;gap:16px;align-items:center;flex-wrap:wrap}
 .muted{color:var(--muted);font-size:12px}
 button{background:var(--accent);color:#04101f;border:0;border-radius:6px;padding:9px 16px;font-weight:700;cursor:pointer;font:inherit}
 button:disabled{opacity:.5;cursor:default}
 code{background:#0c0f15;border:1px solid var(--line);border-radius:5px;padding:2px 6px;font-size:12px}
 pre{background:#0a0d12;border:1px solid var(--line);border-radius:6px;padding:10px;overflow:auto;font-size:12px;max-height:360px;white-space:pre-wrap}
 .pill{display:inline-block;padding:2px 8px;border-radius:20px;font-size:12px;font-weight:700}
 .pill.ok{background:rgba(58,210,159,.15);color:var(--ok)} .pill.err{background:rgba(255,93,93,.15);color:var(--err)}
 a.dl{color:var(--ok);font-weight:700}
</style></head><body>
<header><h1>MyNAV — Build Tool</h1><p>Pick a target, build, download the <code>.hex</code>. Runs CMake in <code>build/</code> (configured on first use).</p></header>
<main>
 <div class="panel">
   <div class="row">
     <label>Target <select id="target"></select></label>
     <button id="buildBtn">Build firmware</button>
   </div>
   <p class="muted" id="toolchain"></p>
 </div>

 <div class="panel" id="resultPanel" style="display:none">
   <h2>Result</h2>
   <div class="row" style="margin-bottom:8px"><span id="status" class="pill"></span>
     <span id="flash" class="muted"></span> <span id="dl"></span></div>
   <pre id="log"></pre>
 </div>
</main>
<script>
const INFO = __INFO__;
const $=id=>document.getElementById(id);
$("target").innerHTML = INFO.targets.map(t=>`<option>${t}</option>`).join("");
$("toolchain").textContent = "cmake: " + INFO.cmake + " · toolchain: " + (INFO.toolchain || "INAV's own (tools/ or PATH)");

$("buildBtn").onclick = async () => {
  const btn = $("buildBtn"); btn.disabled = true; btn.textContent = "Building…";
  $("resultPanel").style.display = "block"; $("status").className="pill"; $("status").textContent="running";
  $("flash").textContent=""; $("dl").innerHTML="";
  $("log").textContent="Building, please wait (the first build of a target takes a few minutes)…";
  try {
    const r = await fetch("/build", {method:"POST", headers:{"Content-Type":"application/json"},
      body: JSON.stringify({target:$("target").value})});
    const d = await r.json();
    $("status").className = "pill " + (d.ok?"ok":"err"); $("status").textContent = d.ok?"success":"failed";
    $("flash").textContent = d.flash ? ("flash " + d.flash) : "";
    $("log").textContent = (d.cmd ? d.cmd + "\n\n" : "") + d.log;
    $("dl").innerHTML = (d.ok && d.hex) ? `<a class="dl" href="/hex/${encodeURIComponent(d.hex)}" download>⤓ ${d.hex}</a>` : "";
  } catch(e){ $("status").className="pill err"; $("status").textContent="error"; $("log").textContent=String(e); }
  btn.disabled=false; btn.textContent="Build firmware";
};
</script></body></html>"""


def make_handler(toolchain_bin):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _send(self, code, body, ctype="text/html; charset=utf-8", extra=None):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            for key, value in (extra or {}).items():
                self.send_header(key, value)
            self.end_headers()
            self.wfile.write(body if isinstance(body, bytes) else body.encode())

        def do_GET(self):
            if self.path == "/" or self.path.startswith("/index"):
                info = {"targets": TARGETS, "cmake": cmake_command(), "toolchain": toolchain_bin}
                self._send(200, PAGE.replace("__INFO__", json.dumps(info)))
            elif self.path.startswith("/hex/"):
                name = os.path.basename(self.path[len("/hex/"):])
                path = os.path.join(BUILD_DIR, name)
                if re.match(r"^inav_[\w.]+\.hex$", name) and os.path.isfile(path):
                    with open(path, "rb") as f:
                        self._send(200, f.read(), "application/octet-stream",
                                   {"Content-Disposition": 'attachment; filename="%s"' % name})
                else:
                    self._send(404, "not found", "text/plain")
            else:
                self._send(404, "not found", "text/plain")

        def do_POST(self):
            if self.path != "/build":
                self._send(404, "not found", "text/plain")
                return
            length = int(self.headers.get("Content-Length", 0))
            try:
                req = json.loads(self.rfile.read(length) or b"{}")
            except ValueError:
                req = {}
            self._send(200, json.dumps(run_build(req.get("target", ""), toolchain_bin)), "application/json")

    return Handler


def main():
    parser = argparse.ArgumentParser(description="MyNAV build tool")
    parser.add_argument("--toolchain-bin", default=os.environ.get("MYNAV_TOOLCHAIN_BIN", ""),
                        help="bin/ directory of an arm-none-eabi toolchain to use instead of INAV's own")
    parser.add_argument("--port", type=int, default=PORT)
    args = parser.parse_args()

    missing = sorted(set(TARGETS) - defined_targets())
    if missing:
        print("Warning, not defined in src/main/target: " + ", ".join(missing))

    print("MyNAV build tool — repo: " + REPO)
    print("Open http://localhost:%d" % args.port)
    http.server.HTTPServer(("127.0.0.1", args.port), make_handler(args.toolchain_bin)).serve_forever()


if __name__ == "__main__":
    main()
