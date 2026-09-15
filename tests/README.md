# Application tests

Run from `examples/c-http-server` after building the server:

```sh
python3 tests/test_httpserver.py --binary build/httpserver --report-dir test-results/regression
python3 tests/run_course.py --binary build/httpserver --report-dir test-results/course
```

These commands require Linux and Python 3.11 or newer. They use only Python's
standard library. The course client uses Linux `epoll`; thread checks read
`/proc`. See the [application README](../README.md) for the Ubuntu container
commands and the supported build targets.

The regression suite has 13 test methods covering GET and PUT status codes,
binary and empty files, independent and conflicting concurrent requests,
restart persistence on the same port, incomplete bodies, malformed or oversized
headers and lengths,
unsupported methods, read-only files, symlink and directory protection, and
invalid command-line arguments. The permission test expects an ordinary non-root
user, as configured by the test container.
Each method starts a server with a temporary document root. Client operations
have a three-second socket timeout; startup is limited to five seconds.

The course suite has 30 cases: all 26 supplied TOML workloads, a generated
100-request mixed GET/PUT batch, and thread counts for the default, two-worker,
and eight-worker configurations. The four `audit_*` workloads use one worker;
`atomic_multi_put` uses five to accommodate its five deliberately stalled PUTs;
other workloads use four. Every case runs in its own temporary document root,
so fixture copies, requests, and replay operations cannot change repository data.
The runner terminates its server even if a client or validator fails. Each helper
process has a 45-second timeout, configurable with `--timeout`. The client's
individual socket operations have a 15-second timeout.

Both suites return a nonzero exit status on failure and write JUnit XML. The
course report also retains the server audit log, client event log, and validator
output for each case. Temporary document roots are removed after each case.
To diagnose a single course case:

```sh
python3 tests/run_course.py --binary build/httpserver \
  --report-dir test-results/course --case audit_get
```

## Imported course tests

`course/test_files`, `course/workloads`, and the four helpers under
`course/test_scripts` came with the supplied systems-design assignment. The
fixtures and workload intent are retained. Their authorship or redistribution
license has not been inferred from the import.

The original shell driver and wrappers were replaced because they could report
success after failures, fail to detect startup timeouts, share mutable files,
and omit supplied workloads. The unused duplicate request client was removed.
The retained helpers have these focused corrections:

- TOML parsing uses `tomllib`; replay file operations use Python rather than
  unchecked shell commands.
- Audit validation rejects missing or duplicate request IDs and mismatched
  response statuses; replay checks status codes as well as response bodies.
- Unsupported-method requests use the valid token `INVALID`, return an expected
  501 response, and participate in audit validation.
- Sleep events do not create fictitious requests. Unknown workload events fail.
- `two_slow_get_header_batch.toml` uses `SEND_HEADERS` consistently and unloads
  its extra fixture. The old `slow_put_body` wrapper referenced a nonexistent
  shell file; the runner selects the supplied TOML directly.
- `atomic_multi_put` and `audit_unsupported`, omitted by the original wrappers,
  now run with the rest of the workload set.

- The five absent `atomic_multi_put` payloads are replaced with distinct,
  deterministic 180 KB text files generated in its temporary fixture directory.
  Their length preserves the workload’s 75 KB partial-body operations.
- The client accepts a partial-read event after its poller has already finished
  a small response; the complete response is still validated.
- `atomic_get` originally attached bodies to GET requests while its comments
  described APPEND and PUT operations. It now checks atomic reading directly:
  partially receive a large GET response, submit a different PUT concurrently,
  finish both, and validate a final GET against the replacement.
