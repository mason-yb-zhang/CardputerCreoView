"""NavLib PoC: connect to 3dxnlserver via WAMP v1, register a 3dcontroller, push motion.

Mirrors the autobahn.js shim that 3DconnexionJS uses (subprotocol "wamp").
Run: python poc/navlib_poc.py
"""
import io
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

import json
import random
import ssl
import string
import time
import urllib.request

import websocket

HOST = "127.51.68.120"
TYPE_WELCOME = 0
TYPE_PREFIX = 1
TYPE_CALL = 2
TYPE_CALL_RESULT = 3
TYPE_CALL_ERROR = 4
TYPE_EVENT = 8


def newid():
    return "".join(random.choices(string.ascii_letters + string.digits, k=16))


def call(ws, prefixes, proc_uri, *args):
    call_id = newid()
    msg = [TYPE_CALL, call_id, proc_uri]
    msg.extend(args)
    ws.send(json.dumps(msg))
    deadline = time.time() + 5
    while time.time() < deadline:
        try:
            frame = ws.recv()
        except websocket.WebSocketTimeoutException:
            continue
        data = json.loads(frame)
        kind = data[0]
        if kind == TYPE_WELCOME:
            print(f"    welcome: session={data[1]}")
            continue
        if kind == TYPE_PREFIX:
            continue
        if kind == TYPE_EVENT:
            print(f"    event on {data[1]}: {json.dumps(data[2:])[:100]}")
            continue
        if kind == TYPE_CALL_RESULT and data[1] == call_id:
            return data[2:]
        if kind == TYPE_CALL_ERROR and data[1] == call_id:
            raise RuntimeError(f"call error: {data}")
        print(f"    (other frame: {str(data)[:120]})")
    raise TimeoutError(proc_uri)


def main():
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    info = json.loads(urllib.request.urlopen(
        f"https://{HOST}:8181/3dconnexion/nlproxy", timeout=5, context=ctx).read())
    port = info["port"]
    print(f"[1] nlproxy handshake ok: port={port} version={info['version']}")

    ws = websocket.create_connection(
        f"wss://{HOST}:{port}/3dconnexion", sslopt={"cert_reqs": ssl.CERT_NONE},
        subprotocols=["wamp"], timeout=5)
    print("[2] websocket connected")

    # Register prefixes exactly like 3DconnexionJS does after connect.
    ws.send(json.dumps([TYPE_PREFIX, "3dx_rpc", f"wss://{HOST}/3dconnexion#"]))
    ws.send(json.dumps([TYPE_PREFIX, "3dconnexion", f"wss://{HOST}/3dconnexion"]))
    # Drain welcome/ack frames briefly
    ws.settimeout(1)
    try:
        while True:
            frame = ws.recv()
            print(f"    handshake frame: {str(frame)[:100]}")
    except websocket.WebSocketTimeoutException:
        pass
    ws.settimeout(5)
    print("[3] prefixes registered")

    result = call(ws, None, "3dx_rpc:create", "3dconnexion:3dmouse", "1.0")
    connexion = result[0]["connexion"] if result and isinstance(result[0], dict) and result else result
    print(f"[4] 3dmouse created: {connexion}")

    info_msg = {"version": 1.0, "name": "CardputerCreoView-PoC", "rowMajorOrder": False}
    result = call(ws, None, "3dx_rpc:create", "3dconnexion:3dcontroller", connexion, info_msg)
    instance = result[0]["instance"] if result and isinstance(result[0], dict) and result else result
    print(f"[5] 3dcontroller created: instance={instance}")

    resource = f"3dconnexion:3dcontroller/{instance}"
    print(f"[6] resource: {resource}")

    try:
        r = call(ws, None, "3dx_rpc:update", resource, {"count": 2})
        print("[7] update count=2 ok:", r)
    except Exception as e:
        print("[7] update count failed:", e)

    try:
        r = call(ws, None, "3dx_rpc:read", resource, "count")
        print("[8] read count:", r)
    except Exception as e:
        print("[8] read count failed:", e)

    print("[9] pushing motion (ry) for 2s ...")
    for i in range(20):
        value = {"x": 0.0, "y": 0.0, "z": 0.0, "rx": 0.0, "ry": 0.5, "rz": 0.0}
        try:
            call(ws, None, "3dx_rpc:update", resource, value)
        except Exception as e:
            print(f"    update {i} failed: {e}")
            break
        time.sleep(0.1)
    print("[10] motion stream finished")

    ws.close()
    print("[11] closed ok")


if __name__ == "__main__":
    main()
