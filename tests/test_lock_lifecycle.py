"""Black-box checks that completed requests do not retain per-filename locks."""

from pathlib import Path


def assert_bounded_lock_memory(case):
    """Exercise enough distinct missing paths to expose a retained-lock registry."""
    def resident_kib():
        case.assertIsNone(case.process.poll(), "Server exited during memory check")
        status = Path(f"/proc/{case.process.pid}/status").read_text()
        for line in status.splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
        case.fail("Linux process status did not report resident memory")

    def missing_requests(start, count):
        for index in range(start, start + count):
            # raw() waits for connection close, after the worker's lock release.
            case.assertEqual(case.request("GET", f"missing-{index}.dat")[0], 404)

    # Warm all workers and allocator arenas before measuring steady-state use.
    warmup = 2048
    measured = 16384
    missing_requests(0, warmup)
    before = resident_kib()
    missing_requests(warmup, measured)
    after = resident_kib()
    case.assertLessEqual(after - before, 2048,
                         f"Resident memory grew by {after - before} KiB across "
                         f"{measured} unique missing files; completed requests "
                         "must release filename locks")
