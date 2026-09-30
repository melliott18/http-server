# Application tests

Run from the repository root after building the server:

```sh
python3 tests/test_httpserver.py --binary build/httpserver --report-dir test-results/regression
python3 tests/run_workloads.py --binary build/httpserver --report-dir test-results/workloads
```

These commands require Linux and Python 3.11 or newer and use only Python's
standard library. The workload client uses Linux `epoll`; thread checks read
`/proc`. Run as an ordinary non-root user so permission checks remain meaningful.
See the [application guide](../README.md) for container commands and Make targets.

## HTTP regressions

`test_httpserver.py` checks GET/PUT status codes, binary and empty files,
independent and conflicting concurrent requests, restart persistence, incomplete
bodies, malformed or oversized headers and lengths, unsupported methods,
filesystem permissions, symlink and directory protection, and command-line
validation. It also checks explicit connection closure, unambiguous audit request
IDs, worker recovery from stalled readers and writers, upload cleanup, and bounded
resident memory after 16,384 distinct missing-file requests. The memory check in
`test_lock_lifecycle.py` reads Linux `/proc` after allocator warmup. Each test starts
a server with a temporary document root. Ordinary client operations time out after
three seconds; the server-inactivity checks allow up to 15 seconds.

## Concurrent workloads

`run_workloads.py` runs 30 cases: the 26 TOML workloads in
[`workloads/cases/`](workloads/cases/), a generated 100-request mixed GET/PUT
batch, and thread counts for default, two-worker, and eight-worker configurations.
The workloads exercise audit ordering, conflicting and independent requests,
partial request lines, headers and bodies, slow readers, and atomic visibility
while files are replaced.

Each case has its own temporary document root, generated payloads, and server.
The four `audit_*` cases use one worker; `atomic_multi_put` uses five workers for
its five deliberately stalled PUTs; other request cases use four workers. Thread
checks expect the worker count plus the main server thread.

[`workloads/fixtures.py`](workloads/fixtures.py) generates distinct deterministic
ASCII payloads locally. Six payloads range from 201,936 to 421,545 bytes; five
additional 180,000-byte payloads exercise concurrent PUTs. All exceed the largest
75,000-byte partial-body send. Cases refer to `fixtures/` within their temporary
directory. No fixture download or repository data mutation is needed.

The tools in [`workloads/tools/`](workloads/tools/) perform these checks:

| Tool | Responsibility |
| --- | --- |
| `request_client.py` | Send staged requests from TOML and capture responses and client event order |
| `validate_audit.py` | Reject missing or duplicate request IDs, inconsistent order, and response-status mismatches |
| `validate_responses.py` | Replay operations in audit order and compare expected status codes and response bytes |
| `generate_batch.py` | Produce GET/PUT batches for concurrency testing |

## Reports and diagnosis

Both suites return a nonzero status on failure and write `junit.xml` beneath
their report directory. The workload suite retains a directory for every case:

| Report | Contents |
| --- | --- |
| `server.log` | Server standard output |
| `audit.log` | Server standard error, including audit records |
| `requests.log` | Client events and helper errors |
| `ordering.log` | Audit-order validator output |
| `responses.log` | Response-replay validator output |
| `failure.txt` | Failure summary, present only for a failed case |

Thread-count cases produce server and audit logs. Temporary document roots and
response bodies are removed after each case, including failures. Each server is
terminated during cleanup, with a forced stop if it does not exit in two seconds.
Startup has a five-second deadline. Workload helper processes have a 45-second
timeout, configurable with `--timeout`; individual client socket operations have
a 15-second timeout.

Run an individual workload with:

```sh
python3 tests/run_workloads.py --binary build/httpserver \
  --report-dir test-results/workloads --case audit_get
```

Use a separate report directory when comparing runs. A selected-case run writes
JUnit results only for that selection.

## Runtime image checks

`test_container.py` uses Docker and Python 3.11 or newer to check an already-built
runtime image. It tests non-root startup, binary PUT/GET, concurrency, and data
persistence across container replacement. It resolves the image tag to a local
immutable image ID and removes its own temporary containers and volume:

```sh
python3 tests/test_container.py --image http-server:local \
  --report test-results/runtime.json
```
