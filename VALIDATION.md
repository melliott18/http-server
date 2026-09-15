# Import validation — 2026-09-14

Validated the uncommitted application preparation on `feature/17-c-http-server`, based on platform commit `bfde1b36007a92b8529a6123b3cebddae3251e56`. These results establish a standalone application baseline, not completion of the shared pipeline or [#17](https://github.com/melliott18/pipeline/issues/17).

## Environment and results

Docker Desktop on the owner's ARM64 Mac supplied the Linux kernel (`6.4.16-linuxkit`). Both container builds and tests used native `linux/arm64`; no x86 emulation was involved.

| Check | Ubuntu 24.04 | Ubuntu 26.04 |
| --- | --- | --- |
| Image userspace | 24.04.4 LTS | 26.04.1 LTS |
| Compiler | Clang 18.1.3 | Clang 21.1.8 |
| C library | glibc 2.39 | glibc 2.43 |
| Test interpreter | Python 3.12.3 | Python 3.14.4 |
| Strict C17 source build, warnings as errors | Passed | Passed |
| HTTP regression methods | 13/13 passed | 13/13 passed |
| Course workload/batch/thread cases | 30/30 passed | 30/30 passed |
| Non-root runtime with read-only root filesystem | Passed | Passed |
| Binary PUT/GET and 16 concurrent PUT/GET pairs | Passed | Passed |
| Persistent file survives container replacement | Passed | Passed |

Both `make test` container runs exited 0. The runtime checks used the already-built final images with separate temporary volumes. Ubuntu 26.04 availability was checked against the [official release notes](https://documentation.ubuntu.com/release-notes/26.04/) and by pulling and running its image; the table records observed versions.

The original source compiled on Ubuntu 24.04, but failed the new regression suite for behaviors including truncated uploads, invalid Content-Length, incomplete headers, and symlink access. The imported suite initially passed 27/30 cases after harness modernization; three defective workload/client cases were repaired and documented in [test provenance](tests/README.md), then the complete matrix was rerun successfully. The claimed course results include those disclosed repairs.

## Failure checks

- Using `/bin/false` as the server for the `audit_get` course case produced a startup diagnostic, a failing JUnit case, and exit status 1.
- Removing `src/fdwrapper.c` inside a disposable test container, then rebuilding from clean, stopped with a missing build input diagnostic and exit status 2.
- The runtime verification script also passed under `python3 -O`; its checks execute even when Python optimization is enabled.
- Clang static analysis on Ubuntu 26.04 reported an existing unused transfer-result assignment in `conn_send_file`; it reported no other diagnostics. Static analysis is not a correctness or security guarantee.
- Repository whitespace and relative documentation links were checked. Imported payload bytes were retained, including their original whitespace. Formatting-only cleanup of four TOML files was checked to preserve the parsed definitions used by the passing test run.

## Inputs and retained evidence

| Input | Identity |
| --- | --- |
| Ubuntu 24.04 base | `ubuntu:24.04@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254` |
| Ubuntu 26.04 base | `ubuntu:26.04@sha256:513c074113a871b51a8d16ab445c88779d6452d937a164fb5cc479f32668a41d` |
| Ubuntu 24.04 runtime image ID | `sha256:0d7310bce511bd74335cc5958749bbde0f7bb49fa63cd82b644b39bef1994c0a` |
| Ubuntu 26.04 runtime image ID | `sha256:24e2d6d35b0f888555e29037b1f5c5acce02c445c5ffea2d0ea6127fb091d9a9` |
| Ubuntu 24.04 server binary SHA-256 | `9cbb57bc651da94bb4baa46556b460e7ee7ad0e7e005dc5e6bf633489b64dd39` |
| Local build/test input manifest SHA-256 | `284b1a19e3dc5723d16f4ef0df892f250c468f8cee7e5ac64853ab81d9a9b9bc` |

The 24.04 server binary hash matched between the test stage and the running final image. Image IDs above are local image identities, not published registry digests or qualified releases. Ubuntu packages were resolved during image construction; the builds do not freeze the package repository.

Local evidence is retained under ignored `test-results/`: `ubuntu-24.04/` and `ubuntu-26.04/` contain regression and course JUnit/logs; `ubuntu-24.04-runtime.json` and `ubuntu-26.04-runtime.json` record runtime checks; `runner-negative/`, `baseline-regression/`, and `logs/` preserve failure and console evidence. `input-manifest.json` lists SHA-256 hashes of build inputs, C sources/headers, test scripts, workloads, and fixtures. Its own hash is computed from the compact, sorted JSON mapping of relative paths to file hashes. This Markdown file preserves the validation summary in version control; CI artifact publication is future work.

## Remaining scope

The actual Ubuntu VM and `linux/amd64` have not been tested. Public distribution rights, production storage/backup policy, workload limits, graceful request draining, and hardening remain open. Jenkins, Terraform, a shared manifest, image publication/qualification, and automated deployment are not part of this standalone bootstrap. The [application guide](README.md) contains the supported commands and operational limits.
