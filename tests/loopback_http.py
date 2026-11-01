#!/usr/bin/env python3
"""Offline host integration: real static ELF against loopback-only HTTP fixtures.
Test harness uses Python stdlib; BrowserRaf production binary does not."""
import socket
import subprocess
import sys
import threading
import time

BIN = sys.argv[1]


def case(name, response, expected_success):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)
    srv.settimeout(5)
    port = srv.getsockname()[1]
    problems = []

    def serve():
        try:
            conn, _ = srv.accept()
            with conn:
                conn.settimeout(4)
                request = bytearray()
                while b"\r\n\r\n" not in request and len(request) < 2048:
                    data = conn.recv(2048)
                    if not data:
                        break
                    request.extend(data)
                if not request.startswith(b"GET /probe HTTP/1.1\r\n"):
                    problems.append("invalid GET wire syntax")
                if ("Host: 127.0.0.1:%d\r\n" % port).encode("ascii") not in request:
                    problems.append("Host header omitted nondefault port")
                if b"\x00" in request:
                    problems.append("NUL leaked into HTTP request")
                conn.sendall(response)
        except Exception as exc:
            problems.append(str(exc))
        finally:
            srv.close()

    t = threading.Thread(target=serve, daemon=True)
    t.start()
    try:
        p = subprocess.run([BIN, "http://127.0.0.1:%d/probe" % port],
                           capture_output=True, timeout=8, check=False)
    finally:
        t.join(timeout=6)
    out = (p.stdout + p.stderr).decode("utf-8", errors="replace")
    if b"GET /probe HTTP/1.1" in p.stderr:
        problems.append("private HTTP request echoed to stderr")
    if name == "text_plain" and "<script>X</script>" not in out:
        problems.append("text/plain was parsed as HTML rather than literal text")
    good = "[OK] Fetch completo" in out
    if problems or good != expected_success or t.is_alive():
        raise AssertionError("%s expected_success=%s got=%s; server=%s; output=%s" %
                             (name, expected_success, good, problems, out[-1800:]))
    print("PASS_LOOPBACK %s expected_success=%s" % (name, expected_success))


def redirect_same_origin(use_direct=False):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(2)
    srv.settimeout(6)
    port = srv.getsockname()[1]
    problems = []

    def serve():
        try:
            for i, expected in enumerate(("/probe", "/redirected?ok=1")):
                conn, _ = srv.accept()
                with conn:
                    conn.settimeout(4)
                    request = bytearray()
                    while b"\r\n\r\n" not in request and len(request)<2048:
                        data=conn.recv(2048)
                        if not data:break
                        request.extend(data)
                    if not request.startswith(("GET "+expected+" HTTP/1.1\r\n").encode()):
                        problems.append("unexpected redirect path %d: %r" % (i,request[:80]))
                    if i==0:
                        conn.sendall(b"HTTP/1.1 302 Found\r\nLocation: /redirected?ok=1\r\nContent-Length: 0\r\n\r\n")
                    else:
                        conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nOK-LOCAL!")
        except Exception as exc:
            problems.append(str(exc))
        finally:
            srv.close()

    t=threading.Thread(target=serve,daemon=True)
    t.start()
    try:
        cmd=[BIN,"http://%s:%d/probe" % ("localhost" if use_direct else "127.0.0.1",port)]
        if use_direct: cmd.append("--connect-ip=127.0.0.1")
        p=subprocess.run(cmd,capture_output=True,timeout=12,check=False)
    finally:
        t.join(timeout=6)
    out=(p.stdout+p.stderr).decode(errors="replace")
    if problems or t.is_alive() or "[OK] Fetch completo" not in out or "[REDIRECT]" not in out:
        raise AssertionError("redirect_same_origin: problems=%r output=%s" %(problems,out[-1600:]))
    print("PASS_LOOPBACK same-origin 302 redirect with 2 real requests" +
          (" pinned-direct-ip" if use_direct else ""))

def interactive_navigation():
    srv=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    srv.bind(("127.0.0.1",0))
    srv.listen(2)
    srv.settimeout(6)
    port=srv.getsockname()[1]
    problems=[]
    def serve():
        try:
            for i,wanted in enumerate(("/probe","/second?x=1&y=2")):
                conn,_=srv.accept()
                with conn:
                    conn.settimeout(4)
                    req=bytearray()
                    while b"\r\n\r\n" not in req and len(req)<2048:
                        data=conn.recv(2048)
                        if not data:break
                        req.extend(data)
                    if not req.startswith(("GET "+wanted+" HTTP/1.1\r\n").encode()):
                        problems.append("wrong interactive path: "+repr(req[:90]))
                    if i==0:
                        html=b"<html><a href='/second?x=1&amp;y=2'>follow</a></html>"
                        conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: "+str(len(html)).encode()+b"\r\n\r\n"+html)
                    else:
                        conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nNEXT-PAGE")
        except Exception as exc:
            problems.append(str(exc))
        finally:
            srv.close()
    t=threading.Thread(target=serve,daemon=True)
    t.start()
    try:
        p=subprocess.run([BIN,"http://127.0.0.1:%d/probe"%port,"--interactive"],
                         input=b"1\nq\n",capture_output=True,timeout=12,check=False)
    finally:
        t.join(timeout=6)
    out=(p.stdout+p.stderr).decode(errors="replace")
    if problems or t.is_alive() or "NEXT-PAGE" not in out or "Links disponíveis" not in out:
        raise AssertionError("interactive_navigation failed %r %s" % (problems,out[-1800:]))
    print("PASS_LOOPBACK interactive HTML link selection with encoded ampersand")

def content_length_without_eof():
    srv=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    srv.bind(("127.0.0.1",0));srv.listen(1);srv.settimeout(5)
    port=srv.getsockname()[1]
    problems=[]
    def serve():
        try:
            conn,_=srv.accept()
            with conn:
                conn.settimeout(5)
                while True:
                    data=conn.recv(2048)
                    if not data or b"\r\n\r\n" in data:break
                conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nOK-LOCAL!")
                time.sleep(3.5)  # Peer intentionally withholds EOF after full body.
        except Exception as exc:problems.append(str(exc))
        finally:srv.close()
    t=threading.Thread(target=serve,daemon=True);t.start()
    try:
        start=time.monotonic()
        p=subprocess.run([BIN,"http://127.0.0.1:%d/probe"%port],
                         capture_output=True,timeout=7,check=False)
        elapsed=time.monotonic()-start
    finally:t.join(timeout=6)
    out=(p.stdout+p.stderr).decode(errors="replace")
    if problems or t.is_alive() or "[OK] Fetch completo" not in out or elapsed>=2.5:
        raise AssertionError("Content-Length response waited for EOF: elapsed %.2fs %s"%
                             (elapsed,out[-900:]))
    print("PASS_LOOPBACK Content-Length completes before peer EOF (%.2fs)"%elapsed)

case("content_length", b"HTTP/1.1 200 OK\r\nContent-Length: 9\r\nConnection: close\r\n\r\nOK-LOCAL!", True)
case("chunked", b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", True)
case("duplicate_length", b"HTTP/1.1 200 OK\r\nContent-Length: 3\r\ncontent-length: 9\r\n\r\nBAD", False)
case("unsupported_encoding", b"HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\nBAD", False)
case("duplicate_encoding", b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\ntransfer-encoding: chunked\r\n\r\n0\r\n\r\n", False)
case("http_404", b"HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\n\r\nBAD", False)
case("cross_origin_redirect", b"HTTP/1.1 302 Found\r\nLocation: //example.org/\r\nContent-Length: 0\r\n\r\n", False)
redirect_same_origin()
redirect_same_origin(use_direct=True)
interactive_navigation()
content_length_without_eof()
case("binary_mime", b"HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: 8\r\n\r\nRAW-DATA", False)
case("text_plain", b"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 18\r\n\r\n<script>X</script>", True)
# Malformed pin, unsafe reroute and pin+interactive must fail before networking.
for extra in [
    ["http://example.org/","--connect-ip=127.0.0.1"],
    ["http://example.org/","--connect-ip=999.0.0.1"],
    ["http://localhost/","--connect-ip=127.0.0.1","--interactive"],
    ["https://example.org/","--connect-ip=93.184.216.34"],
]:
    result=subprocess.run([BIN]+extra,capture_output=True,timeout=3,check=False)
    output=(result.stdout+result.stderr).decode(errors="replace")
    if "[ERRO]" not in output or "[OK] Fetch completo" in output:
        raise AssertionError("invalid direct-route accepted: " + repr(extra))
print("PASS_SCOPED 15 local HTTP sessions including pinned redirect; four route-negative cases")
