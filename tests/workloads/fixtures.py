"""Generate deterministic transfer payloads in a disposable fixture directory."""

from pathlib import Path


# Distinct lengths and contents expose truncated, mixed, or stale responses.
# Every payload exceeds the workloads' largest 75,000-byte partial transfer.
PAYLOAD_SIZES = {
    "payload-a.txt": 201_936,
    "payload-b.txt": 231_680,
    "payload-c.txt": 246_000,
    "payload-d.txt": 215_200,
    "payload-e.txt": 216_600,
    "payload-large.txt": 421_545,
    **{f"concurrent-put-{index}.txt": 180_000 for index in range(5)},
}


def create_fixtures(directory: Path):
    """Write reproducible ASCII payloads without external files or downloads."""
    directory.mkdir()
    for name, size in PAYLOAD_SIZES.items():
        record = f"HTTP transfer fixture {name}: 0123456789abcdef\n".encode("ascii")
        payload = (record * ((size + len(record) - 1) // len(record)))[:size]
        (directory / name).write_bytes(payload)
