# HTTP Server

A Linux file service written in C with POSIX threads. It supports GET and PUT for a single filename in its working directory, with a bounded worker queue and per-file reader/writer locks. PUT creates or replaces persistent files. Audit records go to standard error.

This standalone project includes the server, support library, Docker build, and application tests. See the [operations guide](OPERATIONS.md) for deployment requirements and [validation evidence](VALIDATION.md) for measured compatibility. Repository history and contribution instructions are in [CONTRIBUTING.md](CONTRIBUTING.md).

Clone the repository and enter its root before running the commands below:

```sh
git clone https://github.com/melliott18/http-server.git
cd http-server
```

## Layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Server and support library source |
| `Makefile` | Explicit source build, dependency tracking, application test entry points |
| `Dockerfile` | Ubuntu toolchain, build, test, and non-root runtime stages |
| `tests/` | Isolated concurrency workloads and focused HTTP regression tests |
| `build/`, `test-results/`, `data/` | Ignored local outputs and development data |

## Build and test in Docker

Run these commands from the repository root. Docker must be running; no host C compiler or Python installation is needed. Building downloads Ubuntu packages; running the tests needs no external network.

```sh
docker build --target test -t http-server:test .
docker run --name http-server-tests --network none http-server:test
```

The test command exits nonzero on failure. After either success or failure, copy the test reports and remove its disposable container:

```sh
docker cp http-server-tests:/app/test-results ./test-results
docker rm http-server-tests
```

Tests run as UID/GID 10001, in disposable directories, without mounting the repository or real application data. Both suites write JUnit XML and retained logs beneath `test-results/`. Regression results also appear in the container output.

The default Ubuntu 24.04 base image is pinned by digest. To test Ubuntu 26.04, use the same Dockerfile with this explicit base:

```sh
docker build --target test \
  --build-arg UBUNTU_IMAGE=ubuntu:26.04@sha256:513c074113a871b51a8d16ab445c88779d6452d937a164fb5cc479f32668a41d \
  -t http-server:test-26.04 .
docker run --rm --network none http-server:test-26.04
```

Package versions come from the Ubuntu repository at build time; the base digest alone does not freeze the compiler packages. Record resolved versions with test evidence. Record package versions as well as the base image when reproducing a build.

## Run with persistent data

Build the final runtime stage, create a dedicated data volume, and publish the server only on the local machine:

```sh
docker build --target runtime -t http-server:local .
docker volume create http-server-data
docker run -d --name http-server \
  --read-only --cap-drop ALL --security-opt no-new-privileges \
  --memory 128m --cpus 2 --pids-limit 128 \
  --publish 127.0.0.1:8080:8080 \
  --mount type=volume,source=http-server-data,target=/data \
  http-server:local
curl --fail --request PUT --data-binary 'Hello from the C server!' http://127.0.0.1:8080/hello.txt
curl --fail http://127.0.0.1:8080/hello.txt
docker logs http-server
```

The example resource settings are development limits, not measured capacity guarantees. If port 8080 is occupied, change the first port in `127.0.0.1:8080:8080`. The process runs as UID/GID 10001. A newly created volume inherits `/data` ownership; an existing volume or bind directory must be writable by that identity. Never mount source code, credentials, or unrelated host files as the data directory.

`docker stop http-server` stops the process. Removing and recreating the container with the same named volume preserves uploaded files; removing the volume deletes them. Do not delete the volume when upgrading an image. A PUT replaces a file through a temporary file and rename; image rollback does not restore older file contents.

## Check the runtime image

The host-side check uses Docker and Python 3.11 or newer. It resolves the specified image tag to a local immutable image ID, then tests non-root startup, binary PUT/GET, concurrent requests, and persistence across container replacement. It creates and removes only its own temporary containers and volume:

```sh
python3 tests/test_container.py --image http-server:local \
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

Python 3.11 or newer is required for the test tools. Run tests as an ordinary user. `make test-regression` and `make test-workloads` run the suites separately; `make clean` removes compiled outputs. Each support module is compiled into `build/libhttp-support.a`; no precompiled support archive is needed. Compiler errors, missing sources, failed tests, and timed-out workload cases return failure.

## HTTP interface

- Startup: `httpserver [-t threads] <port>`; four workers by default; supported worker counts are 1–1024. Use an unprivileged TCP port. The process listens on all IPv4 interfaces; restrict exposure at the container or host boundary.
- GET returns file bytes, 404 for a missing file, or 403 for a directory, symlink, or other unsupported filesystem target. PUT requires Content-Length and returns 201 for creation or 200 for replacement. Failed or incomplete uploads preserve the existing file.
- Request targets are a single filename of 1–63 ASCII letters, digits, dots, or hyphens. Paths, percent encoding, and query strings are unsupported. The data directory is the process working directory.
- Requests use HTTP/1.1 and CRLF line endings. Header names and values are limited to 128 characters each; the total header section is limited to 2,048 bytes. Headers use `Name: value` syntax. Content-Length and Request-Id names are case insensitive; duplicates are rejected.
- The server handles one request per connection and sends `Connection: close`. It rejects Transfer-Encoding; it has no chunked transfer, keep-alive, TLS, authentication, or directory listing. Any client allowed to reach it can read and replace files.
- Request-Id is optional and defaults to `0` in audit records. It must not contain commas. Standard error receives one comma-separated audit record per dispatched operation: `method,/filename,status,request-id`. A status records the response attempted, not confirmation that the client received every byte.

## Resource and deployment limits

- Accepted sockets have a ten-second inactivity timeout on each blocking read and write. Idle or stalled clients release their workers. A client that keeps transferring can extend the request; this is not a total request deadline.
- The queue holds one pending socket per worker, in addition to active requests, the listening backlog, and at most one connection waiting to enter the queue. File locks exist only while requests reference a filename, including requests waiting on the lock.
- Use a trusted private network or an authenticated reverse proxy. Set a total request deadline, body-size limit, concurrency limit, and a filesystem quota for the intended workload. Container memory limits do not limit persistent volume growth.
- Termination does not drain active requests. An interrupted upload can leave a hidden temporary file. Rename gives atomic visibility during normal operation; it does not guarantee durability across a host crash. See [operations](OPERATIONS.md) for shutdown, backups, and recovery.
- Linux ARM64 is the validated architecture. Ubuntu container checks exercise Ubuntu userspace on the Docker host's Linux kernel; they do not validate the actual Ubuntu VM or another CPU architecture. Verify a new target before release.
