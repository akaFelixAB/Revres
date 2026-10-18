#!/usr/bin/env python3

# Copyright 2026 Felix Huang

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import signal
import socket
import subprocess
import sys
import time
from typing import NoReturn
from pathlib import Path


HOST = "127.0.0.1"
PORT = 8080
STARTUP_TIMEOUT_SECONDS = 10.0


def fail(message: str) -> NoReturn:
    print(f"[FAIL] {message}", file=sys.stderr)
    raise AssertionError(message)


def wait_for_server() -> None:
    deadline = time.monotonic() + STARTUP_TIMEOUT_SECONDS

    while time.monotonic() < deadline:
        try:
            with socket.create_connection((HOST, PORT), timeout=0.5):
                return
        except OSError:
            time.sleep(0.1)

    fail(f"server did not start within {STARTUP_TIMEOUT_SECONDS} seconds")


def request(raw_target: str) -> tuple[int, bytes]:
    request_data = (
        f"GET {raw_target} HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("ascii")

    with socket.create_connection((HOST, PORT), timeout=5.0) as connection:
        connection.sendall(request_data)

        response = bytearray()
        while True:
            chunk = connection.recv(4096)
            if not chunk:
                break
            response.extend(chunk)

    header_end = response.find(b"\r\n\r\n")
    if header_end == -1:
        fail(f"invalid HTTP response for {raw_target!r}")

    header = bytes(response[:header_end])
    body = bytes(response[header_end + 4:])

    status_line = header.split(b"\r\n", 1)[0]
    try:
        status_code = int(status_line.split(b" ", 2)[1])
    except (IndexError, ValueError) as error:
        fail(f"invalid HTTP status line: {status_line!r}: {error}")

    return status_code, body


def main() -> int:
    if len(sys.argv) != 3:
        print(
            f"usage: {sys.argv[0]} <server-executable> <project-root>",
            file=sys.stderr,
        )
        return 2

    server_executable = Path(sys.argv[1]).resolve()
    project_root = Path(sys.argv[2]).resolve()
    index_file = project_root / "public" / "index.html"

    if not server_executable.is_file():
        fail(f"server executable does not exist: {server_executable}")

    if not index_file.is_file():
        fail(f"index file does not exist: {index_file}")

    # According to the configuration, default.toml,
    # Logs should be written to log/serv.log.
    # Test creates the directory before starting the server to avoid 
    # log initialization failure due to missing directory.
    (project_root / "log").mkdir(exist_ok=True)

    process = subprocess.Popen(
        [str(server_executable)],
        cwd=project_root,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        wait_for_server()
        print("[PASS] server started")

        # Request "/"
        status_code, body = request("/")
        if status_code != 200:
            fail(f"GET / returned HTTP {status_code}, expected 200")

        print("[PASS] GET / returned 200")

        # Check that the body matches public/index.html
        expected_body = index_file.read_bytes()
        if body != expected_body:
            fail("GET / did not return public/index.html")

        print("[PASS] GET / returned public/index.html")

        # Request a missing file, expect 404
        status_code, _ = request("/file-that-does-not-exist.html")
        if status_code != 404:
            fail(
                "GET /file-that-does-not-exist.html returned "
                f"HTTP {status_code}, expected 404"
            )

        print("[PASS] missing file returned 404")

        # Path traversal test: request "/../default.toml", expect 400, 403, or 404
        # Note: Cannot use urllib or requests here because they automatically normalize 
        #       the path and remove "/../", which would prevent us from testing the server's handling 
        #       of path traversal attempts. Instead, use a raw socket to send the request directly.
        # Use a raw socket instead of urllib to avoid the client automatically normalizing /../.
        status_code, body = request("/../default.toml")

        if status_code == 200:
            fail("path traversal request returned HTTP 200")

        if b"[server]" in body or b"Revres" in body:
            fail("path traversal request returned content from default.toml")

        if status_code not in (400, 403, 404):
            fail(
                "path traversal request returned unexpected "
                f"HTTP status {status_code}"
            )

        print("[PASS] path traversal request was blocked")

        print("[PASS] all smoke tests passed")
        return 0

    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)

            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError:
        raise SystemExit(1)