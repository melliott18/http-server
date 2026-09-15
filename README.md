# C HTTP server

A Linux, POSIX-threaded file server imported from the owner's systems course assignment. It supports GET and PUT for a single filename in its working directory, with a bounded worker queue and per-file reader/writer locks. PUT creates or replaces persistent files. Audit records go to standard error.

This is standalone application preparation for [#17](https://github.com/melliott18/pipeline/issues/17). Jenkins, Terraform provisioning, the shared application manifest, and automated releases are not implemented here. See [import notes](IMPORT.md) for provenance and cleanup, and [validation evidence](VALIDATION.md) for measured compatibility.

## Layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Server and supplied support library source; existing author notices retained |
| `Makefile` | Explicit source build, dependency tracking, application test entry points |
| `Dockerfile` | Ubuntu toolchain, build, test, and non-root runtime stages |
| `tests/` | Isolated supplied workloads and focused HTTP regression tests |
| `build/`, `test-results/`, `data/` | Ignored local outputs and development data |

## Build and test in Docker

Run these commands from `examples/c-http-server/`. Docker must be running; no host C compiler or Python installation is needed. Building downloads Ubuntu packages; running the tests needs no external network.

```sh
docker build --target test -t c-http-server:test .
docker run --name c-http-server-tests --network none c-http-server:test
```

The test command exits nonzero on failure. After either success or failure, copy the supplied-suite reports and remove its disposable container:

```sh
docker cp c-http-server-tests:/app/test-results ./test-results
docker rm c-http-server-tests
```

Tests run as UID/GID 10001, in disposable directories, without mounting the repository or real application data. Both suites write JUnit XML and retained logs beneath `test-results/`. Regression results also appear in the container output.

The default Ubuntu 24.04 base image is pinned by digest. To test Ubuntu 26.04, use the same Dockerfile with this explicit base:

```sh
docker build --target test \
  --build-arg UBUNTU_IMAGE=ubuntu:26.04@sha256:513c074113a871b51a8d16ab445c88779d6452d937a164fb5cc479f32668a41d \
  -t c-http-server:test-26.04 .
docker run --rm --network none c-http-server:test-26.04
```

Package versions come from the Ubuntu repository at build time; the base digest alone does not freeze the compiler packages. Record resolved versions with test evidence. This bootstrap does not claim byte-for-byte reproducible builds.

## Run with persistent data

Build the final runtime stage, create a dedicated data volume, and publish the server only on the local machine:

```sh
docker build --target runtime -t c-http-server:local .
docker volume create c-http-server-data
docker run -d --name c-http-server \
  --read-only --cap-drop ALL --security-opt no-new-privileges \
  --memory 128m --cpus 2 --pids-limit 128 \
  --publish 127.0.0.1:8080:8080 \
  --mount type=volume,source=c-http-server-data,target=/data \
  c-http-server:local
curl --fail --request PUT --data-binary 'Hello from the C server!' http://127.0.0.1:8080/hello.txt
curl --fail http://127.0.0.1:8080/hello.txt
docker logs c-http-server
```

The example resource settings are development limits, not measured capacity guarantees. If port 8080 is occupied, change the first port in `127.0.0.1:8080:8080`. The process runs as UID/GID 10001. A newly created volume inherits `/data` ownership; an existing volume or bind directory must be writable by that identity. Never mount source code, credentials, or unrelated host files as the data directory.

`docker stop c-http-server` stops the process. Removing and recreating the container with the same named volume preserves uploaded files; removing the volume deletes them. Do not delete the volume when upgrading an image. A PUT replaces a file through a temporary file and rename; image rollback does not restore older file contents.

## Check the runtime image

The host-side check uses Docker and Python 3.11 or newer. It resolves the supplied image tag to a local immutable image ID, then tests non-root startup, binary PUT/GET, concurrent requests, and persistence across container replacement. It creates and removes only its own temporary containers and volume:

```sh
python3 tests/test_container.py --image c-http-server:local \
  --report test-results/runtime.json
```

This checks the already-built runtime image; it does not rebuild it or use the compiler image as a substitute. Run it for each runtime base you intend to support.

## Native Ubuntu development

On Ubuntu, install the application prerequisites, then build and test from this directory:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends clang make libc6-dev python3 procps
make -j2
make test
mkdir -p data
cd data
../build/httpserver -t 4 8080
```

Python 3.11 or newer is required for the test tools. Run tests as an ordinary user. `make test-regression` and `make test-course` run the suites separately; `make clean` removes compiled outputs. Each support module is compiled into `build/libhttp-support.a`; no imported binary archive is needed. Compiler errors, missing sources, failed tests, and timed-out workload cases return failure.

## Behavior and limits

- Startup: `httpserver [-t threads] <port>`; four workers by default; supported worker counts are 1–1024. Use an unprivileged TCP port. Only one HTTP request is handled per connection.
- GET returns file bytes, 404 for a missing file, or 403 for an unsupported filesystem target. PUT requires Content-Length and returns 201 for creation or 200 for replacement.
- Filenames and HTTP headers follow the original restricted assignment protocol. This is not a general HTTP framework: there is no TLS, authentication, directory browsing, chunked transfer, or keep-alive support. Any client that can reach it can PUT files.
- Keep this development server on loopback or a trusted private network. Its locks can grow with the number of distinct filenames; it has no disk quota or complete defense against slow clients. Existing data permissions and content policy still need to be specified before deployment.
- Termination does not drain active requests. An interrupted upload can leave a hidden temporary file; inspect stale files only while the server is stopped. Rename gives atomic visibility during normal operation, not a guarantee of durability across a host crash. Backups and data recovery are separate work.
- Ubuntu container checks exercise Ubuntu userspace on the Docker host's Linux kernel. They do not validate the actual Ubuntu VM, a different CPU architecture, or the future CI/CD platform.
