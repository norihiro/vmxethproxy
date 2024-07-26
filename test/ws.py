#! /usr/bin/env python3

import argparse
from websockets.sync.client import connect


class Run:
    def __init__(self, url):
        self.ws = connect(url, subprotocols=['ws'])
    def close(self):
        self.ws.close()
        self.ws = None
    def run(self, cmd):
        self.ws.send(cmd)
        if cmd[:2] == 'DT':
            return
        return self.ws.recv(timeout=1)


def main():
    parser = argparse.ArgumentParser(
            prog='ws.py',
            description='websocket client for vmxethproxy',
    )
    parser.add_argument('uri', nargs='?', default='ws://localhost:7681/')
    args = parser.parse_args()

    c = Run(args.uri)
    try:
        while True:
            msg = input()
            res = c.run(msg)
            print(res)
    except (KeyboardInterrupt, EOFError):
        pass
    c.close()


if __name__ == '__main__':
    main()
