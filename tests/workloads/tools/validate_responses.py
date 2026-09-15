#!/usr/bin/env python3
"""Replay GET/PUT operations in audit order and compare response bodies."""

from functools import reduce
import argparse, json, logging, os, sys, tomllib, shutil
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(
        description="""
        Replay workload requests in audit order and compare the expected
        status codes and response bodies with the client's captured responses.
        """
    )
    parser.add_argument(
        "-x", "--audit-log", type=argparse.FileType("r"),
        help="audit log to validate"
    )
    parser.add_argument(
        "-y", "--workload", type=argparse.FileType("r"),
        help="TOML workload that produced the audit log"
    )
    parser.add_argument(
        "-r", "--replay-dir", type=str, default="replay",
        help="directory to replay responses in"
    )
    parser.add_argument(
        "-d", "--response-dir", type=str, default="responses",
        help="directory where responses where stored"
    )
    parser.add_argument(
        "-l", "--logging", default="error",
        choices=['debug', 'info', 'warning', 'error', 'critical', 'off'],
        help="set logging level for logs to stderr (default: error)"
    )
    args = parser.parse_args()

    shutil.rmtree(args.replay_dir, ignore_errors=True)
    Path(args.replay_dir).mkdir(parents=True)
    Path(args.response_dir).mkdir(parents=True, exist_ok=True)

    if args.logging == "off":
        logging.disable(logging.CRITICAL)
    else:
        logging.basicConfig(
            stream=sys.stderr, level=args.logging.upper(),
            format='[%(levelname)s] %(message)s'
        )

    return args

class Operation:
    def __init__(self, type, uri, status, rid):
        self.type = type
        self.uri = uri
        self.status = int(status)
        self.rid = int(rid)

    def __str__(self):
        return self.__repr__()

    def __repr__(self):
        name = type(self).__name__
        serialized = json.dumps(self, default=lambda x: vars(x), indent=4)
        return f"{name} {serialized}"

def parse_op_line(line):
    type, uri, status, rid = line.strip().split(",")
    return Operation(type, uri, status, rid)

class Event:
    def __init__(self, fields):
        self.fields = fields

    def __str__(self):
        return repr(self.fields)

    def __repr__(self):
        return f"Event {json.dumps(self, default=lambda x: vars(x), indent=4)}"

    def replay(self, replay_dir):
        pass

class Load(Event):
    def replay(self, replay_dir):
        infile = self.fields["infile"]
        outfile = self.fields["outfile"]
        shutil.copyfile(infile, outfile)
        logging.info(f"LOAD: {infile} to {outfile}")

class Unload(Event):
    def replay(self, replay_dir):
        file = self.fields["file"]
        Path(file).unlink(missing_ok=True)
        logging.info(f"UNLOAD: {file}")

class Sleep(Event):
    def replay(self, replay_dir):
        seconds = self.fields["seconds"]
        logging.info(f"SLEEP: {seconds} seconds")

class Connect(Event):
    def get(self, response):
        rid = self.fields["id"]
        uri = self.fields["uri"]
        if not os.path.exists(uri):
            with open(response, "w") as f:
                print("Not Found", flush=True, file=f)
        else:
            shutil.copyfile(uri, response)
        logging.info(f"CREATE: GET {uri}")

    def put(self, response):
        rid = self.fields["id"]
        infile = self.fields["infile"]
        uri = self.fields["uri"]
        with open(response, "w") as f:
            if not os.path.exists(uri):
                print("Created", flush=True, file=f)
            else:
                print("OK", flush=True, file=f)
        shutil.copyfile(infile, uri)
        logging.info(f"CREATE: PUT {infile} to {uri}")

    def replay(self, replay_dir):
        rid = self.fields["id"]
        method = self.fields["method"]
        response = os.path.join(replay_dir, f"{rid}-out")
        exists = Path(self.fields["uri"]).exists()
        expected = (200 if exists else 404) if method == "GET" else (200 if exists else 201)
        if method not in ("GET", "PUT"):
            expected = 501
        if self.fields["audit_status"] != expected:
            raise ValueError(f"Request {rid}: expected status {expected}, observed {self.fields['audit_status']}")
        if method == "GET":
            self.get(response)
        elif method == "PUT":
            self.put(response)
        else:
            Path(response).write_text("Not Implemented\n")

class SendLine(Event):
    pass

class SendHeaders(Event):
    pass

class SendAll(Event):
    pass

class SendBody(Event):
    pass

class Received(Event):
    pass

class Wait(Event):
    pass

class Misc(Event):
    pass

def identify_event(fields):
    event = None
    if fields["type"] == "LOAD":
        event = Load(fields)
    elif fields["type"] == "UNLOAD":
        event = Unload(fields)
    elif fields["type"] == "SLEEP":
        event = Sleep(fields)
    elif fields["type"] == "CREATE":
        event = Connect(fields)
    elif fields["type"] == "SEND_LINE":
        event = SendLine(fields)
    elif fields["type"] == "SEND_HEADERS":
        event = SendHeaders(fields)
    elif fields["type"] == "SEND_BODY":
        event = SendBody(fields)
    elif fields["type"] == "SEND_ALL":
        event = SendAll(fields)
    elif fields["type"] == "RECEIVED":
        event = Received(fields)
    elif fields["type"] == "WAIT":
        event = Wait(fields)
    else:
        event = Misc(fields)
    return event

def parse_events_toml(infile):
    loads = []
    unloads = []
    requests = {}

    for fields in tomllib.loads(infile.read())["events"]:
        if fields["type"] == "LOAD":
            loads.append(Load(fields))
        elif fields["type"] == "UNLOAD":
            unloads.append(Unload(fields))
        elif fields["type"] == "SLEEP":
            continue
        else:
            rid = int(fields["id"])
            if rid in requests:
                requests[rid].append(identify_event(fields))
            else:
                requests[rid] = [identify_event(fields)]

    return loads, unloads, requests

def validate_responses(replay_dir, response_dir, requests):
    status = 0
    for rid in requests.keys():
        replayed = os.path.join(replay_dir, f"{rid}-out")
        responded = os.path.join(response_dir, f"{rid}-out")
        if (not Path(replayed).is_file() or not Path(responded).is_file()
                or Path(replayed).read_bytes() != Path(responded).read_bytes()):
            status = 1
            logging.error(f"VALIDATE: {replayed} differs from {responded}")
        else:
            logging.info(f"VALIDATE: {replayed} matches {responded}")
    return status

def main():
    args = parse_args()
    loads, unloads, requests = parse_events_toml(args.workload)
    ops = [parse_op_line(line) for line in args.audit_log.readlines()]
    for op in ops:
        for event in requests[op.rid]:
            event.fields["audit_status"] = op.status
    events = loads + reduce(lambda a, b: a + requests[b.rid], ops, []) + unloads
    for event in events: event.replay(args.replay_dir)
    status = validate_responses(args.replay_dir, args.response_dir, requests)
    sys.exit(status)

if __name__ == "__main__":
    main()
