# Repository instructions

- Read [README.md](README.md), [CONTRIBUTING.md](CONTRIBUTING.md), and the current
  issue before edits. Read [OPERATIONS.md](OPERATIONS.md) for runtime changes.
- Inspect Git status first and preserve unrelated changes. Use the issue-based
  branch and pull request workflow in the contribution guide.
- Keep server source, support-library source, build logic, and application tests
  self-contained. Shared CI platform integration is separate work.
- Supported commands are `make`, `make test`, `make test-regression`, and
  `make test-workloads`, plus Docker build/test and runtime-image checks documented
  in the README. Tests require Linux, a non-root user, and disposable data.
- Run checks appropriate to the change; report actual results and untested
  platforms. For documentation, run `git diff --check` and check links, including
  new files. Add meaningful behavioral checks when behavior changes.
- Preserve author notices. Never commit credentials, build outputs, local reports,
  or live application data. Keep persistent files outside images and disposable
  workspaces; application rollback does not imply data rollback.
- Update interface and operations documentation when behavior changes. Document
  exact supported commands when adding executable entry points.
