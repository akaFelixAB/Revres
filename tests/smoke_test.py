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

import dataclasses
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import NoReturn


HOST = "127.0.0.1"
PORT = 8080
STARTUP_TIMEOUT_SECONDS = 10.0
REQUEST_TIMEOUT_SECONDS = 5.0


@dataclasses.dataclass
class HttpResponse:
    status: int
    headers: dict[bytes, list[bytes]]
    body: bytes


def fail(message: str) -> NoReturn:
    print(f"[FAIL] {message}", file=sys.stderr)
    raise AssertionError(message)


def parse_headers(raw_headers: bytes) -> dict[bytes, list[bytes]]:
    headers: dict[bytes, list[bytes]] = {}

    for line in raw_headers.split(b"\r\n")[1:]:
        if not line:
            continue

        name, _, value = line.partition(b":")
        headers.setdefault(
            name.strip().lower(),
            [],
        ).append(value.strip())

    return headers


def request(
    raw_target: str,
    method: str = "GET",
) -> HttpResponse:
    request_data = (
        f"{method} {raw_target} HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("latin-1")

    with socket.create_connection(
        (HOST, PORT),
        timeout=REQUEST_TIMEOUT_SECONDS,
    ) as connection:
        connection.sendall(request_data)

        response = bytearray()

        while True:
            chunk = connection.recv(4096)

            if not chunk:
                break

            response.extend(chunk)

    data = bytes(response)
    header_end = data.find(b"\r\n\r\n")

    if header_end == -1:
        fail(
            f"invalid HTTP response for {method} {raw_target!r}; "
            f"received {data!r}"
        )

    raw_header = data[:header_end]
    body = data[header_end + 4:]
    status_line = raw_header.split(b"\r\n", 1)[0]

    try:
        status = int(status_line.split(b" ", 2)[1])
    except (IndexError, ValueError) as error:
        fail(
            f"invalid HTTP status line for {method} {raw_target!r}: "
            f"{status_line!r}: {error}"
        )

    return HttpResponse(
        status=status,
        headers=parse_headers(raw_header),
        body=body,
    )


def expect_status(
    response: HttpResponse,
    expected: int,
    label: str,
) -> None:
    if response.status != expected:
        fail(
            f"{label}: expected HTTP {expected}, "
            f"got HTTP {response.status}"
        )


def expect_header(
    response: HttpResponse,
    name: bytes,
    expected: bytes,
    label: str,
) -> None:
    values = response.headers.get(name, [])

    if expected not in values:
        fail(
            f"{label}: expected {name.decode()} {expected.decode()!r}, "
            f"got {[value.decode(errors='replace') for value in values]}"
        )


def expect_body(
    response: HttpResponse,
    expected: bytes,
    label: str,
) -> None:
    if response.body != expected:
        fail(
            f"{label}: response body does not match expected content; "
            f"got {len(response.body)} bytes, "
            f"expected {len(expected)} bytes"
        )


def expect_content_length(
    response: HttpResponse,
    expected: int,
    label: str,
) -> None:
    values = response.headers.get(b"content-length", [])

    if not values:
        fail(f"{label}: missing Content-Length")

    try:
        actual = int(values[0])
    except ValueError as error:
        fail(
            f"{label}: invalid Content-Length {values[0]!r}: {error}"
        )

    if actual != expected:
        fail(
            f"{label}: Content-Length is {actual}, expected {expected}"
        )


def prepare_static_fixtures(public_dir: Path) -> dict[str, bytes]:
    fixtures = {
        "assets/style.css": b"body { color: rebeccapurple; }\n",
        "assets/logo.png": bytes(range(256)),
        "docs/index.html": b"<html><body>docs index</body></html>\n",
    }

    for relative_path, content in fixtures.items():
        path = public_dir / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    (public_dir / "empty-dir").mkdir(parents=True, exist_ok=True)

    return fixtures


def cleanup_test_files(public_dir: Path, secret_file: Path) -> None:
    # Delete files created by prepare_static_fixtures()
    for relative_path in (
        "assets/style.css",
        "assets/logo.png",
        "docs/index.html",
    ):
        (public_dir / relative_path).unlink(missing_ok=True)

    # Delete empty directories created by the test
    for relative_dir in (
        "assets",
        "docs",
        "empty-dir",
    ):
        path = public_dir / relative_dir

        if path.is_dir():
            path.rmdir()

    # Delete the traversal-test secret file
    secret_file.unlink(missing_ok=True)


def test_index_and_directories(public_dir: Path) -> None:
    index_body = (public_dir / "index.html").read_bytes()

    response = request("/")
    expect_status(response, 200, "GET /")
    expect_body(response, index_body, "GET /")
    expect_content_length(
        response,
        len(index_body),
        "GET /",
    )
    print("[PASS] GET / serves index.html")

    docs_index_body = (
        public_dir / "docs/index.html"
    ).read_bytes()

    response = request("/docs/")
    expect_status(response, 200, "GET /docs/")
    expect_body(response, docs_index_body, "GET /docs/")
    expect_content_length(
        response,
        len(docs_index_body),
        "GET /docs/",
    )
    print("[PASS] GET /docs/ serves docs/index.html")

    # Current server policy: a directory without a trailing slash is 404.
    # If you implement a redirect later, change this to expect 301 and
    # assert Location: /docs/.
    response = request("/docs")
    expect_status(response, 404, "GET /docs")
    print("[PASS] GET /docs returns 404 for directory without trailing slash")

    response = request("/empty-dir/")
    expect_status(response, 404, "GET /empty-dir/")
    print("[PASS] GET /empty-dir/ returns 404 when index is missing")


def test_static_files(
    fixtures: dict[str, bytes],
) -> None:
    response = request("/assets/style.css")
    expect_status(response, 200, "GET /assets/style.css")
    expect_body(
        response,
        fixtures["assets/style.css"],
        "GET /assets/style.css",
    )
    expect_content_length(
        response,
        len(fixtures["assets/style.css"]),
        "GET /assets/style.css",
    )
    expect_header(
        response,
        b"content-type",
        b"text/css",
        "GET /assets/style.css",
    )
    print("[PASS] GET /assets/style.css serves exact file")

    response = request("/assets/logo.png")
    expect_status(response, 200, "GET /assets/logo.png")
    expect_body(
        response,
        fixtures["assets/logo.png"],
        "GET /assets/logo.png",
    )
    expect_content_length(
        response,
        len(fixtures["assets/logo.png"]),
        "GET /assets/logo.png",
    )
    expect_header(
        response,
        b"content-type",
        b"image/png",
        "GET /assets/logo.png",
    )
    print("[PASS] GET /assets/logo.png serves exact binary file")

    response = request("/assets/style.css?v=123")
    expect_status(response, 200, "GET /assets/style.css?v=123")
    expect_body(
        response,
        fixtures["assets/style.css"],
        "GET /assets/style.css?v=123",
    )
    print("[PASS] query string does not affect static file lookup")


def assert_not_secret(
    response: HttpResponse,
    label: str,
) -> None:
    if response.status == 200:
        fail(f"{label}: unexpectedly returned HTTP 200")

    if b"TOP SECRET" in response.body:
        fail(f"{label}: leaked secret file contents")

    if response.status not in (400, 403, 404):
        fail(
            f"{label}: unexpected HTTP status {response.status}; "
            "expected 400, 403, or 404"
        )


def test_path_traversal() -> None:
    traversal_targets = [
        "/../secret.txt",
        "/../../secret.txt",
        "/%2e%2e/secret.txt",
        "/%2e%2e%2fsecret.txt",
        "/..%2fsecret.txt",
        "/..%5csecret.txt",
        "/%252e%252e/secret.txt",
        "//../secret.txt",
        "/./../secret.txt",
    ]

    for target in traversal_targets:
        response = request(target)
        assert_not_secret(response, f"GET {target}")
        print(f"[PASS] traversal blocked: {target}")


def test_http_semantics() -> None:
    target = "/assets/style.css"
    expected_body = b"body { color: rebeccapurple; }\n"

    get_response = request(target, method="GET")
    expect_status(get_response, 200, f"GET {target}")
    expect_body(get_response, expected_body, f"GET {target}")
    expect_content_length(
        get_response,
        len(expected_body),
        f"GET {target}",
    )

    head_response = request(target, method="HEAD")
    expect_status(head_response, 200, f"HEAD {target}")
    expect_body(head_response, b"", f"HEAD {target}")
    expect_content_length(
        head_response,
        len(expected_body),
        f"HEAD {target}",
    )
    print("[PASS] HEAD returns no body and correct Content-Length")

    post_response = request(target, method="POST")
    expect_status(post_response, 405, f"POST {target}")
    expect_header(
        post_response,
        b"allow",
        b"GET, HEAD",
        f"POST {target}",
    )
    print("[PASS] POST returns 405 with Allow: GET, HEAD")


def wait_for_server() -> None:
    deadline = time.monotonic() + STARTUP_TIMEOUT_SECONDS

    while time.monotonic() < deadline:
        try:
            with socket.create_connection(
                (HOST, PORT),
                timeout=0.5,
            ):
                return
        except OSError:
            time.sleep(0.1)

    fail(
        f"server did not start within "
        f"{STARTUP_TIMEOUT_SECONDS} seconds"
    )


def main() -> int:
    if len(sys.argv) != 3:
        print(
            f"usage: {sys.argv[0]} <server-executable> <project-root>",
            file=sys.stderr,
        )
        return 2

    server_executable = Path(sys.argv[1]).resolve()
    project_root = Path(sys.argv[2]).resolve()
    public_dir = project_root / "public"
    index_file = public_dir / "index.html"
    secret_file = project_root / "secret.txt"
    server_log = project_root / "log" / "smoke-server.log"

    if not server_executable.is_file():
        fail(f"server executable does not exist: {server_executable}")

    if not index_file.is_file():
        fail(f"index file does not exist: {index_file}")

    server_log.parent.mkdir(parents=True, exist_ok=True)

    secret_file.write_bytes(b"TOP SECRET\n")

    with server_log.open("wb") as log_handle:
        process = subprocess.Popen(
            [str(server_executable)],
            cwd=project_root,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
        )

        try:
            wait_for_server()
            print("[PASS] server started")

            fixtures = prepare_static_fixtures(public_dir)

            test_index_and_directories(public_dir)
            test_static_files(fixtures)
            test_path_traversal()
            test_http_semantics()

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

            cleanup_test_files(public_dir, secret_file)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError:
        raise SystemExit(1)