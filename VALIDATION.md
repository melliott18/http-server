# Application validation

## Repository extraction — 2026-09-30

Validated the standalone repository root on Docker Desktop `linux/arm64`, using
its pinned Ubuntu 24.04 base and unchanged executable inputs. All 60 source,
header, build, and executable test files match Pipeline revision `1442ce0`.
Documentation paths, container examples, and root Git ignores were adapted for
the independent project.

| Check | Result |
| --- | --- |
| Docker test and runtime image builds | Passed; unchanged build layers reused from cache |
| HTTP regression suite, isolated rerun | 17/17 passed |
| Concurrent workload/batch/thread suite | 30/30 passed |
| Non-root, read-only runtime; binary and concurrent PUT/GET | Passed |
| Data survives container replacement and remains writable | Passed |
| Whitespace, local Markdown links, and application-only history review | Passed |

The initial `make test` run passed 16 regression methods and timed out at the
replacement PUT in `test_stalled_readers_release_workers_and_file_locks`, before
workloads started. An independent runtime-image check was running concurrently.
The same unmodified image and tests passed the complete isolated rerun. This
intermittent result is tracked in [issue #1](https://github.com/melliott18/http-server/issues/1);
a timing/synchronization explanation remains a hypothesis. No server or
executable-test change was made for extraction.

Commands used were `docker build --target test -t http-server:test .`,
`docker run --name http-server-extraction-tests --network none http-server:test`,
`docker build --target runtime -t http-server:local .`, and the documented
`tests/test_container.py` check with `--image http-server:local` and
`--report test-results/extraction-runtime.json`.

The runtime image ID was
`sha256:afa83edad44b2a460a102ab9658808b58f01353b1ab7bdd449d2605d9cbc59ea`.
Local reports are retained under ignored `test-results/extraction-first-run/`
and `test-results/extraction-passing-run/`, with runtime results in
`test-results/extraction-runtime.json`. The compact sorted input manifest is
`test-results/extraction-input-manifest.json`, with SHA-256
`fff4b99018fd1a53b2f284ef61c228973b140caf021c54017342aca3f2f8b312`.
Reports are local evidence and are not included in a clone. These checks do not
validate another CPU architecture, the target VM, or shared platform integration.

## Application hardening — 2026-09-15

Validated the application changes on `refactor/17-http-server-production`, based
on Pipeline `main` commit
[`32b03a4`](https://github.com/melliott18/pipeline/commit/32b03a4). The input
manifest below identifies the exact C sources,
headers, build configuration, test scripts, and workload definitions exercised.
This is standalone application evidence for
[#17](https://github.com/melliott18/pipeline/issues/17).

### Environment and results

Docker Desktop provided Linux kernel `6.4.16-linuxkit` on native `linux/arm64`.
Tests ran as UID/GID 10001 with networking disabled and disposable document roots.

| Check | Ubuntu 24.04 | Ubuntu 26.04 |
| --- | --- | --- |
| Image userspace | 24.04.4 LTS | 26.04.1 LTS |
| Compiler | Clang 18.1.3 | Clang 21.1.8 |
| C library | glibc 2.39 | glibc 2.43 |
| Test interpreter | Python 3.12.3 | Python 3.14.4 |
| Strict C17 source build, warnings as errors | Passed | Passed |
| HTTP regression methods | 17/17 passed | 17/17 passed |
| Workload/batch/thread cases | 30/30 passed | 30/30 passed |
| Non-root runtime with read-only root filesystem | Passed | Passed |
| Binary PUT/GET and 16 concurrent PUT/GET pairs | Passed | Passed |
| Persistent file survives container replacement | Passed | Passed |

Both final `make test` container runs exited 0. Runtime checks used
`tests/test_container.py` against the already-built runtime image IDs below with
separate temporary volumes. Commands follow the [application guide](README.md).

The regression suite verifies that stalled request senders and response readers
release workers, incomplete uploads preserve files and discard temporary data,
completed requests reclaim filename locks, responses declare connection closure,
and request IDs cannot add audit fields. The workload suite retains 26 staged
request scenarios plus batch and worker-count cases. All payloads are generated
locally; workload events and assertions were checked after fixture-path changes.

### Failure and memory-safety checks

- The filename-lock regression failed against the previous Ubuntu 24.04 binary:
  resident memory grew by 5,032 KiB across 16,384 unique missing-file requests,
  exceeding its 2,048 KiB allowance after warmup. The updated binary passed on
  both Ubuntu versions.
- The stalled-client regression failed against that previous binary with a
  socket timeout after 15 seconds. The updated server released stalled clients
  and passed both receive and send timeout checks on both Ubuntu versions.
- Using `/bin/false` for the `audit_get` workload produced a startup diagnostic,
  a failing JUnit case, and exit status 1.
- AddressSanitizer and UndefinedBehaviorSanitizer on Ubuntu 26.04 passed four
  focused concurrency, interrupted-upload, and repeated lock-lifecycle checks
  with no diagnostics. This included 4,096 unique missing-file requests and 128
  concurrent mixed valid/truncated PUT and GET operations. A disposable
  validation image added `libclang-rt-21-dev` version `1:21.1.8-6ubuntu1`, needed
  by the sanitizer linker; the runtime image was unchanged. These checks do not
  establish graceful process-exit cleanup or complete memory-safety coverage.
- Repository whitespace, local Markdown links, and application naming were
  reviewed, including new files.

### Inputs and retained evidence

| Input | Identity |
| --- | --- |
| Ubuntu 24.04 base | `ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254` |
| Ubuntu 26.04 base | `ubuntu:26.04@sha256:513c074113a871b51a8d16ab445c88779d6452d937a164fb5cc479f32668a41d` |
| Ubuntu 24.04 runtime image ID | `sha256:afa83edad44b2a460a102ab9658808b58f01353b1ab7bdd449d2605d9cbc59ea` |
| Ubuntu 26.04 runtime image ID | `sha256:befe5de3ecb93a417321090a91ab38786ae5d088d645e555e6369e9087afb828` |
| Ubuntu 24.04 server binary SHA-256 | `6f63ec2fda7f5024e3ba81d155b4dd80cc038c598866eeae3be1476b2b4e5717` |
| Ubuntu 26.04 server binary SHA-256 | `9110c09afafd64a3da9957b12992a1708784314ecf05f63e7477a8e29e5c5bb1` |
| Build/test input manifest SHA-256 | `fff4b99018fd1a53b2f284ef61c228973b140caf021c54017342aca3f2f8b312` |

Image IDs are local immutable identities, not published registry digests or
qualified platform releases. Package repositories were resolved during image
construction; the base digest does not freeze compiler-package versions.

At the time of validation, ignored `test-results/ubuntu-24.04/` and
`ubuntu-26.04/` contained regression and
workload JUnit/logs. `production-24.04-runtime.json` and
`production-26.04-runtime.json` contained runtime outcomes. `environment.json`,
`input-manifest.json`, `logs/`, `runner-negative/`, `baseline-lock-memory/`, and
`sanitizers/` retained toolchain identities and diagnostic evidence. The manifest
hash is computed over the compact, sorted JSON mapping of input paths to SHA-256
hashes. This document records the summary; automated artifact publication remains
platform work. These historical local reports were not copied into the extracted
repository; a fresh clone does not include them.

### Release requirements

The actual Ubuntu VM and `linux/amd64` have not been validated. Access controls,
workload limits, storage quotas, crash durability, graceful shutdown, and backup
and restore need target-specific acceptance. See the
[operations guide](OPERATIONS.md). Shared manifests, Jenkins integration, image
qualification, and automated deployment remain pending under #17 and its
dependencies.
