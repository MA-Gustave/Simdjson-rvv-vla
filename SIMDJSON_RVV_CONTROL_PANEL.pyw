# -*- coding: utf-8 -*-
from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

APP_NAME = "simdjson RVV-VLA Control Panel"
DEFAULT_REPO = Path(r"C:\mycode\Simdjson\simdjson-rvv-vla")
DEFAULT_DIAG = Path(r"C:\mycode\Simdjson\RepoDiagSimdjson")
VLENS_ALL = "128,256,512,1024"


def python_console_exe() -> str:
    exe = Path(sys.executable)
    if exe.name.lower() == "pythonw.exe":
        candidate = exe.with_name("python.exe")
        if candidate.exists():
            return str(candidate)
    return str(exe)


def config_path() -> Path:
    root = Path(os.environ.get("LOCALAPPDATA", str(Path.home())))
    p = root / "SimdjsonRVV"
    p.mkdir(parents=True, exist_ok=True)
    return p / "control_panel.json"


def creation_flags() -> int:
    return getattr(subprocess, "CREATE_NO_WINDOW", 0) if os.name == "nt" else 0


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_NAME)
        self.geometry("1180x820")
        self.minsize(980, 700)

        self.proc = None
        self.events = queue.Queue()
        self.repo_var = tk.StringVar()
        self.diag_var = tk.StringVar()
        self.jobs_var = tk.StringVar(value="8")
        self.vlens_var = tk.StringVar(value=VLENS_ALL)
        self.command_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="Prêt.")

        self._load_settings()
        self._build()
        self.after(80, self._drain_events)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _default_repo(self) -> Path:
        here = Path(__file__).resolve().parent
        if (here / "scripts" / "rvv" / "run_tests.py").exists() and (here / "CMakeLists.txt").exists():
            return here
        return DEFAULT_REPO

    def _load_settings(self) -> None:
        repo = self._default_repo()
        diag = DEFAULT_DIAG
        jobs = "8"
        vlens = VLENS_ALL
        try:
            d = json.loads(config_path().read_text(encoding="utf-8"))
            repo = Path(d.get("repo_root") or repo)
            diag = Path(d.get("repodiag_root") or diag)
            jobs = str(d.get("jobs", jobs))
            vlens = str(d.get("vlens", vlens))
        except Exception:
            pass
        self.repo_var.set(str(repo))
        self.diag_var.set(str(diag))
        self.jobs_var.set(jobs)
        self.vlens_var.set(vlens)

    def _save_settings(self) -> None:
        d = {
            "repo_root": self.repo_var.get().strip(),
            "repodiag_root": self.diag_var.get().strip(),
            "jobs": self.jobs_var.get().strip(),
            "vlens": self.vlens_var.get().strip(),
        }
        try:
            config_path().write_text(json.dumps(d, indent=2), encoding="utf-8")
        except Exception:
            pass

    def _build(self) -> None:
        outer = ttk.Frame(self, padding=12)
        outer.pack(fill="both", expand=True)

        ttk.Label(outer, text="simdjson RVV-VLA", font=("Segoe UI", 16, "bold")).pack(anchor="w")
        ttk.Label(
            outer,
            text="Control panel pour figer et répéter les commandes de build/validation.",
        ).pack(anchor="w", pady=(0, 10))

        paths = ttk.LabelFrame(outer, text="Workspace", padding=10)
        paths.pack(fill="x")
        self._path_row(paths, 0, "Repo simdjson", self.repo_var)
        self._path_row(paths, 1, "RepoDiag", self.diag_var)

        opts = ttk.Frame(paths)
        opts.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(8, 0))
        ttk.Label(opts, text="Jobs").pack(side="left")
        ttk.Spinbox(opts, from_=1, to=128, width=6, textvariable=self.jobs_var).pack(side="left", padx=(6, 18))
        ttk.Label(opts, text="VLEN").pack(side="left")
        ttk.Entry(opts, width=24, textvariable=self.vlens_var).pack(side="left", padx=(6, 18))
        ttk.Button(opts, text="Ouvrir repo", command=lambda: self._open(self.repo_var.get())).pack(side="right")
        ttk.Button(opts, text="Ouvrir .repodiag", command=self.open_repodiag_output).pack(side="right", padx=(0, 6))

        quick = ttk.LabelFrame(outer, text="1 — Préflight", padding=10)
        quick.pack(fill="x", pady=(10, 0))
        self._button_grid(quick, [
            ("Git status", self.git_status),
            ("Git diff", self.git_diff),
            ("Derniers commits", self.git_log),
            ("WSL + outils", self.check_tools),
            ("RepoDiag doctor", lambda: self.run_repodiag("doctor")),
            ("RVV source contract", lambda: self.run_repodiag("rvv-static")),
        ], 6)

        direct = ttk.LabelFrame(outer, text="2 — Tests RVV directs", padding=10)
        direct.pack(fill="x", pady=(10, 0))
        self._button_grid(direct, [
            ("GCC global VLEN 128", lambda: self.run_rvv("gcc", "global", "rvv", "128")),
            ("GCC global matrice", lambda: self.run_rvv("gcc", "global", "rvv", self.vlens_var.get())),
            ("GCC dispatch matrice", lambda: self.run_rvv("gcc", "dispatch", "rvv", self.vlens_var.get())),
            ("Clang global matrice", lambda: self.run_rvv("clang", "global", "rvv", self.vlens_var.get())),
            ("Acceptance GCC", lambda: self.run_rvv("gcc", "global", "acceptance", self.vlens_var.get())),
            ("Tout GCC", lambda: self.run_rvv("gcc", "global", "all", self.vlens_var.get())),
        ], 3)

        diag = ttk.LabelFrame(outer, text="3 — Campagnes RepoDiag", padding=10)
        diag.pack(fill="x", pady=(10, 0))
        self._button_grid(diag, [
            ("RVV required", lambda: self.run_repodiag("rvv")),
            ("RVV full", lambda: self.run_repodiag("rvv-full")),
            ("Deep", lambda: self.run_repodiag("deep")),
            ("Baseline", lambda: self.run_repodiag("baseline")),
            ("Standard", lambda: self.run_repodiag("standard")),
        ], 5)

        misc = ttk.LabelFrame(outer, text="4 — Maintenance", padding=10)
        misc.pack(fill="x", pady=(10, 0))
        self._button_grid(misc, [
            ("Nettoyer builds RVV", self.clean_builds),
            ("Ouvrir builds", self.open_builds),
            ("Ouvrir spec", self.open_spec),
            ("Ouvrir tests RVV", self.open_tests),
        ], 4)

        fixed = ttk.LabelFrame(outer, text="Commande figée sélectionnée", padding=8)
        fixed.pack(fill="x", pady=(10, 0))
        ttk.Entry(fixed, textvariable=self.command_var, state="readonly").pack(fill="x")

        ctl = ttk.Frame(outer)
        ctl.pack(fill="x", pady=(10, 6))
        self.stop_btn = ttk.Button(ctl, text="Arrêter", command=self.stop, state="disabled")
        self.stop_btn.pack(side="left")
        ttk.Button(ctl, text="Effacer log", command=self.clear_log).pack(side="left", padx=(6, 0))
        ttk.Button(ctl, text="Copier commande", command=self.copy_command).pack(side="left", padx=(6, 0))
        ttk.Label(ctl, textvariable=self.status_var).pack(side="right")

        log_frame = ttk.Frame(outer)
        log_frame.pack(fill="both", expand=True)
        self.log = tk.Text(log_frame, wrap="none", font=("Consolas", 10), state="disabled")
        y = ttk.Scrollbar(log_frame, orient="vertical", command=self.log.yview)
        x = ttk.Scrollbar(log_frame, orient="horizontal", command=self.log.xview)
        self.log.configure(yscrollcommand=y.set, xscrollcommand=x.set)
        self.log.grid(row=0, column=0, sticky="nsew")
        y.grid(row=0, column=1, sticky="ns")
        x.grid(row=1, column=0, sticky="ew")
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)

        self._write(
            "Control panel prêt.\n"
            "Ordre conseillé: WSL + outils -> RVV source contract -> GCC global 128 -> matrices.\n"
        )

    def _path_row(self, parent, row, label, var) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=3)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky="ew", pady=3)
        ttk.Button(parent, text="…", width=3, command=lambda: self._choose_dir(var)).grid(row=row, column=2, padx=(8, 0))
        parent.columnconfigure(1, weight=1)

    def _button_grid(self, parent, items, columns) -> None:
        for i, (label, fn) in enumerate(items):
            ttk.Button(parent, text=label, command=fn).grid(
                row=i // columns, column=i % columns, sticky="ew", padx=4, pady=4
            )
        for c in range(columns):
            parent.columnconfigure(c, weight=1)

    def _choose_dir(self, var: tk.StringVar) -> None:
        p = Path(var.get())
        initial = p if p.exists() else p.parent
        value = filedialog.askdirectory(initialdir=str(initial))
        if value:
            var.set(value)
            self._save_settings()

    def _open(self, value: str) -> None:
        p = Path(value)
        if p.exists() and os.name == "nt":
            os.startfile(p)
        else:
            messagebox.showwarning(APP_NAME, f"Introuvable: {p}")

    def open_builds(self) -> None:
        self._open(str(Path(self.repo_var.get()) / ".repodiag" / "builds"))

    def open_repodiag_output(self) -> None:
        self._open(str(Path(self.repo_var.get()) / ".repodiag"))

    def open_spec(self) -> None:
        self._open(str(Path(self.repo_var.get()) / "extra" / "rvv-vla" / "spec"))

    def open_tests(self) -> None:
        self._open(str(Path(self.repo_var.get()) / "tests" / "rvv"))

    def _write(self, text: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state="disabled")

    def clear_log(self) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def copy_command(self) -> None:
        value = self.command_var.get()
        if value:
            self.clipboard_clear()
            self.clipboard_append(value)
            self.status_var.set("Commande copiée")

    def _repo(self) -> Path:
        p = Path(self.repo_var.get().strip())
        if not p.is_dir():
            raise FileNotFoundError(f"Repo simdjson introuvable: {p}")
        return p

    def _diag(self) -> Path:
        p = Path(self.diag_var.get().strip())
        if not (p / "repodiag.py").is_file():
            raise FileNotFoundError(f"RepoDiag introuvable: {p}")
        return p

    def _run(self, cmd, cwd: Path) -> None:
        if self.proc is not None:
            messagebox.showwarning(APP_NAME, "Une commande est déjà en cours.")
            return
        self.command_var.set(subprocess.list2cmdline(cmd))
        self._save_settings()
        self.status_var.set("En cours…")
        self.stop_btn.configure(state="normal")
        self._write("\n> " + subprocess.list2cmdline(cmd) + "\n")
        threading.Thread(target=self._worker, args=(cmd, cwd), daemon=True).start()

    def _worker(self, cmd, cwd: Path) -> None:
        try:
            self.proc = subprocess.Popen(
                cmd,
                cwd=str(cwd),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
                creationflags=creation_flags(),
            )
            for line in self.proc.stdout:
                self.events.put(("line", line))
            rc = self.proc.wait()
            self.events.put(("done", rc))
        except Exception as exc:
            self.events.put(("error", exc))
        finally:
            self.proc = None

    def _drain_events(self) -> None:
        try:
            while True:
                kind, data = self.events.get_nowait()
                if kind == "line":
                    self._write(str(data))
                elif kind == "done":
                    rc = int(data)
                    self._write(f"\n[exit {rc}]\n")
                    self.status_var.set("PASS" if rc == 0 else f"Échec ({rc})")
                    self.stop_btn.configure(state="disabled")
                elif kind == "error":
                    self._write(f"\nERREUR: {data}\n")
                    self.status_var.set("Erreur")
                    self.stop_btn.configure(state="disabled")
        except queue.Empty:
            pass
        self.after(80, self._drain_events)

    def stop(self) -> None:
        p = self.proc
        if p is None:
            return
        try:
            if os.name == "nt":
                subprocess.run(
                    ["taskkill", "/PID", str(p.pid), "/T", "/F"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    creationflags=creation_flags(),
                )
            else:
                p.terminate()
            self._write("\n[commande arrêtée]\n")
        except Exception as exc:
            self._write(f"\n[échec arrêt: {exc}]\n")

    def git_status(self) -> None:
        repo = self._repo()
        self._run(["git", "-C", str(repo), "status", "--short", "--branch"], repo)

    def git_diff(self) -> None:
        repo = self._repo()
        self._run(["git", "-C", str(repo), "diff", "--stat"], repo)

    def git_log(self) -> None:
        repo = self._repo()
        self._run(["git", "-C", str(repo), "log", "-5", "--oneline", "--decorate"], repo)

    def check_tools(self) -> None:
        repo = self._repo()
        script = (
            "$ErrorActionPreference='Continue'; "
            "Write-Host '=== WSL ==='; wsl.exe --status; "
            "Write-Host ''; Write-Host '=== Toolchain WSL ==='; "
            "wsl.exe bash -lc \""
            "python3 --version; "
            "cmake --version | head -1; "
            "ctest --version | head -1; "
            "qemu-riscv64 --version | head -1; "
            "riscv64-linux-gnu-g++ --version | head -1; "
            "clang++ --version | head -1"
            "\""
        )
        self._run(["pwsh", "-NoProfile", "-Command", script], repo)

    def run_repodiag(self, selection: str) -> None:
        try:
            repo = self._repo()
            diag = self._diag()
            cmd = [python_console_exe(), str(diag / "repodiag.py"), "--target", str(repo)]
            if selection == "doctor":
                cmd.append("doctor")
            else:
                cmd += ["run", selection, "--jobs", self.jobs_var.get().strip() or "4"]
            self._run(cmd, diag)
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))

    def run_rvv(self, compiler: str, profile: str, suite: str, vlens: str) -> None:
        try:
            repo = self._repo()
            runner = repo / "scripts" / "rvv" / "run_tests.py"
            if not runner.is_file():
                raise FileNotFoundError(f"Runner introuvable: {runner}")
            cmd = [
                python_console_exe(), str(runner),
                "--compiler", compiler,
                "--profile", profile,
                "--suite", suite,
                "--vlens", vlens.strip() or VLENS_ALL,
                "--jobs", self.jobs_var.get().strip() or "8",
            ]
            self._run(cmd, repo)
        except Exception as exc:
            messagebox.showerror(APP_NAME, str(exc))

    def clean_builds(self) -> None:
        if self.proc is not None:
            messagebox.showwarning(APP_NAME, "Arrête la commande en cours avant de nettoyer.")
            return
        repo = self._repo()
        path = repo / ".repodiag" / "builds"
        if not path.exists():
            messagebox.showinfo(APP_NAME, "Aucun build RVV à nettoyer.")
            return
        if not messagebox.askyesno(APP_NAME, f"Supprimer entièrement?\n\n{path}"):
            return
        escaped = str(path).replace("'", "''")
        self._run(
            ["pwsh", "-NoProfile", "-Command", f"Remove-Item -LiteralPath '{escaped}' -Recurse -Force"],
            repo,
        )

    def _on_close(self) -> None:
        self._save_settings()
        if self.proc is not None:
            if not messagebox.askyesno(APP_NAME, "Une commande est en cours. Quitter quand même?"):
                return
            self.stop()
        self.destroy()


if __name__ == "__main__":
    App().mainloop()
