#!/usr/bin/env python3

import argparse

loads=[]
unloads=[]
events=[]


def add_load(file, uri):
    loads.append('[[events]]')
    loads.append("type = \"LOAD\"")
    loads.append(f'infile=\"{file}"')
    loads.append(f'outfile=\"{uri}"')
    loads.append("")

def add_unload(file):
    unloads.append('[[events]]')
    unloads.append('type = \"UNLOAD\"')
    unloads.append(f'file=\"{file}"')
    unloads.append("")

next_temp=0
def get_temp(file):
    global next_temp
    tmp = f"tmp{next_temp}"
    next_temp += 1
    return tmp


def get_conflict():
    return "conflict"


def add_get(tmp, id):
    events.append('[[events]]')
    events.append('type = \"CREATE\"')
    events.append('method = \"GET\"')
    events.append(f'uri=\"{tmp}"')
    events.append(f'id={id}')
    events.append("")

def add_put(tmp, file, id):
    events.append('[[events]]')
    events.append('type = \"CREATE\"')
    events.append('method = \"PUT\"')
    events.append(f'uri=\"{tmp}"')
    events.append(f'infile=\"{file}"')
    events.append(f'id={id}')
    events.append("")


def add_send(id):
    events.append('[[events]]')
    events.append('type = \"SEND_ALL\"')
    events.append(f'id={id}')
    events.append("")

def add_wait(id):
    events.append('[[events]]')
    events.append('type = \"WAIT\"')
    events.append(f'id={id}')
    events.append("")


def argparser():
    parser = argparse.ArgumentParser(description="Issues a batch of requests.")
    parser.add_argument(
        "-n", "--num", type=int, default=10,
        metavar="num_par", help="Numer of requests to send."
    )

    parser.add_argument(
        "-t", "--types", type=str, choices=["p", "g", "pg"], default="g",
        help="Type of of requests to send."
    )

    parser.add_argument(
        "-c", "--conflict", type=bool, default=False,
        help="Should requests conflict?"
    )

    parser.add_argument(
        'files', metavar='Files', type=str, nargs='+',
        help="list of files."
    )
    return parser.parse_args()


def main():
    args = argparser()
    waits=[]

    for i in range(args.num):
        raw_file=args.files[i % len(args.files)]
        file = None

        if args.conflict:
            file = get_conflict()
        else:
            file = get_temp(raw_file)

        if args.types == "p" or (args.types == "pg" and i % 2 == 0):
            add_put(file, raw_file, i)
            if not args.conflict:
                add_unload(file)
        else:
            if not args.conflict:
                add_load(raw_file, file)
                add_unload(file)
            add_get(file, i)

        add_send(i)
        waits.append(i)

    if args.conflict:
        add_unload(get_conflict())
    for w in waits:
        add_wait(w)

    for load in loads:
        print(load)
    for event in events:
        print(event)
    for unload in unloads:
        print(unload)

if __name__ == "__main__":
    main()
