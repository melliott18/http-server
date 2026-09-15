#!/usr/bin/env python3
"""Run the supplied Linux course workloads against a compiled server.

Each case owns a temporary document root. Reports contain logs and JUnit XML;
any failed validator, server startup, thread count, or timeout fails the run.
"""

import argparse
from contextlib import contextmanager
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

COURSE = Path(__file__).resolve().parent / "course"
SCRIPTS = COURSE / "test_scripts"


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@contextmanager
def server(binary, directory, threads, audit, output):
    port = free_port()
    args = [str(binary)]
    if threads is not None:
        args += ["-t", str(threads)]
    args += [str(port)]
    with output.open("wb") as stdout, audit.open("wb") as stderr:
        process = subprocess.Popen(args, cwd=directory, stdout=stdout, stderr=stderr)
        try:
            deadline = time.monotonic() + 5
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f"Server exited during startup: {process.returncode}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                        break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Server did not listen within 5 seconds")
                    time.sleep(0.05)
            yield process, port
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def command(args, cwd, output, timeout):
    with output.open("wb") as stream:
        result = subprocess.run(args, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"{Path(str(args[1])).name} exited {result.returncode}; see {output.name}")


def run_workload(binary, workload, destination, timeout, generated=False):
    with tempfile.TemporaryDirectory(prefix="httpserver-course-") as tmp:
        directory = Path(tmp)
        shutil.copytree(COURSE / "test_files", directory / "test_files")
        if workload.stem == "atomic_multi_put":
            for index in range(5):
                payload = (f"deterministic PUT fixture {index}\n".encode() * 10000)[:180000]
                (directory / "test_files" / f"generated-payload-{index}.txt").write_bytes(payload)
        batch = workload
        if generated:
            batch = directory / "generated.toml"
            command([sys.executable, str(SCRIPTS / "make_batch.py"), "-n", "100", "-t", "pg",
                     "test_files/ipanema.txt", "test_files/unforgettable.txt"],
                    directory, batch, timeout)
        audit = destination / "audit.log"
        oliver = destination / "requests.log"
        threads = 1 if workload.stem.startswith("audit_") else (5 if workload.stem == "atomic_multi_put" else 4)
        with server(binary, directory, threads, audit, destination / "server.log") as (process, port):
            command([sys.executable, str(SCRIPTS / "olivertwist.py"), "-o", "127.0.0.1",
                     "-d", "outs", "-p", str(port), str(batch)], directory, oliver, timeout)
            if process.poll() is not None:
                raise RuntimeError(f"Server exited while processing requests: {process.returncode}")
        command([sys.executable, str(SCRIPTS / "sherlock.py"), "--audit-log", str(audit),
                 "--oliver-log", str(oliver)], directory, destination / "ordering.log", timeout)
        command([sys.executable, str(SCRIPTS / "watson.py"), "--audit-log", str(audit),
                 "--oliver-events", str(batch), "--response-dir", "outs"],
                directory, destination / "responses.log", timeout)


def run_threads(binary, threads, destination):
    with tempfile.TemporaryDirectory(prefix="httpserver-threads-") as tmp:
        with server(binary, Path(tmp), threads, destination / "audit.log",
                    destination / "server.log") as (process, _):
            expected = (threads if threads is not None else 4) + 1
            deadline = time.monotonic() + 2
            while True:
                count = len(list(Path(f"/proc/{process.pid}/task").iterdir()))
                if count == expected:
                    break
                if time.monotonic() >= deadline:
                    raise AssertionError(f"Expected {expected} total threads, observed {count}")
                time.sleep(0.05)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=45, help="Seconds per helper process")
    parser.add_argument("--case", help="Run one case by name (for example audit_get or threads_2)")
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"Server binary does not exist: {binary}; build it first")
    if sys.platform != "linux":
        parser.error("Course workloads require Linux epoll and /proc; use the Ubuntu test container")
    destination = args.report_dir.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    cases = [(p.stem, p, None) for p in sorted((COURSE / "workloads").glob("*.toml"))]
    cases += [("generated_batch", Path("generated_batch.toml"), None)]
    cases += [(f"threads_{n if n is not None else 'default'}", None, n) for n in (None, 2, 8)]
    if args.case:
        cases = [c for c in cases if c[0] == args.case]
        if not cases:
            parser.error(f"Unknown case: {args.case}")
    suite = ET.Element("testsuite", name="course", tests=str(len(cases)))
    failures = 0
    started = time.monotonic()
    for name, workload, threads in cases:
        report = destination / name
        report.mkdir(exist_ok=True)
        for filename in ("failure.txt", "audit.log", "server.log", "requests.log", "ordering.log", "responses.log"):
            (report / filename).unlink(missing_ok=True)
        element = ET.SubElement(suite, "testcase", classname="course", name=name)
        start = time.monotonic()
        try:
            if workload is None:
                run_threads(binary, threads, report)
            else:
                run_workload(binary, workload.resolve(), report, args.timeout,
                             generated=name == "generated_batch")
        except (OSError, RuntimeError, AssertionError, subprocess.TimeoutExpired) as error:
            failures += 1
            detail = str(error)
            (report / "failure.txt").write_text(detail + "\n")
            ET.SubElement(element, "failure", message=detail).text = detail
            print(f"FAIL {name}: {detail}", flush=True)
        else:
            print(f"PASS {name}", flush=True)
        element.set("time", f"{time.monotonic() - start:.3f}")
    suite.set("failures", str(failures))
    suite.set("time", f"{time.monotonic() - started:.3f}")
    ET.indent(suite)
    ET.ElementTree(suite).write(destination / "junit.xml", encoding="utf-8", xml_declaration=True)
    print(f"{len(cases) - failures}/{len(cases)} course cases passed; reports: {destination}")
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
