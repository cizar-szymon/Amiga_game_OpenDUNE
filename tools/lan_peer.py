#!/usr/bin/env python3
"""A third (or second) player of a LAN game from the PC, for tests.

Joins the OpenDUNE LAN server (TCP 6820), chooses a free house when the server
has chosen its own, and in the game answers each turn of the server (TURNALL)
with an empty turn of its own - a player that gives no orders, so the server's
lockstep waits for it like for an Amiga.

  python tools/lan_peer.py <server address> [--name PEER] [--seconds 600]

A game over the internet, through the game server (Settlers/server): the
address is the game server's (host[:port]), the game is the one with --code
or the first one waiting there:

  python tools/lan_peer.py <game server>:6810 --relay 0.52 [--code ABCD] [--password PW]
"""
import argparse
import socket
import struct
import sys
import time

PROTOCOL = 1
VERSION = b'0.02'
GAME = 16			# the first type of a frame of the game
F_HELLO, F_BYE, F_PING, F_PONG, F_RELAY = 1, 2, 3, 4, 5
MT_LOBBY, MT_PICK, MT_START, MT_TURN, MT_TURNALL, MT_SUM, MT_QUIT, MT_GONE, MT_MAP, MT_CHAT, MT_PAUSE = 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
HOUSES = {0: 'Harkonnen', 1: 'Atreides', 2: 'Ordos', 0xFF: '-'}


def frame(t, data=b''):
    return bytes([1 + len(data), t]) + data


def read_frame(s):
    head = b''
    while len(head) < 1:
        d = s.recv(1)
        if not d:
            raise SystemExit('game server closed')
        head += d
    body = b''
    while len(body) < head[0]:
        d = s.recv(head[0] - len(body))
        if not d:
            raise SystemExit('game server closed')
        body += d
    return body[0], body[1:]


def relay_connect(address):
    host, _, port = address.partition(':')
    s = socket.create_connection((host, int(port or 6810)), timeout=10)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def relay_join(a):
    """Joins a game on the game server; the socket, paired with its maker."""
    version = 'OpenDUNE-' + a.relay
    code = a.code
    while not code:
        s = relay_connect(a.address)
        s.sendall(frame(F_RELAY, ('LIST %d %s' % (PROTOCOL, version)).encode()))
        while True:
            t, d = read_frame(s)
            line = d.decode('latin-1')
            print('server:', line.replace('\t', ' | '))
            if line.startswith('SESSION ') and not code:
                code = line.split()[1]
            if line == 'END' or line.startswith('ERR'):
                break
        s.close()
        if not code:
            time.sleep(2)
    s = relay_connect(a.address)
    s.sendall(frame(F_RELAY, ('JOIN %d %s %s %s\t%s' % (PROTOCOL, version, code, a.name, a.password)).encode('latin-1')))
    t, d = read_frame(s)
    line = d.decode('latin-1')
    print('server:', line)
    if not line.startswith('PAIRED'):
        raise SystemExit(1)
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('address')
    ap.add_argument('--name', default='PEER')
    ap.add_argument('--seconds', type=int, default=600)
    ap.add_argument('--relay', metavar='VERSION', help='through the game server; the release of the game (0.52)')
    ap.add_argument('--code', default='')
    ap.add_argument('--password', default='')
    a = ap.parse_args()

    if a.relay:
        s = relay_join(a)
    else:
        s = socket.create_connection((a.address, 6820), timeout=10)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    s.sendall(frame(F_HELLO, bytes([PROTOCOL]) + VERSION + b'\0' + a.name.encode() + b'\0'))
    s.settimeout(0.5)
    buf = b''
    me = None
    picked = False
    delay = None
    turns = 0
    replies = []	# chat answers, 3 s later
    end = time.time() + a.seconds
    while time.time() < end:
        try:
            d = s.recv(65536)
            if not d:
                print('server closed')
                break
            buf += d
        except socket.timeout:
            pass
        while replies and replies[0][0] <= time.time():
            s.sendall(frame(GAME + MT_CHAT, bytes([me or 0]) + replies.pop(0)[1].encode('latin-1')))
        while len(buf) >= 2 and len(buf) >= 1 + buf[0]:
            n = buf[0]
            t = buf[1]
            data = buf[2:1 + n]
            buf = buf[1 + n:]
            if t == F_HELLO:
                print('joined', data[1:].split(b'\0'))
            elif t == F_PING:
                s.sendall(frame(F_PONG, data))
            elif t == F_BYE:
                print('bye', data)
                return
            elif t >= GAME:
                mt = t - GAME
                if mt == MT_LOBBY:
                    me = data[0]
                    pos = 3	# me, map, level
                    players = []
                    for slot in range(3):
                        used, house = data[pos], data[pos + 1]
                        name = data[pos + 2:data.index(b'\0', pos + 2)]
                        pos = data.index(b'\0', pos + 2) + 1
                        players.append((used, house, name.decode('latin-1')))
                    print('lobby: me %d map %d level %d' % (me, data[1], data[2]), [(p[2], HOUSES.get(p[1], p[1])) for p in players if p[0]])
                    if not picked and players[0][1] != 0xFF:
                        taken = {p[1] for p in players if p[0]}
                        for h in (2, 1, 0):
                            if h not in taken:
                                s.sendall(frame(GAME + MT_PICK, bytes([h])))
                                picked = True
                                print('pick', HOUSES[h])
                                break
                elif mt == MT_START:
                    seed, delay = struct.unpack('>IB', data[:5])
                    print('start: seed %d delay %d' % (seed, delay))
                elif mt == MT_TURNALL:
                    turn = struct.unpack('>I', data[:4])[0]
                    more = data[4]
                    if delay is not None and more == 0:
                        s.sendall(frame(GAME + MT_TURN, struct.pack('>IB', turn + delay, 0)))
                        turns += 1
                        if turns % 100 == 0:
                            print('turn', turn, 'orders of others', (len(data) - 5) // 10)
                elif mt == MT_QUIT:
                    print('the server ended the game')
                    return
                elif mt == MT_GONE:
                    print('player gone', data[0])
                elif mt == MT_PAUSE:
                    print('pause by %d: %s' % (data[0], 'on' if data[1] else 'off'))
                elif mt == MT_CHAT:
                    # a chat line [slot][text]: printed and answered (tests of the chat)
                    text = data[1:].decode('latin-1')
                    print('chat from %d: %s' % (data[0], text))
                    if text.strip().lower() == 'pause':
                        # as P of a joiner: ask the server to (un)pause
                        s.sendall(frame(GAME + MT_PAUSE, bytes([me or 0])))
                        print('pause asked')
                    if not text.startswith('got '):
                        replies.append((time.time() + 3, ('got ' + text)[:32]))
    s.sendall(frame(GAME + MT_QUIT, b'q'))
    time.sleep(0.2)
    s.close()


if __name__ == '__main__':
    main()
