#!/usr/bin/env python3
"""Uploads an Android App Bundle to a Google Play track (internal testing by default).

Own design over Google's public documentation of the Play Developer API v3 (edits, bundles.upload, tracks.update,
edits.commit) and of OAuth 2.0 for service accounts (a JWT signed with RS256). Standard library only; the JWT is
signed with the openssl command. Nothing is printed of the key or the token.

    play_upload.py --service-account key.json --package net.tismaservidores.tisma --bundle app.aab [--track internal]
"""
import argparse
import base64
import json
import os
import subprocess
import tempfile
import time
import urllib.parse
import urllib.request

API = "https://androidpublisher.googleapis.com/androidpublisher/v3/applications"
UPLOAD = "https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications"
SCOPE = "https://www.googleapis.com/auth/androidpublisher"


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def token(account: dict) -> str:
    """An access token for the service account (OAuth 2.0 JWT bearer grant)."""
    now = int(time.time())
    header = b64url(json.dumps({"alg": "RS256", "typ": "JWT"}).encode())
    claims = b64url(json.dumps({"iss": account["client_email"], "scope": SCOPE, "aud": account["token_uri"],
                                "iat": now, "exp": now + 3000}).encode())
    signing_input = f"{header}.{claims}".encode()
    with tempfile.NamedTemporaryFile("w", suffix=".pem", delete=False) as k:
        k.write(account["private_key"])
        key_path = k.name
    try:
        os.chmod(key_path, 0o600)
        sig = subprocess.run(["openssl", "dgst", "-sha256", "-sign", key_path], input=signing_input,
                             capture_output=True, check=True).stdout
    finally:
        os.remove(key_path)
    jwt = f"{header}.{claims}.{b64url(sig)}"
    body = urllib.parse.urlencode({"grant_type": "urn:ietf:params:oauth:grant-type:jwt-bearer", "assertion": jwt}).encode()
    with urllib.request.urlopen(urllib.request.Request(account["token_uri"], data=body, method="POST")) as r:
        return json.load(r)["access_token"]


def call(method: str, url: str, auth: str, body=None, content_type="application/json") -> dict:
    data = None
    if body is not None:
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={"Authorization": f"Bearer {auth}", "Content-Type": content_type})
    with urllib.request.urlopen(req, timeout=600) as r:
        text = r.read()
    return json.loads(text) if text else {}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--service-account", required=True)
    ap.add_argument("--package", required=True)
    ap.add_argument("--bundle", required=True)
    ap.add_argument("--track", default="internal")
    ap.add_argument("--status", default="completed", help="completed, or draft for an app not yet published")
    a = ap.parse_args()
    with open(a.service_account) as f:
        account = json.load(f)
    auth = token(account)
    edit = call("POST", f"{API}/{a.package}/edits", auth, {})["id"]
    with open(a.bundle, "rb") as f:
        bundle = call("POST", f"{UPLOAD}/{a.package}/edits/{edit}/bundles?uploadType=media", auth, f.read(),
                      "application/octet-stream")
    code = str(bundle["versionCode"])
    call("PUT", f"{API}/{a.package}/edits/{edit}/tracks/{a.track}", auth,
         {"track": a.track, "releases": [{"versionCodes": [code], "status": a.status}]})
    call("POST", f"{API}/{a.package}/edits/{edit}:commit", auth)
    print(f"Uploaded version code {code} to the {a.track} track")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
