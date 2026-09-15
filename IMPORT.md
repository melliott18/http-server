# Import and cleanup

The owner supplied `http-server/` on 2026-09-14. Its root source files were retained as the authoritative application; the alternate grader server was not substituted. Existing source attribution was preserved. The full original import was moved to a private local backup outside the repository before cleanup.

## Retained

- All eleven root C source files, organized into `src/`, and the headers they use in `include/`.
- The supplied workload descriptions, input fixtures, and Python workload/audit/replay tools under `tests/course/`.
- The original GET/PUT model, POSIX threads, queue, hash/list lookup, and reader/writer locks.

## Excluded from the codebase

- Course submission configuration containing credentials, assignment upload/copy scripts, and assignment-only README/configuration.
- Precompiled ARM/x86 archives, executables, and object files. The included source now builds the support library for the selected platform.
- `grader/`, `starter-code/`, and `helper_funcs/` alternate or duplicate source trees; `test_scripts_resources/` older duplicate test helpers.
- `.format/`, `.formatted/`, `.fuse_hidden*`, temporary files, captured output, audit logs, and generated `outs/` and `replay/` results.
- Redundant shell test wrappers; the supported runner covers their workload and thread-count behavior explicitly.

The replacement Makefile names each compilation input instead of linking whichever prebuilt archive happened to be copied into the assignment directory. The Docker context uses an allowlist so private configuration, development data, and old artifacts cannot enter the image build.

## Correctness changes

The import exposed request-buffer bounds, PUT byte-count/cleanup/locking, argument parsing, and audit/runner failure-handling defects. Focused fixes and regression tests accompany the organization change. The supplied course suite remains recognizable; its runners now isolate state, enforce timeouts, and report failures instead of silently passing the outer script. See the tests' documentation and [validation evidence](VALIDATION.md).

## Provenance and distribution

The import includes course-provided helper code and test fixtures as well as the owner's work. No license or new ownership claim has been added. The repository remains private; resolve [decision O05](../../docs/decisions.md#open-decisions-and-evidence) before public distribution. The old course credential file is not part of the cleaned application or Docker context.
