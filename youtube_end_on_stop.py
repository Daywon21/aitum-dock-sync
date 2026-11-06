import base64
import hashlib
import http.server
import json
import obspython as obs
import os
import queue
import re
import secrets
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

CONFIG_DIR = os.path.join(os.environ.get("APPDATA", ""), "obs-stream-scripts")
CONFIG_PATH = os.path.join(CONFIG_DIR, "config.json")
AUTH_PATH = os.path.join(CONFIG_DIR, "youtube-auth.json")
_CHANNEL_RSS_RE = re.compile(r"<(?:yt:)?videoId>([^<]+)</(?:yt:)?videoId>")

GOOGLE_AUTH = "https://accounts.google.com/o/oauth2/v2/auth"
GOOGLE_TOKEN = "https://oauth2.googleapis.com/token"
OAUTH_PORT = 17564
OAUTH_CALLBACK_PATH = "/"
OAUTH_TIMEOUT_S = 300.0
YOUTUBE_SCOPES = (
    "openid "
    "https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/userinfo.profile "
    "https://www.googleapis.com/auth/youtube.readonly "
    "https://www.googleapis.com/auth/youtube.force-ssl"
).strip()

_GOOGLE_SECRET_XOR_KEY = 0x5A
_GOOGLE_SECRET_XOR_BYTES = [
    29, 21, 25, 9, 10, 2, 119, 46, 99, 43, 55, 3, 43, 3, 98, 12,
    24, 8, 32, 51, 14, 48, 98, 42, 53, 5, 22, 0, 44, 25, 24, 47, 63, 47, 18,
]
GOOGLE_OAUTH_CLIENT_ID = (
    "121745350741-bqpoja11pqh15rivcbgde2o2gabp9v3h.apps.googleusercontent.com"
)
GOOGLE_OAUTH_CLIENT_SECRET = "".join(
    chr(byte ^ _GOOGLE_SECRET_XOR_KEY) for byte in _GOOGLE_SECRET_XOR_BYTES
)

account_name = ""
channel_id = ""
access_token = ""
refresh_token = ""
token_expires_at = 0.0
disable_auto_stop_on_start = True
_oauth_in_progress = False

_stream_active = False
_cached_live_video_id = ""
_work_queue = queue.Queue()
_result_queue = queue.Queue()
_worker_shutdown = threading.Event()
_worker_thread = None
_poll_timer_registered = False


def script_description():
    return (
        "Ends the YouTube site broadcast when OBS main stream stops. "
        "Sign in with YouTube once in script settings — no API keys to paste."
    )


def script_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_text(
        props, "account_status", _youtube_account_label(), obs.OBS_TEXT_INFO
    )
    obs.obs_properties_add_button(props, "login_youtube", "Login to YouTube", _on_login_clicked)
    obs.obs_properties_add_button(props, "logout_youtube", "Logout", _on_logout_clicked)
    obs.obs_properties_add_bool(
        props,
        "disable_auto_stop_on_start",
        "Disable YouTube enableAutoStop when stream starts",
    )
    return props


def script_defaults(settings):
    obs.obs_data_set_default_bool(settings, "disable_auto_stop_on_start", True)


def script_update(settings):
    global disable_auto_stop_on_start
    disable_auto_stop_on_start = obs.obs_data_get_bool(settings, "disable_auto_stop_on_start")
    _persist_auth_file()


def _youtube_account_label():
    if _oauth_in_progress:
        return "Signing in — complete login in your browser…"
    if account_name and channel_id:
        return f"Signed in: {account_name} ({channel_id})"
    if channel_id:
        return f"Signed in: {channel_id}"
    return "Not signed in — click Login to YouTube"


def _on_login_clicked(props, prop):
    global _oauth_in_progress
    if _oauth_in_progress:
        _log(obs.LOG_INFO, "YouTube login already in progress")
        return True
    if not GOOGLE_OAUTH_CLIENT_ID:
        _log(obs.LOG_ERROR, "YouTube login is not configured in this build")
        return True
    _oauth_in_progress = True
    _log(obs.LOG_INFO, "Opening browser for YouTube login…")
    _enqueue("oauth_login")
    return True


def _on_logout_clicked(props, prop):
    _clear_auth()
    _log(obs.LOG_INFO, "YouTube logout complete")
    return True


def _log(level, msg):
    obs.script_log(level, f"[youtube-end] {msg}")


def _read_json_file(path):
    if not os.path.isfile(path):
        return None
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None


def _write_json_file(path, payload):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)


def _apply_auth_record(record):
    global account_name, channel_id, access_token, refresh_token, token_expires_at
    account_name = str(record.get("account_name") or "").strip()
    channel_id = str(record.get("channel_id") or "").strip()
    access_token = str(record.get("access_token") or "").strip()
    refresh_token = str(record.get("refresh_token") or "").strip()
    try:
        token_expires_at = float(record.get("expires_at") or 0.0)
    except (TypeError, ValueError):
        token_expires_at = 0.0


def _auth_record():
    return {
        "account_name": account_name,
        "channel_id": channel_id,
        "access_token": access_token,
        "refresh_token": refresh_token,
        "expires_at": token_expires_at,
        "disable_auto_stop_on_start": disable_auto_stop_on_start,
    }


def _persist_auth_file():
    if not refresh_token and not access_token and not channel_id:
        return
    _write_json_file(AUTH_PATH, _auth_record())


def _clear_auth():
    global account_name, channel_id, access_token, refresh_token, token_expires_at
    account_name = ""
    channel_id = ""
    access_token = ""
    refresh_token = ""
    token_expires_at = 0.0
    try:
        if os.path.isfile(AUTH_PATH):
            os.remove(AUTH_PATH)
    except OSError:
        pass


def _load_auth():
    record = _read_json_file(AUTH_PATH)
    if isinstance(record, dict):
        _apply_auth_record(record)
        return

    legacy = _read_json_file(CONFIG_PATH)
    if not legacy:
        return
    youtube = legacy.get("youtube") or {}
    if not youtube:
        return
    _apply_auth_record(
        {
            "account_name": youtube.get("accountName") or youtube.get("account_name") or "",
            "channel_id": youtube.get("channelId") or youtube.get("channel_id") or "",
            "access_token": youtube.get("accessToken") or youtube.get("access_token") or "",
            "refresh_token": youtube.get("refreshToken") or youtube.get("refresh_token") or "",
            "expires_at": youtube.get("expiresAt") or youtube.get("expires_at") or 0.0,
        }
    )
    if refresh_token or access_token:
        _persist_auth_file()


def _pkce_pair():
    verifier = base64.urlsafe_b64encode(secrets.token_bytes(32)).decode("ascii").rstrip("=")
    challenge = (
        base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest())
        .decode("ascii")
        .rstrip("=")
    )
    return verifier, challenge


def _http_text(url, headers=None, data=None, method=None):
    request = urllib.request.Request(url, data=data, headers=headers or {}, method=method)
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read().decode("utf-8")


def _http_json(url, headers=None, data=None, method=None):
    return json.loads(_http_text(url, headers=headers, data=data, method=method))


def _wait_for_oauth_redirect(state):
    result = [None]

    class _Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            return

        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            if parsed.path != OAUTH_CALLBACK_PATH:
                self.send_error(404, "Not found")
                return
            query = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)
            flat = {key: (values[0] if values else "") for key, values in query.items()}
            html = (
                b"<!DOCTYPE html><html><body style='font-family:system-ui;padding:2rem'>"
                b"<p>You can return to OBS and close this tab.</p></body></html>"
            )
            if flat.get("error"):
                result[0] = {
                    "ok": False,
                    "error": flat.get("error", "error"),
                    "error_description": flat.get("error_description", ""),
                }
            else:
                code = (flat.get("code") or "").strip()
                callback_state = (flat.get("state") or "").strip()
                if not code:
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()
                    self.wfile.write(html)
                    return
                if callback_state and callback_state != state:
                    self.send_response(200)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()
                    self.wfile.write(html)
                    return
                result[0] = {"ok": True, "code": code, "state": callback_state}
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html)

    try:
        httpd = http.server.HTTPServer(("127.0.0.1", OAUTH_PORT), _Handler)
    except OSError as err:
        raise RuntimeError(f"Could not start login listener on port {OAUTH_PORT}: {err}") from err

    redirect_uri = f"http://127.0.0.1:{OAUTH_PORT}{OAUTH_CALLBACK_PATH}"
    verifier, challenge = _pkce_pair()
    auth_url = (
        f"{GOOGLE_AUTH}?"
        + urllib.parse.urlencode(
            {
                "client_id": GOOGLE_OAUTH_CLIENT_ID,
                "redirect_uri": redirect_uri,
                "response_type": "code",
                "scope": YOUTUBE_SCOPES,
                "state": state,
                "code_challenge": challenge,
                "code_challenge_method": "S256",
                "access_type": "offline",
                "prompt": "consent",
            }
        )
    )

    try:
        webbrowser.open(auth_url)
        httpd.timeout = 0.5
        deadline = time.time() + OAUTH_TIMEOUT_S
        while time.time() < deadline and result[0] is None:
            httpd.handle_request()
    finally:
        try:
            httpd.server_close()
        except Exception:
            pass

    if result[0] is None:
        raise RuntimeError("YouTube login timed out. Try again.")
    if not result[0].get("ok"):
        message = (
            result[0].get("error_description")
            or result[0].get("error")
            or "authorization failed"
        )
        raise RuntimeError(str(message))
    return result[0]["code"], verifier, redirect_uri


def _exchange_youtube_code(code, verifier, redirect_uri):
    body = urllib.parse.urlencode(
        {
            "code": code,
            "client_id": GOOGLE_OAUTH_CLIENT_ID,
            "redirect_uri": redirect_uri,
            "grant_type": "authorization_code",
            "code_verifier": verifier,
            "client_secret": GOOGLE_OAUTH_CLIENT_SECRET,
        }
    ).encode("utf-8")
    payload = _http_json(
        GOOGLE_TOKEN,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        data=body,
        method="POST",
    )
    access = str(payload.get("access_token") or "").strip()
    if not access:
        raise RuntimeError("Google did not return an access token")
    refresh = str(payload.get("refresh_token") or "").strip()
    expires_in = int(payload.get("expires_in") or 0)
    return access, refresh, expires_in


def _fetch_youtube_channel_profile(token):
    url = "https://www.googleapis.com/youtube/v3/channels?" + urllib.parse.urlencode(
        {"part": "id,snippet", "mine": "true", "maxResults": "1"}
    )
    payload = _http_json(url, headers={"Authorization": f"Bearer {token}"})
    items = payload.get("items") or []
    if not items:
        raise RuntimeError("No YouTube channel found for this Google account")
    item = items[0]
    cid = str(item.get("id") or "").strip()
    snippet = item.get("snippet") or {}
    title = str(snippet.get("title") or "").strip()
    handle = str(snippet.get("customUrl") or "").strip().lstrip("@")
    name = title or (f"@{handle}" if handle else cid)
    if not cid:
        raise RuntimeError("YouTube channels.list returned no channel id")
    return name, cid


def _worker_oauth_login():
    global _oauth_in_progress, account_name, channel_id, access_token, refresh_token, token_expires_at
    try:
        state = secrets.token_hex(24)
        code, verifier, redirect_uri = _wait_for_oauth_redirect(state)
        access, refresh, expires_in = _exchange_youtube_code(code, verifier, redirect_uri)
        if not refresh:
            raise RuntimeError(
                "Google did not return a refresh token — revoke app access at "
                "myaccount.google.com/permissions and log in again"
            )
        name, cid = _fetch_youtube_channel_profile(access)
        account_name = name
        channel_id = cid
        access_token = access
        refresh_token = refresh
        token_expires_at = float(int(time.time()) + max(0, expires_in))
        _persist_auth_file()
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_INFO,
                "msg": f"YouTube login complete: {name} ({cid})",
            }
        )
    except Exception as err:
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_WARNING,
                "msg": f"YouTube login failed: {err}",
            }
        )
    finally:
        _oauth_in_progress = False


def _refresh_google_access_token():
    global access_token, refresh_token, token_expires_at
    if not refresh_token:
        raise RuntimeError("Not signed in — click Login to YouTube in script settings")
    body = urllib.parse.urlencode(
        {
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
            "client_id": GOOGLE_OAUTH_CLIENT_ID,
            "client_secret": GOOGLE_OAUTH_CLIENT_SECRET,
        }
    ).encode("utf-8")
    payload = _http_json(
        GOOGLE_TOKEN,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        data=body,
        method="POST",
    )
    token = str(payload.get("access_token") or "").strip()
    if not token:
        raise RuntimeError("Google token refresh returned no access_token")
    access_token = token
    expires_in = int(payload.get("expires_in") or 0)
    token_expires_at = float(int(time.time()) + max(0, expires_in))
    new_refresh = str(payload.get("refresh_token") or "").strip()
    if new_refresh:
        refresh_token = new_refresh
    _persist_auth_file()
    return token


def _ensure_access_token():
    token = str(access_token or "").strip()
    now = time.time()
    if token and token_expires_at and now < token_expires_at - 120:
        return token
    if refresh_token:
        return _refresh_google_access_token()
    raise RuntimeError("Not signed in — click Login to YouTube in script settings")


def _parse_video_ids_from_rss(body, limit=15):
    ids = []
    for match in _CHANNEL_RSS_RE.finditer(body or ""):
        video_id = str(match.group(1) or "").strip()
        if video_id and video_id not in ids:
            ids.append(video_id)
    return ids[:limit]


def _fetch_channel_rss(channel):
    cid = str(channel or "").strip()
    if not cid.startswith("UC"):
        return None
    url = f"https://www.youtube.com/feeds/videos.xml?channel_id={urllib.parse.quote(cid)}"
    return _http_text(url, headers={"User-Agent": "obs-youtube-end/1.0"})


def _videos_list(token, video_ids):
    if not video_ids:
        return []
    url = "https://www.googleapis.com/youtube/v3/videos?" + urllib.parse.urlencode(
        {
            "part": "liveStreamingDetails,snippet",
            "id": ",".join(video_ids),
        }
    )
    payload = _http_json(url, headers={"Authorization": f"Bearer {token}"})
    return payload.get("items") or []


def _find_live_video_from_rss(token):
    body = _fetch_channel_rss(channel_id)
    if not body:
        return None
    video_ids = _parse_video_ids_from_rss(body)
    if not video_ids:
        return None
    for index in range(0, len(video_ids), 50):
        chunk = video_ids[index : index + 50]
        for item in _videos_list(token, chunk):
            live_chat_id = str(
                ((item.get("liveStreamingDetails") or {}).get("activeLiveChatId") or "")
            ).strip()
            if live_chat_id:
                return {
                    "videoId": str(item.get("id") or "").strip(),
                    "title": str(((item.get("snippet") or {}).get("title") or "")).strip(),
                }
    return None


def _is_video_live(token, video_id):
    items = _videos_list(token, [video_id])
    if not items:
        return False
    live_chat_id = str(
        ((items[0].get("liveStreamingDetails") or {}).get("activeLiveChatId") or "")
    ).strip()
    return bool(live_chat_id)


def _disable_broadcast_auto_stop(token, video_id):
    url = "https://www.googleapis.com/youtube/v3/liveBroadcasts?" + urllib.parse.urlencode(
        {"part": "contentDetails"}
    )
    body = json.dumps(
        {"id": video_id, "contentDetails": {"enableAutoStop": False}}
    ).encode("utf-8")
    try:
        _http_json(
            url,
            headers={
                "Authorization": f"Bearer {token}",
                "Content-Type": "application/json",
            },
            data=body,
            method="PUT",
        )
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_INFO,
                "msg": f"enableAutoStop disabled for {video_id}",
            }
        )
    except (urllib.error.HTTPError, urllib.error.URLError, OSError) as err:
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_WARNING,
                "msg": f"enableAutoStop disable failed for {video_id}: {err}",
            }
        )


def _complete_live_broadcast(token, video_id):
    if not _is_video_live(token, video_id):
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_INFO,
                "msg": f"Not live — skip complete: {video_id}",
            }
        )
        return
    url = (
        "https://www.googleapis.com/youtube/v3/liveBroadcasts/transition?"
        + urllib.parse.urlencode(
            {
                "broadcastStatus": "complete",
                "id": video_id,
                "part": "status",
            }
        )
    )
    _http_json(url, headers={"Authorization": f"Bearer {token}"}, method="POST")
    _result_queue.put(
        {
            "type": "log",
            "level": obs.LOG_INFO,
            "msg": f"Broadcast completed on site: {video_id}",
        }
    )
    _result_queue.put({"type": "clear_cached_video"})


def _worker_cache_live_video():
    if not channel_id:
        raise RuntimeError("Not signed in — click Login to YouTube in script settings")
    token = _ensure_access_token()
    live = _find_live_video_from_rss(token)
    if not live:
        _result_queue.put(
            {
                "type": "log",
                "level": obs.LOG_INFO,
                "msg": "No live video found from RSS + videos.list yet",
            }
        )
        _result_queue.put({"type": "set_cached_video", "video_id": ""})
        return
    _result_queue.put(
        {
            "type": "log",
            "level": obs.LOG_INFO,
            "msg": f"Cached live video: {live['videoId']} — {live['title']}",
        }
    )
    _result_queue.put({"type": "set_cached_video", "video_id": live["videoId"]})
    if disable_auto_stop_on_start:
        _disable_broadcast_auto_stop(token, live["videoId"])


def _worker_end_broadcast(video_id):
    token = _ensure_access_token()
    if not video_id:
        if not channel_id:
            raise RuntimeError("Not signed in — click Login to YouTube in script settings")
        live = _find_live_video_from_rss(token)
        if not live:
            _result_queue.put(
                {
                    "type": "log",
                    "level": obs.LOG_INFO,
                    "msg": "No live video to end",
                }
            )
            return
        video_id = live["videoId"]
    _complete_live_broadcast(token, video_id)


def _worker_loop():
    while not _worker_shutdown.is_set():
        try:
            job = _work_queue.get(timeout=0.25)
        except queue.Empty:
            continue
        try:
            if job == "oauth_login":
                _worker_oauth_login()
            elif job == "cache_live":
                _worker_cache_live_video()
            elif isinstance(job, tuple) and job[0] == "end_broadcast":
                _worker_end_broadcast(str(job[1] or "").strip())
        except Exception as err:
            _result_queue.put(
                {
                    "type": "log",
                    "level": obs.LOG_WARNING,
                    "msg": str(err),
                }
            )


def _ensure_worker():
    global _worker_thread
    if _worker_thread and _worker_thread.is_alive():
        return
    _worker_shutdown.clear()
    _worker_thread = threading.Thread(target=_worker_loop, name="youtube-end-worker", daemon=True)
    _worker_thread.start()


def _shutdown_worker():
    global _worker_thread
    _worker_shutdown.set()
    if _worker_thread and _worker_thread.is_alive():
        _worker_thread.join(timeout=2.0)
    _worker_thread = None


def _enqueue(job):
    _ensure_worker()
    _work_queue.put(job)


def _process_results():
    while True:
        try:
            item = _result_queue.get_nowait()
        except queue.Empty:
            break

        item_type = item.get("type")
        if item_type == "log":
            _log(item.get("level", obs.LOG_INFO), item.get("msg", ""))
        elif item_type == "set_cached_video":
            global _cached_live_video_id
            _cached_live_video_id = str(item.get("video_id") or "")
        elif item_type == "clear_cached_video":
            _cached_live_video_id = ""


def _poll_results():
    _process_results()


def _signed_in():
    return bool(refresh_token and channel_id)


def _on_stream_started():
    global _stream_active
    if _stream_active:
        return
    _stream_active = True
    if not _signed_in():
        _log(obs.LOG_WARNING, "Not signed in — open script settings and click Login to YouTube")
        return
    _log(obs.LOG_INFO, "Main stream started — caching live video from RSS")
    _enqueue("cache_live")


def _on_stream_stopped():
    global _stream_active
    if not _stream_active:
        return
    _stream_active = False
    if not _signed_in():
        _log(obs.LOG_WARNING, "Not signed in — cannot end YouTube broadcast")
        return
    _log(obs.LOG_INFO, "Main stream stopped — ending YouTube broadcast on site")
    _enqueue(("end_broadcast", _cached_live_video_id))


def on_frontend_event(event):
    if event == obs.OBS_FRONTEND_EVENT_STREAMING_STARTED:
        _on_stream_started()
        return

    stopping = getattr(obs, "OBS_FRONTEND_EVENT_STREAMING_STOPPING", None)
    if stopping is not None and event == stopping:
        _on_stream_stopped()
        return

    if event == obs.OBS_FRONTEND_EVENT_STREAMING_STOPPED:
        _on_stream_stopped()


def script_load(settings):
    global _stream_active, _cached_live_video_id, _poll_timer_registered
    _stream_active = bool(obs.obs_frontend_streaming_active())
    _cached_live_video_id = ""
    _load_auth()
    script_update(settings)
    _ensure_worker()
    obs.obs_frontend_add_event_callback(on_frontend_event)
    if not _poll_timer_registered:
        obs.timer_add(_poll_results, 200)
        _poll_timer_registered = True
    if _signed_in():
        label = account_name or channel_id
        _log(obs.LOG_INFO, f"Loaded — signed in as {label}")
    else:
        _log(obs.LOG_WARNING, "Loaded — click Login to YouTube in script settings")


def script_unload():
    global _poll_timer_registered
    obs.timer_remove(_poll_results)
    _poll_timer_registered = False
    obs.obs_frontend_remove_event_callback(on_frontend_event)
    _shutdown_worker()
