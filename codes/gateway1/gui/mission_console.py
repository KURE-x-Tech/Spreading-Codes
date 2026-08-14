"""LunaNet mission console for the Gateway 1-6 operational workflow.

The console intentionally presents receiver and navigation states rather than
raw spreading-code, FEC, or payload bit streams. It invokes the locally built
``goon`` CLI for the actual G2-G6 pipeline and uses the C API bridge for G1.
"""

from __future__ import annotations

import json
import subprocess
import sys
import threading
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from tkinter import ttk
from typing import Any, Callable

_PYTHON_DIR = Path(__file__).resolve().parents[2] / "python"
if str(_PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(_PYTHON_DIR))


@dataclass
class StageState:
    title: str
    detail: str
    status: str = "STANDBY"


class MissionConsole(ttk.Frame):
    """An operator-facing panel that runs and visualizes the LSIS-AFS chain."""

    def __init__(self, parent: tk.Misc, palette: dict[str, str], repo_root: Path) -> None:
        super().__init__(parent, style="Dark.TFrame")
        self._c = palette
        self._repo_root = Path(repo_root)
        self._goon = self._repo_root / "build" / "bin" / "goon"
        self._run_dir = self._repo_root / "Validation" / "generated" / "mission_console"
        self._iq_path = self._run_dir / "uplink.iq32"
        self._decode_path = self._run_dir / "receiver.json"
        self._bridge: Any = None
        self._busy = False
        self._data: dict[str, Any] | None = None
        self._stage_labels: dict[str, ttk.Label] = {}
        self._stage_details: dict[str, ttk.Label] = {}
        self._states = {
            "g1": StageState("G1  SPREADING", "Awaiting PRN assignment"),
            "g2": StageState("G2  PROTECTION", "BCH · CRC-24Q · LDPC · 60 x 98"),
            "g3": StageState("G3  FRAME", "12-second navigation frame"),
            "g4": StageState("G4  UPLINK", "AFS-I/AFS-Q baseband signal"),
            "link": StageState("EARTH -> ORBIT", "Waiting to transmit"),
            "g5": StageState("G5  RECEIVER", "Awaiting downlink"),
            "g6": StageState("G6  NAVIGATION", "Awaiting parsed message"),
        }

        self._configure_styles()
        self._build_header()
        self._build_workspace()
        self._refresh_stages()

    def _configure_styles(self) -> None:
        style = ttk.Style(self)
        c = self._c
        style.configure("Mission.TLabel", background=c["BG_DARK"], foreground=c["FG_SECONDARY"],
                        font=("Segoe UI", 10))
        style.configure("MissionValue.TLabel", background=c["BG_SURFACE"], foreground=c["FG_PRIMARY"],
                        font=("Segoe UI Semibold", 11))
        style.configure("MissionHeading.TLabel", background=c["BG_DARK"], foreground=c["ACCENT"],
                        font=("Segoe UI Semibold", 16))
        style.configure("MissionCaption.TLabel", background=c["BG_DARK"], foreground=c["FG_MUTED"],
                        font=("Segoe UI", 9))
        style.configure("MissionCard.TFrame", background=c["BG_SURFACE"])
        style.configure("MissionAction.TButton", background=c["ACCENT"], foreground="#101014",
                        font=("Segoe UI Semibold", 10), padding=(10, 6))
        style.map("MissionAction.TButton", background=[("active", "#5d8bdb"), ("disabled", c["BG_ELEVATED"])])

    def _build_header(self) -> None:
        header = ttk.Frame(self, style="Dark.TFrame")
        header.pack(fill=tk.X, padx=14, pady=(12, 6))
        ttk.Label(header, text="LUNANET MISSION CONSOLE", style="MissionHeading.TLabel").pack(side=tk.LEFT)
        ttk.Label(header, text="EARTH STATION / ORBITAL NAVIGATION LINK", style="MissionCaption.TLabel").pack(side=tk.LEFT, padx=14)
        self._mission_badge = ttk.Label(header, text="STANDBY", style="MissionValue.TLabel", padding=(10, 4))
        self._mission_badge.pack(side=tk.RIGHT)

    def _build_workspace(self) -> None:
        main = ttk.Frame(self, style="Dark.TFrame")
        main.pack(fill=tk.BOTH, expand=True, padx=14, pady=(0, 10))
        main.columnconfigure(0, weight=1, minsize=260)
        main.columnconfigure(1, weight=2, minsize=500)
        main.rowconfigure(0, weight=1)

        left = ttk.Frame(main, style="Dark.TFrame")
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        self._build_controls(left)
        self._build_actions(left)
        self._build_telemetry(left)

        right = ttk.Frame(main, style="Dark.TFrame")
        right.grid(row=0, column=1, sticky="nsew")
        self._build_link_scene(right)
        self._build_stage_grid(right)
        self._build_navigation(right)

    def _spin(self, parent: tk.Misc, variable: tk.StringVar, low: int, high: int, width: int) -> tk.Spinbox:
        return tk.Spinbox(parent, from_=low, to=high, textvariable=variable, width=width,
                          bg=self._c["BG_SURFACE"], fg=self._c["FG_PRIMARY"],
                          buttonbackground=self._c["BG_ELEVATED"], insertbackground=self._c["FG_PRIMARY"],
                          highlightthickness=1, highlightbackground=self._c["BORDER_CLR"],
                          highlightcolor=self._c["ACCENT"], relief=tk.FLAT, justify=tk.RIGHT,
                          font=("Consolas", 10))

    def _build_controls(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="MissionCard.TFrame", padding=10)
        frame.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(frame, text="MISSION PROFILE", style="Section.TLabel").pack(anchor=tk.W)
        fields = (("PRN", "prn", 1, 210, "1"), ("FID", "fid", 0, 3, "0"),
                  ("TOI", "toi", 0, 99, "42"), ("WEEK", "wn", 0, 8191, "100"),
                  ("INTERVAL", "itow", 0, 503, "250"))
        self._vars: dict[str, tk.StringVar] = {}
        grid = ttk.Frame(frame, style="MissionCard.TFrame")
        grid.pack(fill=tk.X, pady=(8, 0))
        for index, (label, key, low, high, default) in enumerate(fields):
            self._vars[key] = tk.StringVar(value=default)
            ttk.Label(grid, text=label, style="MissionCaption.TLabel").grid(row=index, column=0, sticky=tk.W, pady=3)
            self._spin(grid, self._vars[key], low, high, 8).grid(row=index, column=1, sticky=tk.E, pady=3)
        grid.columnconfigure(1, weight=1)

    def _build_actions(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="MissionCard.TFrame", padding=10)
        frame.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(frame, text="OPERATIONS", style="Section.TLabel").pack(anchor=tk.W, pady=(0, 8))
        actions: list[tuple[str, Callable[[], None]]] = [
            ("Generate G1 Spreading", self._run_g1),
            ("Prepare G2-G4 Uplink", self._prepare_uplink),
            ("Send to Satellite", self._transmit),
            ("Receive and Decode", self._receive),
        ]
        self._buttons: list[ttk.Button] = []
        for text, action in actions:
            button = ttk.Button(frame, text=text, style="MissionAction.TButton", command=action)
            button.pack(fill=tk.X, pady=3)
            self._buttons.append(button)

    def _build_telemetry(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="MissionCard.TFrame", padding=10)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, text="LINK HEALTH", style="Section.TLabel").pack(anchor=tk.W)
        self._telemetry = tk.Text(frame, height=8, wrap=tk.WORD, relief=tk.FLAT,
                                  bg=self._c["BG_SURFACE"], fg=self._c["FG_SECONDARY"],
                                  insertbackground=self._c["FG_PRIMARY"], font=("Segoe UI", 10),
                                  padx=2, pady=8)
        self._telemetry.pack(fill=tk.BOTH, expand=True)
        self._set_telemetry("Set a mission profile, generate the spreading identity, then prepare an uplink.")

    def _build_link_scene(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="MissionCard.TFrame", padding=8)
        frame.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(frame, text="EARTH / ORBITAL LINK", style="Section.TLabel").pack(anchor=tk.W)
        self._scene = tk.Canvas(frame, height=126, bg=self._c["BG_SURFACE"], highlightthickness=0)
        self._scene.pack(fill=tk.X, pady=(4, 0))
        self._scene.bind("<Configure>", lambda _event: self._draw_scene())
        self._draw_scene()

    def _draw_scene(self) -> None:
        canvas = self._scene
        width, height = max(canvas.winfo_width(), 420), max(canvas.winfo_height(), 126)
        canvas.delete("all")
        accent = self._c["ACCENT"]
        muted = self._c["FG_MUTED"]
        canvas.create_arc(-80, height - 90, width + 80, height + 150, start=190, extent=160,
                          outline="#31516f", width=3, style=tk.ARC)
        canvas.create_oval(54, height - 38, 84, height - 8, outline=accent, width=2)
        canvas.create_line(69, height - 38, 69, height - 80, fill=accent, width=2)
        canvas.create_line(49, height - 55, 69, height - 80, fill=accent)
        canvas.create_line(89, height - 55, 69, height - 80, fill=accent)
        sx, sy = width - 100, 42
        canvas.create_oval(sx - 10, sy - 10, sx + 10, sy + 10, outline=accent, width=2)
        canvas.create_line(sx - 40, sy, sx + 40, sy, fill=accent, width=2)
        canvas.create_line(sx, sy - 40, sx, sy + 40, fill=accent, width=2)
        active = self._states["link"].status == "TRANSMITTING"
        canvas.create_line(85, height - 72, sx - 42, sy + 12, fill=accent if active else muted,
                           width=2, dash=() if active else (5, 4))
        canvas.create_text(69, height - 4, text="EARTH STATION", fill=self._c["FG_SECONDARY"], font=("Segoe UI", 8))
        canvas.create_text(sx, sy + 56, text="LUNANET NODE", fill=self._c["FG_SECONDARY"], font=("Segoe UI", 8))

    def _build_stage_grid(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="Dark.TFrame")
        frame.pack(fill=tk.BOTH, expand=True, pady=(0, 8))
        for index, key in enumerate(("g1", "g2", "g3", "g4", "link", "g5", "g6")):
            row, column = divmod(index, 2)
            card = ttk.Frame(frame, style="MissionCard.TFrame", padding=10)
            card.grid(row=row, column=column, sticky="nsew", padx=(0, 6) if column == 0 else (6, 0), pady=5)
            title = ttk.Label(card, text=self._states[key].title, style="MissionValue.TLabel")
            title.pack(anchor=tk.W)
            status = ttk.Label(card, text="", style="MissionCaption.TLabel")
            status.pack(anchor=tk.W, pady=(5, 2))
            detail = ttk.Label(card, text="", style="MissionCaption.TLabel", wraplength=260)
            detail.pack(anchor=tk.W)
            self._stage_labels[key] = status
            self._stage_details[key] = detail
        frame.columnconfigure(0, weight=1)
        frame.columnconfigure(1, weight=1)

    def _build_navigation(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="MissionCard.TFrame", padding=10)
        frame.pack(fill=tk.X)
        ttk.Label(frame, text="PARSED NAVIGATION MESSAGE", style="Section.TLabel").pack(anchor=tk.W)
        self._navigation = tk.Text(frame, height=7, wrap=tk.WORD, relief=tk.FLAT,
                                   bg=self._c["BG_SURFACE"], fg=self._c["FG_PRIMARY"],
                                   insertbackground=self._c["FG_PRIMARY"], font=("Segoe UI", 11),
                                   padx=2, pady=8)
        self._navigation.pack(fill=tk.X)
        self._set_navigation("No downlink decoded. Parsed navigation will appear here after reception.")

    def _read_profile(self) -> dict[str, int]:
        ranges = {"prn": (1, 210), "fid": (0, 3), "toi": (0, 99), "wn": (0, 8191), "itow": (0, 503)}
        profile: dict[str, int] = {}
        for key, (low, high) in ranges.items():
            try:
                value = int(self._vars[key].get())
            except ValueError as exc:
                raise ValueError(f"{key.upper()} must be a whole number") from exc
            if not low <= value <= high:
                raise ValueError(f"{key.upper()} must be between {low} and {high}")
            profile[key] = value
        return profile

    def _load_bridge(self) -> Any:
        if self._bridge is None:
            from lunanet import LunaNet  # type: ignore
            self._bridge = LunaNet(self._repo_root / "config" / "spreading_codes_config.ini")
        return self._bridge

    def _run_g1(self) -> None:
        try:
            profile = self._read_profile()
            bridge = self._load_bridge()
            code = bridge.generate_gold(profile["prn"])
        except Exception as exc:
            self._fail(f"Gateway 1 could not generate a spreading identity: {exc}")
            return
        ones = sum(code)
        self._set_state("g1", "READY", f"PRN {profile['prn']} Gold sequence · 2,046 chips · {ones:,} transitions")
        self._set_telemetry(f"Gateway 1 assigned PRN {profile['prn']} to this mission. The Gold sequence is ready for data spreading.")
        self._mission_badge.configure(text="MISSION CONFIGURED")

    def _prepare_uplink(self) -> None:
        try:
            profile = self._read_profile()
        except ValueError as exc:
            self._fail(str(exc))
            return
        self._run_background("Preparing protected 12-second uplink", lambda: self._encode(profile), self._uplink_ready)

    def _encode(self, profile: dict[str, int]) -> dict[str, Any]:
        self._run_dir.mkdir(parents=True, exist_ok=True)
        command = [str(self._goon), "encode", "--format", "iq32", "--prn", str(profile["prn"]),
                   "--fid", str(profile["fid"]), "--toi", str(profile["toi"]), "--wn", str(profile["wn"]),
                   "--itow", str(profile["itow"]), "--rate", "1023000", "--output", str(self._iq_path)]
        self._run_command(command)
        return profile

    def _uplink_ready(self, profile: dict[str, int]) -> None:
        self._set_state("g1", "READY", f"PRN {profile['prn']} spreading identity assigned")
        self._set_state("g2", "PROTECTED", "SB1 BCH · SB2-SB4 CRC-24Q + LDPC · 60 x 98 interleaver")
        self._set_state("g3", "ASSEMBLED", "Sync pattern + 4 subframes · 6,000 symbols · 12 seconds")
        self._set_state("g4", "UPLINK READY", f"BPSK AFS-I/AFS-Q · {self._iq_path.stat().st_size / (1024 * 1024):.1f} MB IQ32")
        self._set_state("link", "READY", "Encoded navigation signal held at Earth station")
        self._set_telemetry("The uplink is protected, framed, modulated, and ready for orbital transmission. No raw bitstreams are exposed to the operator.")
        self._mission_badge.configure(text="UPLINK READY")

    def _transmit(self) -> None:
        if not self._iq_path.exists():
            self._fail("Prepare the G2-G4 uplink before sending it to the satellite.")
            return
        self._set_state("link", "TRANSMITTING", "Earth station is sending the 12-second navigation frame to orbit")
        self._draw_scene()
        self.after(850, self._transmit_complete)

    def _transmit_complete(self) -> None:
        self._set_state("link", "DELIVERED", "Orbital node has received the protected signal")
        self._set_state("g5", "SIGNAL RECEIVED", "Receiver is ready to acquire and decode")
        self._set_telemetry("Uplink delivery complete. The orbital receiver has a signal lock opportunity.")
        self._mission_badge.configure(text="SIGNAL IN ORBIT")
        self._draw_scene()

    def _receive(self) -> None:
        if not self._iq_path.exists():
            self._fail("No signal is in orbit. Prepare and send an uplink first.")
            return
        try:
            profile = self._read_profile()
        except ValueError as exc:
            self._fail(str(exc))
            return
        self._run_background("Acquiring orbital signal and decoding navigation message", lambda: self._decode(profile), self._decode_ready)

    def _decode(self, profile: dict[str, int]) -> dict[str, Any]:
        command = [str(self._goon), "decode", "--input", str(self._iq_path), "--input-format", "raw",
                   "--prn", str(profile["prn"]), "--rate", "1023000", "--output", str(self._decode_path)]
        self._run_command(command)
        return json.loads(self._decode_path.read_text(encoding="utf-8"))

    def _decode_ready(self, data: dict[str, Any]) -> None:
        self._data = data
        subframes = data["subframes"]
        sb1, sb2, sb3, sb4 = subframes["sb1"], subframes["sb2"], subframes["sb3"], subframes["sb4"]
        self._set_state("g5", "DECODED", f"Frame lock confirmed · BCH/LDPC/CRC accepted · {data['decode_ms']:.0f} ms")
        self._set_state("g6", "PARSED", f"SB1-SB4 decoded · FID {sb1['fid']} · TOI {sb1['toi']}")
        self._set_navigation(
            f"Frame identity  FID {sb1['fid']}  |  Time of interval  TOI {sb1['toi']}\n"
            f"LunaNet week {sb2['wn']}  |  Interval time of week {sb2['itow']}\n"
            f"Broadcast service  SB3 type {sb3['type']}  |  Network access  SB4 type {sb4['type']}\n"
            "Signal integrity verified across the protected navigation frame."
        )
        self._set_telemetry("Gateway 5 acquired the Earth signal and passed all FEC/CRC checks. Gateway 6 has labeled and parsed all four subframes.")
        self._mission_badge.configure(text="NAVIGATION AVAILABLE")

    def _run_command(self, command: list[str]) -> None:
        if not self._goon.exists():
            raise RuntimeError("Local goon executable not found. Run cmake --build build first.")
        completed = subprocess.run(command, cwd=self._repo_root, capture_output=True, text=True, check=False)
        if completed.returncode != 0:
            detail = completed.stderr.strip() or completed.stdout.strip() or "unknown CLI error"
            raise RuntimeError(detail)

    def _run_background(self, label: str, work: Callable[[], Any], success: Callable[[Any], None]) -> None:
        if self._busy:
            return
        self._busy = True
        self._set_buttons(False)
        self._set_telemetry(label + "...")

        def worker() -> None:
            try:
                result = work()
            except Exception as exc:
                self.after(0, lambda: self._background_failed(str(exc)))
                return
            self.after(0, lambda: self._background_succeeded(result, success))

        threading.Thread(target=worker, daemon=True).start()

    def _background_succeeded(self, result: Any, success: Callable[[Any], None]) -> None:
        self._busy = False
        self._set_buttons(True)
        success(result)

    def _background_failed(self, message: str) -> None:
        self._busy = False
        self._set_buttons(True)
        self._fail(message)

    def _set_buttons(self, enabled: bool) -> None:
        for button in self._buttons:
            button.state(["!disabled"] if enabled else ["disabled"])

    def _set_state(self, key: str, status: str, detail: str) -> None:
        state = self._states[key]
        state.status, state.detail = status, detail
        self._refresh_stages()

    def _refresh_stages(self) -> None:
        for key, state in self._states.items():
            label = self._stage_labels.get(key)
            detail = self._stage_details.get(key)
            if label:
                color = self._c["ACCENT_GREEN"] if state.status not in ("STANDBY", "WAITING") else self._c["FG_MUTED"]
                label.configure(text=state.status, foreground=color)
            if detail:
                detail.configure(text=state.detail)

    def _set_telemetry(self, message: str) -> None:
        self._telemetry.configure(state=tk.NORMAL)
        self._telemetry.delete("1.0", tk.END)
        self._telemetry.insert(tk.END, message)
        self._telemetry.configure(state=tk.DISABLED)

    def _set_navigation(self, message: str) -> None:
        self._navigation.configure(state=tk.NORMAL)
        self._navigation.delete("1.0", tk.END)
        self._navigation.insert(tk.END, message)
        self._navigation.configure(state=tk.DISABLED)

    def _fail(self, message: str) -> None:
        self._set_telemetry("Operation needs attention: " + message)
        self._mission_badge.configure(text="OPERATOR ACTION REQUIRED")
