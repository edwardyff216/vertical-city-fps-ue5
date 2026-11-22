"""Assemble the three completed campaign recordings from their logged boss-room entrances."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess


def stamp(log, pattern, last=False):
    lines=log.splitlines()
    for line in (reversed(lines) if last else lines):
        if re.search(pattern, line):
            match = re.search(r"\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d+)\]", line)
            if match:
                return datetime.strptime(match[1], "%Y.%m.%d-%H.%M.%S:%f")
    raise ValueError(f"Missing campaign log event: {pattern}")


def read_log(path):
    data = path.read_bytes()
    codec = "utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
    return data.decode(codec, errors="replace")


def make_reel(runs, output):
    clips = []
    output.parent.mkdir(parents=True,exist_ok=True)
    args = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
    for route, directory in zip(("UUU","DDD","UDU"), runs, strict=True):
        result = json.loads((directory / "result.json").read_text(encoding="utf-8"))
        if result["route"] != route or result["mode"] != "combat input" or not result["passed"]:
            raise ValueError(f"Expected a passed and recorded {route} combat run")
        log = read_log(directory / "campaign.log")
        start = stamp(log, r"City master mix started")
        end = stamp(log, r"City master mix stopped")
        boss = stamp(log, r"City room depth=4 ")
        span = result["audio_sync"]["video_seconds_between_markers"]
        # Use the same two observed markers that aligned this recording's master audio.
        scene_offset = (boss-start).total_seconds() / (end-start).total_seconds() * span - .55
        seek = max(0, scene_offset-.45)
        movie = directory / result["recording"]
        args.extend(["-ss", f"{seek:.4f}", "-i", str(movie)])
        attack_event = { "UUU": r"City boss fired kind=spire", "DDD": r"City boss charged kind=breaker",
                         "UDU": r"City boss fired kind=relay" }[route]
        attack = stamp(log,attack_event,last=route=="UUU")
        attack_offset = (attack-start).total_seconds()/(end-start).total_seconds()*span-.55
        shot = output.parent / (result["boss"]+"-telegraph.png")
        subprocess.run(["ffmpeg","-hide_banner","-loglevel","error","-ss",f"{max(0,attack_offset-.32):.4f}",
                        "-i",str(movie),"-frames:v","1","-y",str(shot)],check=True)
        clips.append({"route": route, "boss": result["boss"], "source": str(movie),
                      "start_seconds": round(seek, 4), "boss_room_seconds": round(scene_offset, 4),
                      "sampled_attack_seconds": round(attack_offset, 4), "telegraph": shot.name,
                      "sha256": hashlib.sha256(movie.read_bytes()).hexdigest()})
    inputs = "".join(f"[{i}:v:0][{i}:a:0]" for i in range(3))
    args.extend(["-filter_complex", inputs+"concat=n=3:v=1:a=1[v][a]", "-map", "[v]", "-map", "[a]",
                 "-c:v", "libx264", "-preset", "veryfast", "-crf", "19", "-c:a", "aac", "-b:a", "192k",
                 "-movflags", "+faststart", str(output)])
    output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(args, check=True)
    probe = subprocess.check_output(["ffprobe", "-v", "error", "-show_entries",
                                     "stream=codec_type,codec_name,width,height:format=duration",
                                     "-of", "json", str(output)], text=True)
    media = json.loads(probe)
    if {s["codec_type"] for s in media["streams"]} != {"audio","video"}:
        raise ValueError("The three-boss reel must have both picture and game audio")
    output.with_suffix(".json").write_text(json.dumps({"clips":clips,"media":media},ensure_ascii=False,indent=2),encoding="utf-8")


if __name__ == "__main__":
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--runs",type=Path,nargs=3,required=True,metavar=("UUU","DDD","UDU"))
    cli.add_argument("--out",type=Path,required=True)
    settings=cli.parse_args()
    make_reel(settings.runs,settings.out)
