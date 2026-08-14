"""LunaNet — Test Report Viewer

Displays markdown summary and fine-grained JUnit XML results
with color-coded pass/fail highlighting. Covers all gateways.

Usage:
    python codes/gateway1/gui/report_viewer.py
"""

from __future__ import annotations

import json
import re
import tkinter as tk
from tkinter import ttk
from pathlib import Path

from report_parser import ReportData, parse_junit_xml, load_markdown
from mission_console import MissionConsole
from signal_panel import SignalGenerationPanel

# ── Colour Palette (loaded from gui/config/theme.json) ───────────────────────

_THEME_PATH = Path(__file__).resolve().parent / "config" / "theme.json"

_VSCODE_TO_PALETTE = {
    "BG_DARK":       "editor.background",
    "FG_PRIMARY":    "editorCursor.foreground",
    "FG_SECONDARY":  "editor.foreground",
    "FG_MUTED":      "foreground",
    "ACCENT":        "terminal.ansiBlue",
    "ACCENT_GREEN":  "terminal.ansiGreen",
    "ACCENT_RED":    "terminal.ansiRed",
    "ACCENT_YELLOW": "terminal.ansiYellow",
    "BG_SURFACE":    "list.activeSelectionBackground",
    "BG_ELEVATED":   "diffEditor.diagonalFill",
    "BORDER_CLR":    "editorIndentGuide.activeBackground1",
    "HEADER_BG":     "diffEditor.diagonalFill",
}

_DEFAULTS = {
    "BG_DARK": "#1a1b26", "BG_SURFACE": "#24283b", "BG_ELEVATED": "#292e42",
    "FG_PRIMARY": "#c0caf5", "FG_SECONDARY": "#9aa5ce", "FG_MUTED": "#565f89",
    "ACCENT": "#7aa2f7", "ACCENT_GREEN": "#9ece6a", "ACCENT_RED": "#f7768e",
    "ACCENT_YELLOW": "#e0af68", "PASS_ROW_BG": "#1e3a1e", "FAIL_ROW_BG": "#3a1e1e",
    "SKIP_ROW_BG": "#3a351e", "BORDER_CLR": "#414868", "HEADER_BG": "#292e42",
}


def _blend_hex(base: str, tint: str, alpha: float = 0.15) -> str:
    """Blend *tint* over *base* at the given alpha to derive row backgrounds."""
    br, bg, bb = int(base[1:3], 16), int(base[3:5], 16), int(base[5:7], 16)
    tr, tg, tb = int(tint[1:3], 16), int(tint[3:5], 16), int(tint[5:7], 16)
    r = int(br * (1 - alpha) + tr * alpha)
    g = int(bg * (1 - alpha) + tg * alpha)
    b = int(bb * (1 - alpha) + tb * alpha)
    return f"#{r:02x}{g:02x}{b:02x}"


def _load_theme() -> dict[str, str]:
    """Load colour palette from theme.json, falling back to defaults."""
    palette = dict(_DEFAULTS)

    try:
        with open(_THEME_PATH, encoding="utf-8") as fh:
            theme = json.load(fh)
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return palette

    colors = theme.get("colors", {})

    for palette_key, vscode_key in _VSCODE_TO_PALETTE.items():
        raw = colors.get(vscode_key)
        if raw and raw.startswith("#"):
            palette[palette_key] = raw[:7]

    palette["PASS_ROW_BG"] = _blend_hex(palette["BG_DARK"], palette["ACCENT_GREEN"])
    palette["FAIL_ROW_BG"] = _blend_hex(palette["BG_DARK"], palette["ACCENT_RED"])
    palette["SKIP_ROW_BG"] = _blend_hex(palette["BG_DARK"], palette["ACCENT_YELLOW"])

    return palette


_T = _load_theme()
BG_DARK      = _T["BG_DARK"]
BG_SURFACE   = _T["BG_SURFACE"]
BG_ELEVATED  = _T["BG_ELEVATED"]
FG_PRIMARY   = _T["FG_PRIMARY"]
FG_SECONDARY = _T["FG_SECONDARY"]
FG_MUTED     = _T["FG_MUTED"]
ACCENT       = _T["ACCENT"]
ACCENT_GREEN = _T["ACCENT_GREEN"]
ACCENT_RED   = _T["ACCENT_RED"]
ACCENT_YELLOW = _T["ACCENT_YELLOW"]
PASS_ROW_BG  = _T["PASS_ROW_BG"]
FAIL_ROW_BG  = _T["FAIL_ROW_BG"]
SKIP_ROW_BG  = _T["SKIP_ROW_BG"]
BORDER_CLR   = _T["BORDER_CLR"]
HEADER_BG    = _T["HEADER_BG"]


def _find_reports_dir() -> Path:
    """Locate Validation/reports/ relative to this script."""
    return Path(__file__).resolve().parents[3] / "Validation" / "reports"


def _discover_reports(base: Path) -> list[dict]:
    """Scan for report pairs grouped by date/time, newest first."""
    reports: list[dict] = []
    if not base.is_dir():
        return reports

    for date_dir in sorted(base.iterdir(), reverse=True):
        if not date_dir.is_dir() or not re.match(r"\d{4}-\d{2}-\d{2}", date_dir.name):
            continue
        for xml_file in sorted(date_dir.glob("*.xml"), reverse=True):
            md_file = xml_file.with_suffix(".md")
            reports.append(
                {
                    "label": f"{date_dir.name}  ·  {xml_file.stem}",
                    "xml": xml_file,
                    "md": md_file if md_file.exists() else None,
                }
            )
    return reports


class ReportViewer(tk.Tk):
    """Main application window."""

    def __init__(self) -> None:
        super().__init__()
        self.title("LunaNet: Test Report")
        self.geometry("1200x820")
        self.minsize(960, 640)
        self.configure(bg=BG_DARK)

        self._reports: list[dict] = []
        self._current: ReportData | None = None

        self._setup_styles()
        self._build_header()

        # ── Tabbed interface: Operations, reports, and signal generation ──
        notebook = ttk.Notebook(self)
        notebook.pack(fill=tk.BOTH, expand=True, padx=12, pady=(4, 4))

        mission_tab = MissionConsole(
            notebook, palette=_T, repo_root=Path(__file__).resolve().parents[3])
        notebook.add(mission_tab, text="  Mission Console  ")

        reports_tab = ttk.Frame(notebook, style="Dark.TFrame")
        notebook.add(reports_tab, text="  Test Reports  ")
        self._build_reports_tab(reports_tab)

        signal_tab = SignalGenerationPanel(
            notebook, palette=_T, repo_root=Path(__file__).resolve().parents[3])
        notebook.add(signal_tab, text="  Signal Generation  ")

        self._build_status_bar()
        self._refresh_reports()

    # ── Reports Tab ───────────────────────────────────────────────────────

    def _build_reports_tab(self, parent: tk.Misc) -> None:
        self._build_selector(parent)

        # Resizable vertical split: summary over details.
        paned = tk.PanedWindow(
            parent, orient=tk.VERTICAL, bg=BG_DARK,
            sashwidth=6, sashrelief=tk.FLAT, bd=0,
        )
        paned.pack(fill=tk.BOTH, expand=True, pady=(0, 4))

        self._build_summary_panel(paned)
        self._build_details_panel(paned)

    # ── Style Configuration ───────────────────────────────────────────────

    def _setup_styles(self) -> None:
        s = ttk.Style(self)
        s.theme_use("clam")

        s.configure(".", background=BG_DARK, foreground=FG_PRIMARY,
                     fieldbackground=BG_SURFACE, bordercolor=BORDER_CLR,
                     troughcolor=BG_SURFACE, selectbackground=ACCENT,
                     font=("Segoe UI", 10))

        s.configure("Header.TLabel", background=BG_DARK, foreground=ACCENT,
                     font=("Segoe UI Semibold", 16))
        s.configure("SubHeader.TLabel", background=BG_DARK, foreground=FG_SECONDARY,
                     font=("Segoe UI", 10))
        s.configure("Status.TLabel", background=BG_ELEVATED, foreground=FG_SECONDARY,
                     font=("Segoe UI", 9), padding=(8, 4))
        s.configure("Section.TLabel", background=BG_DARK, foreground=FG_MUTED,
                     font=("Segoe UI Semibold", 9))
        s.configure("PassBadge.TLabel", background="#1e3a1e", foreground=ACCENT_GREEN,
                     font=("Segoe UI Semibold", 13), padding=(12, 4))
        s.configure("FailBadge.TLabel", background="#3a1e1e", foreground=ACCENT_RED,
                     font=("Segoe UI Semibold", 13), padding=(12, 4))

        s.configure("TCombobox", fieldbackground=BG_SURFACE,
                     background=BG_ELEVATED, foreground=FG_PRIMARY,
                     selectbackground=BG_ELEVATED, selectforeground=FG_PRIMARY,
                     arrowcolor=FG_SECONDARY)
        s.map("TCombobox",
              fieldbackground=[("readonly", BG_SURFACE), ("focus", BG_SURFACE)],
              foreground=[("readonly", FG_PRIMARY)],
              selectbackground=[("readonly", BG_SURFACE)],
              selectforeground=[("readonly", FG_PRIMARY)])

        # Force dark colors on the Combobox popup Listbox (Tk, not ttk)
        self.option_add("*TCombobox*Listbox.background", BG_SURFACE)
        self.option_add("*TCombobox*Listbox.foreground", FG_PRIMARY)
        self.option_add("*TCombobox*Listbox.selectBackground", ACCENT)
        self.option_add("*TCombobox*Listbox.selectForeground", BG_DARK)

        s.configure("Accent.TButton", background=ACCENT, foreground="#1a1b26",
                     font=("Segoe UI Semibold", 9), padding=(10, 4))
        s.map("Accent.TButton",
              background=[("active", "#5d8bdb"), ("pressed", "#4a72b8")])

        # Treeview
        s.configure("Summary.Treeview", background=BG_SURFACE,
                     foreground=FG_PRIMARY, fieldbackground=BG_SURFACE,
                     rowheight=28, font=("Segoe UI", 10))
        s.configure("Summary.Treeview.Heading", background=HEADER_BG,
                     foreground=ACCENT, font=("Segoe UI Semibold", 10),
                     relief="flat")
        s.map("Summary.Treeview", background=[("selected", BG_ELEVATED)])

        s.configure("Detail.Treeview", background=BG_SURFACE,
                     foreground=FG_PRIMARY, fieldbackground=BG_SURFACE,
                     rowheight=24, font=("Consolas", 9))
        s.configure("Detail.Treeview.Heading", background=HEADER_BG,
                     foreground=ACCENT, font=("Segoe UI Semibold", 9),
                     relief="flat")
        s.map("Detail.Treeview", background=[("selected", BG_ELEVATED)])

        s.configure("Dark.TFrame", background=BG_DARK)

        # Notebook (tab strip) — dark theme.
        s.configure("TNotebook", background=BG_DARK, borderwidth=0, tabmargins=(2, 6, 2, 0))
        s.configure("TNotebook.Tab", background=BG_SURFACE, foreground=FG_SECONDARY,
                     padding=(18, 7), font=("Segoe UI Semibold", 10), borderwidth=0)
        s.map("TNotebook.Tab",
              background=[("selected", BG_ELEVATED)],
              foreground=[("selected", ACCENT)])

    # ── Header ────────────────────────────────────────────────────────────

    def _build_header(self) -> None:
        frame = ttk.Frame(self, style="Dark.TFrame")
        frame.pack(fill=tk.X, padx=12, pady=(10, 2))

        ttk.Label(
            frame, text="LunaNet: Test Report",
            style="Header.TLabel",
        ).pack(side=tk.LEFT)

        self._badge = ttk.Label(frame, text="No report loaded", style="SubHeader.TLabel")
        self._badge.pack(side=tk.RIGHT)

    # ── Report Selector ───────────────────────────────────────────────────

    def _build_selector(self, parent: tk.Misc) -> None:
        frame = ttk.Frame(parent, style="Dark.TFrame")
        frame.pack(fill=tk.X, pady=(8, 6))

        ttk.Label(frame, text="REPORT", style="Section.TLabel").pack(side=tk.LEFT, padx=(0, 8))

        self._combo_var = tk.StringVar()
        self._combo = ttk.Combobox(
            frame, textvariable=self._combo_var,
            state="readonly", width=40,
        )
        self._combo.pack(side=tk.LEFT, padx=(0, 8))
        self._combo.bind("<<ComboboxSelected>>", lambda _: self._on_report_selected())

        ttk.Button(
            frame, text="↻  Refresh", style="Accent.TButton",
            command=self._refresh_reports,
        ).pack(side=tk.LEFT)

    # ── Summary Panel ─────────────────────────────────────────────────────

    def _build_summary_panel(self, parent: tk.PanedWindow) -> None:
        container = ttk.Frame(parent, style="Dark.TFrame")
        parent.add(container, minsize=180)

        header_frame = ttk.Frame(container, style="Dark.TFrame")
        header_frame.pack(fill=tk.X, pady=(4, 4))

        ttk.Label(header_frame, text="SUMMARY", style="Section.TLabel").pack(side=tk.LEFT)

        self._overall_label = ttk.Label(header_frame, text="", style="SubHeader.TLabel")
        self._overall_label.pack(side=tk.RIGHT)

        cols = ("suite", "passed", "failed", "skipped", "total", "time")
        self._summary_tree = ttk.Treeview(
            container, columns=cols, show="headings",
            style="Summary.Treeview", height=7,
        )

        self._summary_tree.heading("suite", text="Suite", anchor=tk.W)
        self._summary_tree.heading("passed", text="Passed")
        self._summary_tree.heading("failed", text="Failed")
        self._summary_tree.heading("skipped", text="Skipped")
        self._summary_tree.heading("total", text="Total")
        self._summary_tree.heading("time", text="Time (ms)")

        self._summary_tree.column("suite", width=220, anchor=tk.W)
        self._summary_tree.column("passed", width=80, anchor=tk.CENTER)
        self._summary_tree.column("failed", width=80, anchor=tk.CENTER)
        self._summary_tree.column("skipped", width=80, anchor=tk.CENTER)
        self._summary_tree.column("total", width=80, anchor=tk.CENTER)
        self._summary_tree.column("time", width=100, anchor=tk.E)

        self._summary_tree.tag_configure("pass_row", background=PASS_ROW_BG, foreground=ACCENT_GREEN)
        self._summary_tree.tag_configure("fail_row", background=FAIL_ROW_BG, foreground=ACCENT_RED)
        self._summary_tree.tag_configure("totals", background=BG_ELEVATED, foreground=ACCENT,
                                          font=("Segoe UI Semibold", 10))

        self._summary_tree.pack(fill=tk.BOTH, expand=True)

    # ── Details Panel ─────────────────────────────────────────────────────

    def _build_details_panel(self, parent: tk.PanedWindow) -> None:
        container = ttk.Frame(parent, style="Dark.TFrame")
        parent.add(container, minsize=250)

        header_frame = ttk.Frame(container, style="Dark.TFrame")
        header_frame.pack(fill=tk.X, pady=(4, 4))

        ttk.Label(header_frame, text="DETAILED RESULTS", style="Section.TLabel").pack(side=tk.LEFT)

        ttk.Label(header_frame, text="Suite:", style="SubHeader.TLabel").pack(side=tk.LEFT, padx=(20, 4))
        self._filter_var = tk.StringVar(value="All")
        self._filter_combo = ttk.Combobox(
            header_frame, textvariable=self._filter_var,
            state="readonly", width=25,
        )
        self._filter_combo.pack(side=tk.LEFT)
        self._filter_combo.bind("<<ComboboxSelected>>", lambda _: self._apply_filter())

        ttk.Label(header_frame, text="Status:", style="SubHeader.TLabel").pack(side=tk.LEFT, padx=(12, 4))
        self._status_filter_var = tk.StringVar(value="All")
        self._status_filter = ttk.Combobox(
            header_frame, textvariable=self._status_filter_var,
            state="readonly", width=10, values=["All", "PASS", "FAIL", "SKIP"],
        )
        self._status_filter.pack(side=tk.LEFT)
        self._status_filter.bind("<<ComboboxSelected>>", lambda _: self._apply_filter())

        # Treeview with scrollbar
        tree_frame = ttk.Frame(container, style="Dark.TFrame")
        tree_frame.pack(fill=tk.BOTH, expand=True)

        cols = ("status", "suite", "name", "time", "detail")
        self._detail_tree = ttk.Treeview(
            tree_frame, columns=cols, show="headings",
            style="Detail.Treeview",
        )

        self._detail_tree.heading("status", text="Status")
        self._detail_tree.heading("suite", text="Suite", anchor=tk.W)
        self._detail_tree.heading("name", text="Test Name", anchor=tk.W)
        self._detail_tree.heading("time", text="Time (s)")
        self._detail_tree.heading("detail", text="Detail", anchor=tk.W)

        self._detail_tree.column("status", width=60, anchor=tk.CENTER)
        self._detail_tree.column("suite", width=180, anchor=tk.W)
        self._detail_tree.column("name", width=280, anchor=tk.W)
        self._detail_tree.column("time", width=80, anchor=tk.E)
        self._detail_tree.column("detail", width=300, anchor=tk.W)

        self._detail_tree.tag_configure("pass", background=PASS_ROW_BG, foreground=ACCENT_GREEN)
        self._detail_tree.tag_configure("fail", background=FAIL_ROW_BG, foreground=ACCENT_RED)
        self._detail_tree.tag_configure("skip", background=SKIP_ROW_BG, foreground=ACCENT_YELLOW)

        scrollbar = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self._detail_tree.yview)
        self._detail_tree.configure(yscrollcommand=scrollbar.set)

        self._detail_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

    # ── Status Bar ────────────────────────────────────────────────────────

    def _build_status_bar(self) -> None:
        self._status_label = ttk.Label(self, text="Ready", style="Status.TLabel")
        self._status_label.pack(fill=tk.X, side=tk.BOTTOM)

    # ── Report Discovery & Loading ────────────────────────────────────────

    def _refresh_reports(self) -> None:
        base = _find_reports_dir()
        self._reports = _discover_reports(base)

        labels = [r["label"] for r in self._reports]
        self._combo["values"] = labels

        if labels:
            self._combo.current(0)
            self._on_report_selected()
            self._set_status(f"Found {len(labels)} report(s) in {base}")
        else:
            self._set_status(f"No reports found in {base}")

    def _on_report_selected(self) -> None:
        idx = self._combo.current()
        if idx < 0 or idx >= len(self._reports):
            return

        entry = self._reports[idx]
        report = parse_junit_xml(entry["xml"])
        if entry["md"]:
            report.markdown = load_markdown(entry["md"])

        self._current = report
        self._populate_summary(report)
        self._populate_details(report)
        self._update_badge(report)
        self._set_status(f"Loaded: {entry['xml']}")

    # ── Summary Population ────────────────────────────────────────────────

    def _populate_summary(self, report: ReportData) -> None:
        tree = self._summary_tree
        for item in tree.get_children():
            tree.delete(item)

        for suite in report.suites:
            tag = "pass_row" if suite.failed == 0 else "fail_row"
            tree.insert("", tk.END, values=(
                suite.name,
                suite.passed,
                suite.failed,
                suite.skipped,
                suite.total,
                f"{suite.time_s * 1000:.1f}",
            ), tags=(tag,))

        tree.insert("", tk.END, values=(
            "Total",
            report.total_passed,
            report.total_failed,
            report.total_skipped,
            report.total_tests,
            f"{report.total_time_s * 1000:.1f}",
        ), tags=("totals",))

        status = "✅  PASS" if report.all_passed else "❌  FAIL"
        self._overall_label.configure(
            text=f"{status}  ({report.total_passed}/{report.total_tests})")

    # ── Details Population ────────────────────────────────────────────────

    def _populate_details(self, report: ReportData) -> None:
        # Update suite filter values
        suite_names = sorted({tc.suite for tc in report.testcases})
        self._filter_combo["values"] = ["All"] + suite_names
        self._filter_var.set("All")
        self._status_filter_var.set("All")
        self._apply_filter()

    def _apply_filter(self) -> None:
        if self._current is None:
            return

        tree = self._detail_tree
        for item in tree.get_children():
            tree.delete(item)

        suite_filter = self._filter_var.get()
        status_filter = self._status_filter_var.get()

        status_icons = {"PASS": "✅", "FAIL": "❌", "SKIP": "⏭"}

        visible = 0
        for tc in self._current.testcases:
            if suite_filter != "All" and tc.suite != suite_filter:
                continue
            if status_filter != "All" and tc.status != status_filter:
                continue

            tag = tc.status.lower()
            tree.insert("", tk.END, values=(
                status_icons.get(tc.status, "?"),
                tc.suite,
                tc.name,
                f"{tc.time_s:.4f}",
                tc.detail,
            ), tags=(tag,))
            visible += 1

        self._set_status(f"Showing {visible} of {len(self._current.testcases)} test cases")

    # ── Badge & Status ────────────────────────────────────────────────────

    def _update_badge(self, report: ReportData) -> None:
        if report.all_passed:
            self._badge.configure(text="✅  ALL TESTS PASSED", style="PassBadge.TLabel")
        else:
            self._badge.configure(
                text=f"❌  {report.total_failed} FAILURE(S)", style="FailBadge.TLabel")

    def _set_status(self, text: str) -> None:
        self._status_label.configure(text=text)


def main() -> None:
    app = ReportViewer()
    app.mainloop()


if __name__ == "__main__":
    main()
