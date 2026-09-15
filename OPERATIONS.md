# Operating the file service

Use the exact build, test, runtime-image, and container commands in the
[application guide](README.md). The [validation record](VALIDATION.md) names the
environments exercised. Select a tested image and verify it on the intended host
before serving application data.

## Access and workload requirements

The service provides a small HTTP GET/PUT interface. Every reachable client can
read and replace files. Bind the published Docker port to loopback for local use.
For access by other machines, place it behind an authenticated TLS reverse proxy
and restrict direct access to the backend port. Configure the proxy to send
Content-Length, use the documented filename/header syntax, and close backend
connections after each request.

Set admission limits for concurrent requests, upload size, total request duration,
and request rate at the access boundary. The server's ten-second socket timeout
limits inactivity in each blocking I/O call; repeated progress can keep a request
alive. Test those limits with representative clients. Worker counts, volume size,
and container limits require measurement against the intended workload.

## Storage and audit records

- Use a dedicated data directory or volume owned by UID/GID 10001. Only the
  service and trusted maintenance tools should have write access. Do not let
  another process mutate files while requests are running.
- Apply a filesystem quota and monitor available space. PUT stages each upload
  in the data directory, so an overwrite temporarily needs space for both the old
  and new content. Failed uploads are discarded during normal process operation.
- Forward standard error to retained container logs with bounded size and
  retention. Audit records contain the attempted response status; they do not
  certify successful client receipt or durable storage after a power loss.
- Decide access, retention, backup location, and acceptable data loss before
  storing important data. Keep backups outside the disposable container and
  outside Git. Existing source author notices remain in the code; distribution
  licensing is tracked in [decision O05](../../docs/decisions.md#open-decisions-and-evidence).

## Shutdown and recovery

1. Stop admission of new requests at the access boundary. Allow existing clients
   to finish according to the configured request deadline. The process itself
   does not implement graceful request draining.
2. Stop the container using the documented `docker stop c-http-server` command.
   Forced or immediate termination may leave `.httpserver_upload-*` staging
   files. Inspect and remove confirmed abandoned staging files only after the
   service is stopped; never remove the volume as part of a release.
3. Back up or restore the stopped data volume with the selected host backup
   tooling. Record the snapshot time and verify restored file contents on a
   separate volume before resuming writes.
4. Start the verified runtime image with the same data volume and permissions.
   Check a known file with GET, then exercise a disposable PUT/GET through the
   access boundary before reopening traffic.

Atomic rename prevents readers from seeing a partially replaced file during
normal operation. Uploads are not fsynced, so an acknowledged PUT can still be
lost after a host crash. Rolling back an image does not roll back uploaded data.
Choose a backup and durability policy that accounts for this behavior.

## Release acceptance

A production release needs evidence for the actual Linux host/CPU, expected
concurrency and file sizes, admission controls, disk exhaustion behavior, log
retention, shutdown, and backup/restore. The standalone test suites establish
application behavior; the shared manifest, Jenkins workflow, image qualification,
and automated deployment remain tracked under
[#17](https://github.com/melliott18/pipeline/issues/17) and its dependencies.
