#!/usr/bin/env python3
"""Application regression tests; Linux, Python 3.11+, and a compiled binary only."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
import io
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET

from test_lock_lifecycle import assert_bounded_lock_memory

BINARY = None


class HTTPServerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="httpserver-regression-")
        self.root = Path(self.directory.name)
        self.process = None
        self.log = (self.root / "server.log").open("wb")
        self.addCleanup(self.directory.cleanup)
        self.addCleanup(self.log.close)
        self.addCleanup(self.stop)
        self.start()

    def start(self, port=None):
        if port is None:
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]
        self.port = port
        self.process = subprocess.Popen([str(BINARY), "-t", "4", str(self.port)],
                                        cwd=self.root, stdout=self.log, stderr=self.log)
        deadline = time.monotonic() + 5
        while True:
            if self.process.poll() is not None:
                self.fail(f"Server exited during startup: {self.process.returncode}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.1):
                    return
            except OSError:
                if time.monotonic() >= deadline:
                    self.fail("Server did not listen within 5 seconds")
                time.sleep(0.02)

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

    def raw(self, request, timeout=3):
        with socket.create_connection(("127.0.0.1", self.port), timeout=timeout) as sock:
            sock.sendall(request)
            sock.shutdown(socket.SHUT_WR)
            response = bytearray()
            while True:
                try:
                    data = sock.recv(65536)
                except ConnectionResetError:
                    # A rejected oversized request can leave unread bytes when the
                    # server closes. A complete error response is still required.
                    break
                if not data:
                    break
                response.extend(data)
                self.assertLess(len(response), 8 * 1024 * 1024, "Unbounded response")
        headers, separator, body = bytes(response).partition(b"\r\n\r\n")
        self.assertEqual(separator, b"\r\n\r\n", f"Incomplete HTTP response: {response[:200]!r}")
        lines = headers.split(b"\r\n")
        status = int(lines[0].split()[1])
        fields = dict(line.lower().split(b":", 1) for line in lines[1:])
        self.assertIn(b"content-length", fields)
        self.assertEqual(fields.get(b"connection", b"").strip(), b"close")
        self.assertEqual(int(fields[b"content-length"]), len(body))
        return status, body

    def request(self, method, name, body=b"", rid=1):
        headers = (f"{method} /{name} HTTP/1.1\r\nRequest-Id: {rid}\r\n"
                   f"Content-Length: {len(body)}\r\n\r\n").encode()
        return self.raw(headers + body)

    def test_create_replace_get_and_missing(self):
        self.assertEqual(self.request("GET", "missing.txt"), (404, b"Not Found\n"))
        self.assertEqual(self.request("PUT", "document.txt", b"first"), (201, b"Created\n"))
        self.assertEqual(self.request("GET", "document.txt"), (200, b"first"))
        self.assertEqual(self.request("PUT", "document.txt", b"second"), (200, b"OK\n"))
        self.assertEqual(self.request("GET", "document.txt"), (200, b"second"))

    def test_binary_payload_and_declared_length(self):
        payload = bytes(range(256)) * 1024
        self.assertEqual(self.request("PUT", "binary.dat", payload)[0], 201)
        self.assertEqual(self.request("GET", "binary.dat"), (200, payload))
        self.assertEqual(self.raw(b"PUT /limited.dat HTTP/1.1\r\nContent-Length: 3\r\n\r\nabcdef")[0], 201)
        self.assertEqual((self.root / "limited.dat").read_bytes(), b"abc")

    def test_empty_file_and_case_insensitive_header(self):
        self.assertEqual(self.raw(b"PUT /empty.dat HTTP/1.1\r\ncontent-length: 0\r\n\r\n")[0], 201)
        self.assertEqual(self.request("GET", "empty.dat"), (200, b""))

    def test_parallel_independent_requests(self):
        def write_read(index):
            name = f"parallel{index}.dat"
            body = bytes([index]) * 8192
            self.assertEqual(self.request("PUT", name, body, index + 1)[0], 201)
            self.assertEqual(self.request("GET", name, rid=index + 100), (200, body))
        with ThreadPoolExecutor(max_workers=8) as workers:
            list(workers.map(write_read, range(16)))

    def test_parallel_same_file_is_complete(self):
        payloads = [bytes([index]) * 65536 for index in range(8)]
        self.assertEqual(self.request("PUT", "shared.dat", payloads[0])[0], 201)
        def replace_read(index):
            self.assertEqual(self.request("PUT", "shared.dat", payloads[index], index + 1)[0], 200)
            status, received = self.request("GET", "shared.dat", rid=index + 100)
            self.assertEqual(status, 200)
            self.assertIn(received, payloads)
        with ThreadPoolExecutor(max_workers=8) as workers:
            list(workers.map(replace_read, range(8)))

    def test_restart_preserves_data(self):
        payload = b"persistent application data\n"
        self.assertEqual(self.request("PUT", "persistent.txt", payload)[0], 201)
        port = self.port
        self.stop()
        self.start(port=port)
        self.assertEqual(self.request("GET", "persistent.txt"), (200, payload))

    def test_completed_requests_release_filename_locks(self):
        assert_bounded_lock_memory(self)

    def test_truncated_put_preserves_existing_file(self):
        self.assertEqual(self.request("PUT", "existing.txt", b"original")[0], 201)
        malformed = b"PUT /existing.txt HTTP/1.1\r\nContent-Length: 10\r\n\r\nshort"
        self.assertEqual(self.raw(malformed)[0], 400)
        self.assertEqual(self.request("GET", "existing.txt"), (200, b"original"))
        self.assertEqual({p.name for p in self.root.iterdir()}, {"server.log", "existing.txt"},
                         "Interrupted PUT left temporary data")
        self.assertEqual(self.request("PUT", "existing.txt", b"retry"), (200, b"OK\n"))
        self.assertEqual(self.request("GET", "existing.txt"), (200, b"retry"))

    def test_invalid_content_lengths(self):
        for value in (b"-1", b"nope", b"1x", b"18446744073709551616", b""):
            with self.subTest(value=value):
                request = b"PUT /invalid.dat HTTP/1.1\r\nContent-Length: " + value + b"\r\n\r\nx"
                self.assertEqual(self.raw(request)[0], 400)
                self.assertFalse((self.root / "invalid.dat").exists())
        duplicate = b"PUT /invalid.dat HTTP/1.1\r\nContent-Length: 1\r\ncontent-length: 2\r\n\r\nx"
        self.assertEqual(self.raw(duplicate)[0], 400)
        self.assertEqual(self.raw(b"PUT /invalid.dat HTTP/1.1\r\n\r\n")[0], 400)
        self.assertEqual(self.raw(b"PUT /invalid.dat HTTP/1.1\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n")[0], 400)

    def test_request_id_cannot_inject_audit_fields(self):
        request = (b"PUT /invalid.dat HTTP/1.1\r\nContent-Length: 1\r\n"
                   b"Request-Id: client,200,forged\r\n\r\nx")
        self.assertEqual(self.raw(request)[0], 400)
        self.assertFalse((self.root / "invalid.dat").exists())
        self.assertEqual(self.request("PUT", "valid.dat", b"x", rid="client-42")[0], 201)
        self.assertEqual(self.request("GET", "valid.dat"), (200, b"x"))
        self.assertNotIn(b"forged", (self.root / "server.log").read_bytes())

    def test_stalled_clients_release_workers_and_uploads(self):
        (self.root / "existing.txt").write_bytes(b"original")
        requests = [b"", b"GET /missing HTTP/1.1\r\nPartial:",
                    b"PUT /existing.txt HTTP/1.1\r\nContent-Length: 10\r\n\r\nx",
                    b"PUT /new.txt HTTP/1.1\r\nContent-Length: 10\r\n\r\nx"]
        with ExitStack() as stack:
            clients = [stack.enter_context(socket.create_connection(
                ("127.0.0.1", self.port), timeout=15)) for _ in requests]
            for client, request in zip(clients, requests):
                if request:
                    client.sendall(request)
            started = time.monotonic()
            # Keep every peer open; only the server's timeout can release them.
            for client in clients:
                response = bytearray()
                while data := client.recv(4096):
                    response.extend(data)
                self.assertIn(bytes(response).split(b" ")[1], (b"400", b"500"))
            self.assertLess(time.monotonic() - started, 14)
        self.assertEqual(self.request("GET", "existing.txt"), (200, b"original"))
        self.assertFalse((self.root / "new.txt").exists())
        self.assertFalse(list(self.root.glob(".httpserver_upload-*")))
        self.assertEqual(self.request("PUT", "new.txt", b"retry")[0], 201)

    def test_stalled_readers_release_workers_and_file_locks(self):
        (self.root / "large.dat").write_bytes(b"x" * (8 * 1024 * 1024))
        with ExitStack() as stack:
            for _ in range(4):
                client = stack.enter_context(socket.socket())
                client.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                client.settimeout(3)
                client.connect(("127.0.0.1", self.port))
                client.sendall(b"GET /large.dat HTTP/1.1\r\n\r\n")
                # Verify each worker has started sending, then stop consuming.
                self.assertEqual(client.recv(1), b"H")
            response = self.raw(b"GET /missing HTTP/1.1\r\n\r\n", timeout=14)
            self.assertEqual(response, (404, b"Not Found\n"))
            # Readers still holding the filename lock would block replacement.
            self.assertEqual(self.request("PUT", "large.dat", b"replacement")[0], 200)
            self.assertEqual(self.request("GET", "large.dat"), (200, b"replacement"))

    def test_malformed_headers_and_unsupported_method(self):
        for raw in (b"GET /data HTTP/1.1\r\nBad Header: value\r\n\r\n",
                    b"GET /data HTTP/1.1\r\nMissingColon\r\n\r\n",
                    b"GET /data HTTP/1.1\r\nIncomplete: header\r\n"):
            with self.subTest(request=raw):
                self.assertEqual(self.raw(raw)[0], 400)
        self.assertEqual(self.request("INVALID", "data"), (501, b"Not Implemented\n"))

    def test_oversized_request_line_and_headers(self):
        requests = [b"GET /" + b"a" * 4096 + b" HTTP/1.1\r\n\r\n",
                    b"GET /data HTTP/1.1\r\nLong: " + b"x" * 4096 + b"\r\n\r\n"]
        for request in requests:
            with self.subTest(request_size=len(request)):
                self.assertEqual(self.raw(request)[0], 400)
        self.assertEqual(self.request("GET", "missing"), (404, b"Not Found\n"))

    def test_read_only_file_and_symlinks(self):
        protected = self.root / "protected.txt"
        protected.write_bytes(b"original")
        protected.chmod(0o444)
        self.assertEqual(self.request("PUT", "protected.txt", b"overwrite")[0], 403)
        self.assertEqual(protected.read_bytes(), b"original")
        link = self.root / "link.txt"
        link.symlink_to("protected.txt")
        self.assertEqual(self.request("GET", "link.txt")[0], 403)
        self.assertEqual(self.request("PUT", "link.txt", b"overwrite")[0], 403)
        self.assertTrue(link.is_symlink())
        self.assertEqual(protected.read_bytes(), b"original")

    def test_directory_cannot_be_replaced(self):
        (self.root / "directory").mkdir()
        self.assertEqual(self.request("PUT", "directory", b"file")[0], 403)
        self.assertTrue((self.root / "directory").is_dir())

    def test_invalid_cli_arguments(self):
        cases = [[], ["0"], ["65536"], ["-1"], ["80x"], ["8080", "extra"],
                 ["-t", "0", "8080"], ["-t", "-1", "8080"],
                 ["-t", "2x", "8080"], ["-t", "1025", "8080"],
                 ["-t", "999999999999999999999", "8080"]]
        for args in cases:
            with self.subTest(args=args):
                result = subprocess.run([str(BINARY), *args], cwd=self.root,
                                        capture_output=True, timeout=2)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr, "Invalid arguments need a useful diagnostic")


class XMLResult(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.xml = ET.Element("testsuite", name="httpserver")

    def startTest(self, test):
        super().startTest(test)
        self.started = time.monotonic()
        self.element = ET.SubElement(self.xml, "testcase", classname=type(test).__name__,
                                     name=test._testMethodName)

    def stopTest(self, test):
        self.element.set("time", f"{time.monotonic() - self.started:.3f}")
        super().stopTest(test)

    def addFailure(self, test, err):
        super().addFailure(test, err)
        ET.SubElement(self.element, "failure").text = self._exc_info_to_string(err, test)

    def addError(self, test, err):
        super().addError(test, err)
        ET.SubElement(self.element, "error").text = self._exc_info_to_string(err, test)

    def addSubTest(self, test, subtest, err):
        super().addSubTest(test, subtest, err)
        if err:
            ET.SubElement(self.element, "failure", message=str(subtest)).text = self._exc_info_to_string(err, test)


def main():
    global BINARY
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("Application tests require Linux; use the Ubuntu test container")
    BINARY = args.binary.resolve()
    if not BINARY.is_file():
        parser.error(f"Missing server binary: {BINARY}; build it first")
    args.report_dir.mkdir(parents=True, exist_ok=True)
    stream = io.StringIO()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(HTTPServerTests)
    result = unittest.TextTestRunner(stream=stream, verbosity=2, resultclass=XMLResult).run(suite)
    log = stream.getvalue()
    print(log, end="")
    (args.report_dir / "results.log").write_text(log)
    cases = list(result.xml)
    result.xml.set("tests", str(len(cases)))
    result.xml.set("failures", str(sum(case.find("failure") is not None and case.find("error") is None
                                     for case in cases)))
    result.xml.set("errors", str(sum(case.find("error") is not None for case in cases)))
    ET.indent(result.xml)
    ET.ElementTree(result.xml).write(args.report_dir / "junit.xml", encoding="utf-8", xml_declaration=True)
    return not result.wasSuccessful()


if __name__ == "__main__":
    sys.exit(main())
