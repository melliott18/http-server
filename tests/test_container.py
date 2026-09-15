#!/usr/bin/env python3
"""Exercise an existing runtime image, including replacement with a shared volume."""

import argparse
import concurrent.futures
import http.client
import json
from pathlib import Path
import subprocess
import time
import uuid


def docker(*args):
    return subprocess.check_output(["docker", *args], text=True, timeout=30).strip()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, help="Already-built runtime image")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    image_id = docker("image", "inspect", "--format", "{{.Id}}", args.image)
    name = "http-check-" + uuid.uuid4().hex[:12]
    volume = name + "-data"
    port = None
    checks = []

    def request(method, filename, body=None):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        try:
            connection.request(method, "/" + filename, body=body)
            response = connection.getresponse()
            return response.status, response.read()
        finally:
            connection.close()

    def start():
        nonlocal port
        docker("run", "-d", "--name", name, "--read-only", "--cap-drop", "ALL",
               "--security-opt", "no-new-privileges", "--memory", "128m",
               "--cpus", "2", "--pids-limit", "128",
               "--publish", "127.0.0.1::8080",
               "--mount", f"type=volume,source={volume},target=/data", image_id)
        port = int(docker("port", name, "8080/tcp").rsplit(":", 1)[1])
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            try:
                if request("GET", "absent")[0] == 404:
                    return
            except (OSError, http.client.HTTPException):
                time.sleep(0.1)
        raise RuntimeError("Runtime image did not become ready")

    docker("volume", "create", volume)
    try:
        start()
        require(docker("exec", name, "id", "-u") == "10001", "runtime must be non-root")
        checks.append("non-root runtime starts with read-only root filesystem")
        payload = bytes(range(256)) * 256
        require(request("PUT", "persistent.bin", payload)[0] == 201, "PUT must create file")
        require(request("GET", "persistent.bin") == (200, payload), "GET must match binary bytes")
        checks.append("binary PUT/GET round trip")

        def parallel(i):
            filename = f"parallel-{i}"
            content = f"request {i}\n".encode() * 100
            require(request("PUT", filename, content)[0] == 201, f"PUT failed: {filename}")
            require(request("GET", filename) == (200, content), f"GET mismatch: {filename}")

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(parallel, range(16)))
        checks.append("16 concurrent independent PUT/GET pairs")
        docker("stop", "--time", "5", name)
        docker("rm", name)
        start()
        require(request("GET", "persistent.bin") == (200, payload), "File lost after replacement")
        require(request("PUT", "persistent.bin", b"replacement")[0] == 200, "PUT must replace file")
        require(request("GET", "persistent.bin") == (200, b"replacement"), "Replacement bytes differ")
        checks.append("data survives container replacement and remains writable")
        report = {"image": args.image, "image_id": image_id, "checks": checks,
                  "os_architecture": docker("image", "inspect", "--format",
                                            "{{.Os}}/{{.Architecture}}", image_id),
                  "result": "passed"}
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    except Exception:
        subprocess.run(["docker", "logs", name], check=False, timeout=10)
        raise
    finally:
        subprocess.run(["docker", "rm", "-f", name], check=False, timeout=30,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["docker", "volume", "rm", volume], check=False, timeout=30,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == "__main__":
    main()
