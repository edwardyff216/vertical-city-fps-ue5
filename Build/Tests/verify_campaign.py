"""Run cooked Development campaign routes, verify the log, and record only the game window."""
import argparse
import csv
import ctypes
from ctypes import wintypes
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import statistics
import sys
import time

ROUTES = ["UUU", "DDD", "UDU", "UUD", "UDD", "DUU", "DUD", "DDU"]
COMBAT = set(ROUTES[:3])


def game_window(pid):
    user32 = ctypes.windll.user32
    matches = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def visit(hwnd, _):
        process = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(process))
        if process.value != pid or not user32.IsWindowVisible(hwnd):
            return True
        classname = ctypes.create_unicode_buffer(128)
        user32.GetClassNameW(hwnd, classname, len(classname))
        if classname.value != "UnrealWindow":
            return True
        bounds = wintypes.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(bounds))
        if bounds.right >= 640 and bounds.bottom >= 400:
            matches.append(hwnd)
        return True

    user32.EnumWindows(visit, 0)
    return matches[0] if matches else None


def read_log(path):
    data = path.read_bytes() if path.exists() else b""
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16", errors="replace")
    return data.decode("utf-8-sig", errors="replace")


def recording(hwnd, path, diagnostic):
    source = (f"gfxcapture=hwnd={hwnd}:capture_cursor=0:capture_border=0:"
              "max_framerate=30:output_fmt=bgra,hwdownload,format=bgra")
    args = ["ffmpeg", "-hide_banner", "-loglevel", "warning", "-f", "lavfi",
            "-i", source, "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2,format=yuv420p",
            "-r", "30", "-c:v", "libx264", "-preset", "veryfast", "-crf", "21",
            "-movflags", "+faststart", "-y", str(path)]
    return subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                            stderr=diagnostic, creationflags=subprocess.CREATE_NO_WINDOW)


def stop_recording(recorder):
    if not recorder or recorder.poll() is not None:
        return
    try:
        recorder.communicate(input=b"q\n", timeout=15)
    except (BrokenPipeError, subprocess.TimeoutExpired):
        recorder.terminate()
        recorder.wait(timeout=10)


def synchronized_mix(raw, wave, movie):
    if not raw.is_file() or not wave.is_file():
        raise ValueError("Window video or Unreal master-mix WAV is missing")
    # The campaign draws this patch on the same game-thread frames that start and stop
    # the master submix recording. Capture-window startup and encoder delay may vary.
    pixels = subprocess.check_output(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(raw),
                                      "-an", "-vf", "crop=8:8:4:4,scale=1:1,format=rgb24",
                                      "-f", "rawvideo", "pipe:1"])
    frames = list(zip(pixels[0::3], pixels[1::3], pixels[2::3]))
    def find_run(predicate, begin=0):
        for index in range(begin, len(frames)-2):
            if all(predicate(*pixel) for pixel in frames[index:index+3]):
                return index
        raise ValueError("Could not identify the video synchronization marker")
    start = find_run(lambda r,g,b: r > 155 and b > 155 and g < 80)
    end = find_run(lambda r,g,b: g > 155 and r < 80 and b < 80, start+15)
    wave_info = subprocess.check_output(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                                         "-of", "default=nw=1:nk=1", str(wave)], text=True)
    audio_seconds = float(wave_info.strip())
    video_seconds = (end-start)/30
    tempo = audio_seconds/video_seconds
    if not .97 < tempo < 1.03:
        raise ValueError(f"Game audio and capture clock disagreement: {tempo:.5f}")
    # Remove the calibration flashes. Keep gameplay duration and video rate unchanged.
    trim = .55
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-ss", f"{start/30+trim:.6f}",
                    "-i", str(raw), "-i", str(wave), "-t", f"{video_seconds-trim:.6f}",
                    "-map", "0:v:0", "-map", "1:a:0", "-af", f"atempo={tempo:.8f},atrim=start={trim},asetpts=PTS-STARTPTS",
                    "-c:v", "libx264", "-preset", "veryfast", "-crf", "19", "-c:a", "aac", "-b:a", "192k",
                    "-movflags", "+faststart", "-y", str(movie)], check=True)
    sound = subprocess.run(["ffmpeg", "-hide_banner", "-i", str(wave), "-af", "volumedetect", "-f", "null", "-"],
                           capture_output=True, text=True, check=True)
    peak = re.search(r"max_volume: ([-.\d]+) dB", sound.stderr)
    if not peak or float(peak[1]) < -65:
        raise ValueError("The recorded Unreal master mix is silent")
    return {"start_marker_frame": start, "end_marker_frame": end, "video_rate": 30,
            "video_seconds_between_markers": round(video_seconds, 6), "wav_seconds": audio_seconds,
            "audio_tempo_correction": round(tempo, 8), "master_peak_db": float(peak[1])}


def validate_video(movie):
    if not movie.is_file() or movie.stat().st_size < 10000:
        return False
    output = subprocess.check_output(["ffprobe", "-v", "error", "-select_streams", "v:0",
                                      "-show_entries", "stream=codec_name,width,height,duration",
                                      "-of", "json", str(movie)], text=True)
    streams = json.loads(output)["streams"]
    if not streams:
        return False
    duration = float(streams[0].get("duration", 0))
    if duration < 8 or streams[0]["width"] < 640 or streams[0]["height"] < 400:
        return False
    sheet = movie.with_suffix(".contact.png")
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(movie), "-vf",
                    f"fps={9/duration:.8f},scale=400:-2,tile=3x3", "-frames:v", "1",
                    "-y", str(sheet)], check=True)
    return sheet.is_file()


def packaged_binary(launcher):
    inner = launcher.parent / "VerticalCityFPS" / "Binaries" / "Win64" / "VerticalCityFPS.exe"
    return inner if inner.is_file() else launcher


def starting_profile(path, build=None):
    def weapon(number, kind, frame):
        return {"id": f"{number:032x}", "template": kind, "frame": frame, "level": 1,
                "rarity": 1, "damage_roll": 10000, "affixes": []}
    data = {"version": 1, "active": False, "last_result": "", "parts": 0,
            "equipped": [weapon(1, "rifle", "automatic"), weapon(2, "sword", "sweep")], "bag": []}
    if build:
        affixes = {
            "UUU": ([], ["quick_blade", "reach", "parry_range", "frost_counter"]),
            "DDD": (["rapid", "arcane_quill", "arcane_triad", "arcane_rupture"], []),
            "UDU": (["frost_tip", "volt_conduct", "volt_link", "frost_break"], ["frost_counter", "volt_conduct"]),
        }
        for item, effects in zip(data["equipped"], affixes[build]):
            item.update(rarity=5, affixes=effects)
        data["active_skill_ids"] = ["frost_floor", "arc_lance"] if build=="UDU" else ["echo_pin", "cinch_post"]
        data["ultimate_id"] = "barrage"
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def frame_summary(path):
    if not path.exists():
        return {}
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or "elapsed_ms" not in rows[0]:
        return {}
    stats = {"sample_count": len(rows)}
    for key in ("elapsed_ms", "game_ms", "render_ms", "gpu_ms", "working_set_mib"):
        values = sorted(float(row[key]) for row in rows if row.get(key) and float(row[key]) > 0)
        if values:
            stats[key] = {"p50": round(statistics.median(values), 3),
                          "p95": round(values[min(len(values)-1, int(.95*len(values)))], 3),
                          "max": round(values[-1], 3)}
    transitions = []
    for index in range(1, len(rows)):
        if float(rows[index-1]["pawn_x"]) < 1100 or float(rows[index]["pawn_x"]) > -1000:
            continue
        nearby = rows[max(0, index-2):min(len(rows), index+4)]
        transitions.append({"after_frame": int(rows[index]["frame"]),
                            "elapsed_peak_ms": max(float(r["elapsed_ms"]) for r in nearby),
                            "render_peak_ms": max(float(r["render_ms"]) for r in nearby),
                            "gpu_peak_ms": max(float(r["gpu_ms"]) for r in nearby),
                            "peak_working_set_mib": max(float(r["working_set_mib"]) for r in nearby)})
    stats["gate_transitions"] = transitions
    return stats


def run_route(exe, folder, route, live, record, seed, performance=False, engine_user=None, showcase=False):
    folder.mkdir(parents=True)
    log = folder / "campaign.log"
    movie = folder / f"{route}-gameplay.mp4"
    raw = folder / "window-capture.mp4"
    profile = folder / "test-profile.json"
    starting_profile(profile, route if showcase else None)
    frames = folder / "frames.csv"
    args = [str(exe), "-unattended", "-nosplash",
            "-windowed", "-ResX=1280", "-ResY=800", "-ForceRes", "-NoVSync",
            f"-CityRouteTest={route}", f"-CitySeed={seed}", f"-CityProfile={profile}",
            f"-abslog={log}", "-ExecCmds=t.MaxFPS 0,r.VSync 0" if performance else "-ExecCmds=t.MaxFPS 60,au.DisableAppVolume 1"]
    if showcase:
        args.extend(["-CityBuildShowcase", "-LogCmds=LogTemp Verbose"])
    if engine_user:
        args.append(f"-UserDir={engine_user}")
    if performance:
        args.append("-CityPerf")
    if live:
        args.append(f"-CityBench={frames}")
    if not live:
        args += ["-CityRouteFast", "-nullrhi", "-nosound", "-benchmark", "-fps=60"]
    if record:
        args += ["-CityRouteRecord", f"-CityAudioDir={folder}"]
    (folder / "arguments.json").write_text(json.dumps(args, ensure_ascii=False, indent=2), encoding="utf-8")
    game = subprocess.Popen(args, cwd=exe.parent)
    started = time.monotonic()
    capture = None
    capture_error = None
    capture_began = None
    timed_out = False
    with (folder / "capture.log").open("wb") as diagnostic:
        try:
            while game.poll() is None:
                age = time.monotonic() - started
                if record and capture is None and not capture_error:
                    hwnd = game_window(game.pid)
                    if hwnd:
                        try:
                            capture = recording(hwnd, raw, diagnostic)
                            capture_began = time.monotonic()
                        except OSError as error:
                            capture_error = str(error)
                    elif age > 85:
                        capture_error = "No game window matched the packaged process."
                if capture and capture.poll() is None and time.monotonic()-capture_began > 1.0 and not (folder / "capture.ready").exists():
                    (folder / "capture.ready").touch()
                if age > 540:
                    timed_out = True
                    game.terminate()
                    break
                time.sleep(.35)
        finally:
            stop_recording(capture)
            if game.poll() is None:
                game.terminate()
            game.wait(timeout=15)
    content = read_log(log)
    saved = json.loads(read_log(profile)) if profile.exists() else {}
    advertised = re.search(r"City extract open boss=\w+ reward=(\w+)", content)
    last_pair = [w.get("template") for w in saved.get("bag", [])[-2:]]
    boss = {"UUU": "spire", "DDD": "breaker"}.get(route, "relay")
    rooms = re.findall(r"City room depth=(\d+) layout=\d+ encounter=\d+ boss=(\w+)", content)
    gates = re.findall(r"City gate crossed depth=(\d+) slot=(\d+) direction=([UD]) seed=(\d+)", content)
    offers = {(d,slot): (direction, seed) for d,slot,direction,seed in re.findall(
        r"City door offer depth=(\d+) slot=(\d+) upper=(\d+) seed=(\d+)", content)}
    arrivals = dict(re.findall(r"City room depth=(\d+).*?ticket=\w+-\d+-(\d+)", content))
    quality = {k: int(v) for k,v in re.findall(r"Applying CVar settings from Section \[(\w+Quality)@(\d+)\]", content)}
    report = {
        "quality_groups": quality,
        "route": route, "mode": "combat input" if live else "debug damage + physical movement",
        "exit_code": game.returncode, "elapsed_seconds": round(time.monotonic()-started, 2),
        "boss": boss, "seed": seed, "timeout": timed_out, "rooms": rooms, "gates": gates,
        "recording": movie.name if record else None, "capture_error": capture_error,
        "checks": {
            "exit": game.returncode == 0 and not timed_out,
            "default_entry": "LoadMap: /Game/VerticalCity/Maps/L_Campaign" in content and "Game class is 'CityCampaignMode'" in content,
            "persisted_victory": not saved.get("active", True) and saved.get("last_result") == "victory",
            "equipped_levels": [(w.get("template"), w.get("level")) for w in saved.get("equipped", [])] == [("rifle", 2), ("sword", 2)],
            "promised_boss_weapons": bool(advertised and last_pair == [advertised[1]]*2),
            "rooms": rooms == [(str(i), "none") for i in range(4)] + [("4", boss)],
            "physical_gates": len(gates) == 4 and "".join(row[2] for row in gates)[:3] == route,
            "three_world_doors": len(offers) == 12 and all(
                {offers.get((str(depth),str(slot)), (None,None))[0] for slot in range(3)} == {"0","1"}
                for depth in range(4)),
            "advertised_destination": len(gates) == 4 and all(
                offers.get((depth,slot)) == ("1" if direction == "U" else "0", seed)
                and arrivals.get(str(int(depth)+1)) == seed for depth,slot,direction,seed in gates),
            "full_clear": len(re.findall(r"City clear depth=", content)) == 5,
            "boss_reward": f"City extract open boss={boss}" in content,
            "extraction": f"City settlement victory=1 depth=4 route={route}" in content,
            "result": f"CITY_ROUTE_TEST PASS route={route} debugDamage={0 if live else 1}" in content,
        },
    }
    if performance:
        report["checks"]["uncapped"] = "City performance capture uncapped" in content
    if live:
        report["frames"] = frame_summary(frames)
        report["checks"]["frame_probe"] = report["frames"].get("sample_count", 0) >= 240
        mechanic = {"spire": "City boss fired kind=spire lanes=1", "breaker": "City boss charged kind=breaker",
                    "relay": "City boss fired kind=relay lanes=5"}[boss]
        report["checks"]["observed_boss_attack"] = mechanic in content
    if record:
        try:
            report["audio_sync"] = synchronized_mix(raw, folder / "game-mix.wav", movie)
            report["checks"]["video"] = not capture_error and validate_video(movie)
        except (OSError, subprocess.SubprocessError, ValueError) as error:
            report["checks"]["video"] = False
            report["capture_error"] = str(error)
    if showcase:
        report["checks"]["build_slots"] = "City loadout primary=" in content and "City loadout secondary=" in content
        report["checks"]["loadout_saved"] = saved.get("equipped",[{}])[0].get("rarity")==5 and saved.get("ultimate_id")=="barrage"
    report["passed"] = all(report["checks"].values())
    (folder / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--routes", nargs="+", choices=ROUTES, default=ROUTES)
    parser.add_argument("--no-video", action="store_true")
    parser.add_argument("--performance", action="store_true", help="Uncapped real combat without recording, with VSync and frame smoothing disabled.")
    parser.add_argument("--fast", action="store_true", help="Use debug damage for every requested route.")
    parser.add_argument("--showcase", action="store_true", help="Play the three representative affix builds on UUU, DDD and UDU.")
    parser.add_argument("--all-combat", action="store_true", help="Use real shooting for every requested route.")
    args = parser.parse_args()
    exe = packaged_binary(args.exe.resolve())
    if not exe.is_file():
        parser.error(f"Packaged binary missing: {exe}")
    if not args.no_video and not args.performance and not args.fast and not (shutil.which("ffmpeg") and shutil.which("ffprobe")):
        parser.error("ffmpeg and ffprobe are required for window-scoped video evidence")
    out = args.out.resolve() / datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    out.mkdir(parents=True)
    print(f"Evidence {out}", flush=True)
    engine_user = args.out.resolve() / "EngineUser"
    engine_user.mkdir(parents=True, exist_ok=True)
    results = []
    for route in args.routes:
        live = not args.fast and (args.all_combat or route in COMBAT)
        print(f"{route} {'combat' if live else 'debug'}", flush=True)
        result = run_route(exe, out/route, route, live, live and not args.no_video and not args.performance, 20261120+ROUTES.index(route), args.performance, engine_user, args.showcase and route in COMBAT and live)
        results.append(result)
        print(json.dumps(result, ensure_ascii=True), flush=True)
        if not result["passed"]:
            break
    summary = {"executable": str(exe), "sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
               "requested_routes": args.routes, "results": results}
    (out / "report.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0 if len(results) == len(args.routes) and all(row["passed"] for row in results) else 1


if __name__ == "__main__":
    sys.exit(main())
