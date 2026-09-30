# Contributing

Read the [application guide](README.md), [test guide](tests/README.md), and
[operations guide](OPERATIONS.md) before changing behavior.

## Development workflow

Track changes in [GitHub Issues](https://github.com/melliott18/http-server/issues).
Use a short-lived `<type>/<issue-number>-<short-description>` branch and a pull
request into `main`. Types are `feature`, `fix`, `docs`, `refactor`, `test`, or
`chore`; descriptions use lowercase words separated by hyphens. Use real issue
numbers and keep each change focused.

Run `make` and `make test` on Linux as an ordinary user, or use the documented
Docker commands. Tests need disposable data directories. Build the runtime image
and run the following when changing the container or runtime interface:

```sh
python3 tests/test_container.py --image http-server:local \
  --report test-results/runtime.json
```

Run `git diff --check` and review documentation links. Describe actual validation
and any untested targets in the pull request.

Keep credentials, build products, test reports, and live data out of Git. Preserve
existing author notices and unrelated working-tree changes. Application-specific
build and test logic belongs here; shared platform orchestration belongs to its
own project.

## Provenance and licensing

The owner requested this public standalone repository on 2026-09-30. The project
was extracted from `examples/c-http-server/` at Pipeline revision
[`1442ce0`](https://github.com/melliott18/pipeline/commit/1442ce0), whose latest
application change was
[`523bf30`](https://github.com/melliott18/pipeline/commit/523bf30).
Those links refer to the original private repository and may require access.

The two application commits were retained with paths moved to the repository
root. Historical `tests/course/test_files/` fixture files were excluded from all
published commits; current tests generate their payloads locally. Extraction and
filtering change commit IDs. Issue and PR numbers in imported commit messages
refer to Pipeline. Platform files and history, local reports, build artifacts,
and application data were not imported.

Initializing `main` from this filtered application history and adding standalone
project documentation is the owner-authorized bootstrap exception for this
repository. Subsequent changes follow the issue and pull request workflow above.
Pipeline tracks its corresponding extraction under
[#28](https://github.com/melliott18/pipeline/issues/28); platform integration
remains a separate task.

The source includes existing support-library and test-tool contributions as well
as the owner's work. Their author notices are preserved. No project license has
been selected or added; public repository visibility does not grant a new license.
