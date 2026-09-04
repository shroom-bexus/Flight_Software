#!/usr/bin/env python3
"""Download SHROOM CSV logs from a Teensy over USB serial.

Install pyserial once:
    python -m pip install pyserial

Examples:
    python tools/download_logs.py /dev/ttyACM0 --storage BACKUP --list
    python tools/download_logs.py /dev/ttyACM0 --storage BACKUP
    python tools/download_logs.py COM4 --storage INTERNAL --file airdos.csv
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import time
import zlib

try:
    import serial
except ImportError:
    sys.exit("pyserial fehlt: python -m pip install pyserial")


BAUD_RATE = 115200
COMMAND_TIMEOUT_S = 10
TRANSFER_TIMEOUT_S = 5


def send_command(port: serial.Serial, command: str) -> None:
    port.write((command + "\n").encode("ascii"))
    port.flush()


def read_nonempty_line(port: serial.Serial, deadline: float) -> str:
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        line = raw.decode("ascii", errors="replace").strip()
        if line:
            return line
    raise TimeoutError("Keine Antwort vom Teensy")


def enter_download_mode(port: serial.Serial) -> None:
    send_command(port, "DOWNLOAD,ENTER")
    deadline = time.monotonic() + COMMAND_TIMEOUT_S

    # Ignore boot/debug lines that may still be waiting in the USB buffer.
    while True:
        line = read_nonempty_line(port, deadline)
        if line.startswith("DOWNLOAD_READY,"):
            return
        if line.startswith("DOWNLOAD_ERROR,"):
            raise RuntimeError(line)


def list_files(port: serial.Serial, storage: str) -> dict[str, int]:
    send_command(port, f"DOWNLOAD,LIST,{storage}")
    deadline = time.monotonic() + COMMAND_TIMEOUT_S
    files: dict[str, int] = {}
    started = False

    while True:
        line = read_nonempty_line(port, deadline)
        if line == f"LIST_BEGIN,{storage}":
            started = True
            continue
        if line == f"LIST_END,{storage}" and started:
            return files
        if line.startswith("DOWNLOAD_ERROR,"):
            raise RuntimeError(line)
        if started and line.startswith("FILE,"):
            _, filename, size_text = line.split(",", 2)
            files[filename] = int(size_text)


def read_exact(port: serial.Serial, output, size: int) -> int:
    remaining = size
    received = 0
    crc = 0
    last_progress = 0.0

    while remaining:
        chunk = port.read(min(64 * 1024, remaining))
        if not chunk:
            raise TimeoutError("Dateiübertragung unterbrochen")

        output.write(chunk)
        crc = zlib.crc32(chunk, crc)
        received += len(chunk)
        remaining -= len(chunk)

        now = time.monotonic()
        if now - last_progress >= 0.25 or remaining == 0:
            percent = 100.0 if size == 0 else received * 100.0 / size
            print(
                f"\r  {percent:6.2f}%  "
                f"{received / 1_048_576:.2f}/{size / 1_048_576:.2f} MiB",
                end="",
                flush=True,
            )
            last_progress = now

    print()
    return crc & 0xFFFFFFFF


def download_file(
    port: serial.Serial,
    storage: str,
    filename: str,
    expected_size: int,
    destination: Path,
    overwrite: bool,
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and not overwrite:
        print(f"Übersprungen (existiert): {destination}")
        return

    print(f"Lade {storage}/{filename} -> {destination}")
    send_command(port, f"DOWNLOAD,GET,{storage},{filename}")

    line = read_nonempty_line(
        port,
        time.monotonic() + COMMAND_TIMEOUT_S,
    )
    if line.startswith("DOWNLOAD_ERROR,"):
        raise RuntimeError(line)

    parts = line.split(",", 3)
    if len(parts) != 4 or parts[:3] != ["FILE_BEGIN", storage, filename]:
        raise RuntimeError(f"Unerwartete Antwort: {line}")

    announced_size = int(parts[3])
    if announced_size != expected_size:
        raise RuntimeError(
            f"Dateigröße hat sich geändert: {expected_size} -> {announced_size}"
        )

    partial = destination.with_suffix(destination.suffix + ".part")
    old_timeout = port.timeout
    port.timeout = TRANSFER_TIMEOUT_S
    try:
        with partial.open("wb") as output:
            calculated_crc = read_exact(port, output, announced_size)
    except Exception:
        print(f"Unvollständige Datei bleibt erhalten: {partial}")
        raise
    finally:
        port.timeout = old_timeout

    end_line = read_nonempty_line(
        port,
        time.monotonic() + COMMAND_TIMEOUT_S,
    )
    end_parts = end_line.split(",")
    if len(end_parts) != 3 or end_parts[0] != "FILE_END":
        raise RuntimeError(f"Fehlendes FILE_END: {end_line}")

    completed_size = int(end_parts[1])
    received_crc = int(end_parts[2], 16)
    if completed_size != announced_size or received_crc != calculated_crc:
        raise RuntimeError(
            "Prüfsumme stimmt nicht; die .part-Datei wird nicht übernommen"
        )

    os.replace(partial, destination)
    print(f"Fertig, CRC32={calculated_crc:08X}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SHROOM-Logs über Teensy USB herunterladen"
    )
    parser.add_argument("port", help="z. B. /dev/ttyACM0 oder COM4")
    parser.add_argument(
        "--storage",
        choices=("INTERNAL", "BACKUP"),
        default="BACKUP",
        help="Speicher auswählen (Standard: BACKUP)",
    )
    parser.add_argument(
        "--file",
        action="append",
        dest="files",
        help="nur diese Datei laden; mehrfach verwendbar",
    )
    parser.add_argument("--list", action="store_true", help="nur Dateien anzeigen")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("downloaded_logs"),
        help="Zielverzeichnis (Standard: downloaded_logs)",
    )
    parser.add_argument("--overwrite", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    with serial.Serial(
        args.port,
        BAUD_RATE,
        timeout=0.5,
        write_timeout=2,
    ) as port:
        # Let an already-running Teensy finish any pending USB output.
        time.sleep(0.5)
        port.reset_input_buffer()
        enter_download_mode(port)

        available = list_files(port, args.storage)
        if not available:
            print(f"Keine Dateien auf {args.storage} gefunden.")
            return

        print(f"Dateien auf {args.storage}:")
        for filename, size in available.items():
            print(f"  {filename:<18} {size / 1_048_576:9.2f} MiB")

        if args.list:
            return

        selected = args.files or list(available)
        unknown = [filename for filename in selected if filename not in available]
        if unknown:
            raise RuntimeError(f"Nicht vorhanden: {', '.join(unknown)}")

        for filename in selected:
            destination = args.output / args.storage.lower() / filename
            download_file(
                port,
                args.storage,
                filename,
                available[filename],
                destination,
                args.overwrite,
            )

    print("Downloadmodus beenden: Teensy resetten.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        sys.exit(f"Fehler: {error}")
