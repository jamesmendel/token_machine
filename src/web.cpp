#include "web.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "tag_db.h"
#include "rfid.h"

namespace Web {

static AsyncWebServer server(80);
static DNSServer dnsServer;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Token Machine Admin</title>
<style>
  :root { --bg:#0f1419; --card:#1a2332; --fg:#e7ecf3; --muted:#8b9bb4; --accent:#3dbbdb; --danger:#e85d5d; --border:#2a3548; }
  * { box-sizing:border-box; }
  body { margin:0; font-family:system-ui,-apple-system,sans-serif; background:var(--bg); color:var(--fg); padding:0.75rem; }
  h1 { font-size:1.2rem; margin:0 0 0.2rem; }
  .sub { color:var(--muted); margin-bottom:0.9rem; font-size:0.85rem; }
  section { background:var(--card); border:1px solid var(--border); border-radius:10px; padding:0.75rem; margin-bottom:0.75rem; }
  h2 { font-size:0.95rem; margin:0 0 0.6rem; color:var(--accent); }
  table { width:100%; border-collapse:collapse; font-size:0.85rem; table-layout:fixed; }
  th,td { text-align:left; padding:0.45rem 0.25rem; border-bottom:1px solid var(--border); vertical-align:middle; }
  th { color:var(--muted); font-weight:600; font-size:0.7rem; text-transform:uppercase; letter-spacing:0.03em; }
  col.uid { width:4.6rem; }
  col.name { width:auto; }
  col.tok { width:3.6rem; }
  col.act { width:4rem; }
  input[type=text],input[type=number] { width:100%; background:#0f1419; border:1px solid var(--border); color:var(--fg); border-radius:6px; padding:0.35rem 0.4rem; font-size:max(0.85rem, 16px); }
  input.tok { text-align:right; }
  .uid { font-family:ui-monospace,monospace; font-size:0.65rem; color:var(--muted); word-break:break-all; line-height:1.2; }
  button { cursor:pointer; border:none; border-radius:6px; padding:0.35rem 0.5rem; font-weight:600; font-size:0.75rem; background:var(--accent); color:#062029; }
  button.secondary { background:#2a3548; color:var(--fg); }
  button.danger { background:var(--danger); color:#fff; }
  button:disabled { opacity:0.5; cursor:default; }
  .acts { display:flex; flex-direction:column; gap:0.25rem; }
  .row { display:flex; flex-wrap:wrap; gap:0.5rem; align-items:center; margin-top:0.5rem; }
  .row input.name { flex:1; min-width:8rem; }
  .msg { margin-top:0.6rem; font-size:0.8rem; color:var(--muted); min-height:1.2em; }
  .msg.err { color:var(--danger); }
  .msg.ok { color:#6bcf8e; }
  .last { font-family:ui-monospace,monospace; font-size:0.75rem; word-break:break-all; }
</style>
</head>
<body>
  <h1>Token Machine</h1>
  <p class="sub">Admin panel</p>

  <section>
    <h2>Register tag</h2>
    <p class="sub" style="margin:0 0 0.5rem">Present a tag to the reader, then enter a name.</p>
    <div class="row">
      <span>Last UID:</span>
      <span class="last" id="lastUid">(none)</span>
      <button type="button" class="secondary" onclick="refreshLast()">Refresh</button>
    </div>
    <div class="row">
      <input class="name" type="text" id="regName" placeholder="Name" maxlength="32"/>
      <button type="button" onclick="registerTag()">Register</button>
    </div>
    <div class="msg" id="regMsg"></div>
  </section>

  <section>
    <h2>Registered tags</h2>
    <p class="sub" id="totalTokens" style="margin:0 0 0.5rem">Team total tokens: 0</p>
    <table>
      <colgroup>
        <col class="uid"/>
        <col class="name"/>
        <col class="tok"/>
        <col class="act"/>
      </colgroup>
      <thead><tr><th>UID</th><th>Name</th><th>Tok</th><th></th></tr></thead>
      <tbody id="tbody"></tbody>
    </table>
    <div class="msg" id="listMsg"></div>
  </section>

<script>
async function api(path, opts) {
  const r = await fetch(path, opts);
  const t = await r.text();
  let j = null;
  try { j = JSON.parse(t); } catch(e) {}
  if (!r.ok) throw new Error((j && j.error) || t || r.statusText);
  return j;
}

function setMsg(id, text, ok) {
  const el = document.getElementById(id);
  el.textContent = text || '';
  el.className = 'msg' + (text ? (ok ? ' ok' : ' err') : '');
}

async function refreshLast() {
  try {
    const j = await api('/api/last_uid');
    document.getElementById('lastUid').textContent = j.uid || '(none)';
  } catch(e) {
    document.getElementById('lastUid').textContent = '(error)';
  }
}

async function loadTags() {
  try {
    const tags = await api('/api/tags');
    const tb = document.getElementById('tbody');
    tb.innerHTML = '';
    tags.forEach(tag => {
      const tr = document.createElement('tr');
      tr.innerHTML =
        '<td class="uid" title="'+tag.uid+'">'+shortUid(tag.uid)+'</td>'+
        '<td><input type="text" value="'+esc(tag.name)+'" data-uid="'+tag.uid+'" data-field="name" maxlength="32"/></td>'+
        '<td><input class="tok" type="number" pattern="[0-9]*" min="0" value="'+tag.tokens+'" data-uid="'+tag.uid+'" data-field="tokens"/></td>'+
        '<td><div class="acts">'+
          '<button type="button" onclick="saveTag(\''+tag.uid+'\', this)">Save</button>'+
          '<button type="button" class="danger" onclick="deleteTag(\''+tag.uid+'\')">Del</button>'+
        '</div></td>';
      tb.appendChild(tr);
    });
    const total = tags.reduce((sum, t) => sum + (t.tokens || 0), 0);
    document.getElementById('totalTokens').textContent = 'Team total tokens: ' + total;
    if (!tags.length) setMsg('listMsg', 'No tags registered yet.', true);
    else setMsg('listMsg', '', true);
  } catch(e) {
    setMsg('listMsg', e.message, false);
  }
}

function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;');
}

function shortUid(uid) {
  if (uid.length <= 8) return uid;
  return uid.slice(0, 4) + '..' + uid.slice(-4);
}

function rowValues(uid) {
  const nameEl = document.querySelector('input[data-uid="'+uid+'"][data-field="name"]');
  const tokEl = document.querySelector('input[data-uid="'+uid+'"][data-field="tokens"]');
  return { name: nameEl.value.trim(), tokens: parseInt(tokEl.value, 10) || 0 };
}

async function saveTag(uid, btn) {
  const v = rowValues(uid);
  btn.disabled = true;
  try {
    await api('/api/tags/'+encodeURIComponent(uid), {
      method:'PUT',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(v)
    });
    setMsg('listMsg', 'Saved '+uid, true);
  } catch(e) {
    setMsg('listMsg', e.message, false);
  }
  btn.disabled = false;
}

async function deleteTag(uid) {
  if (!confirm('Delete '+uid+'?')) return;
  try {
    await api('/api/tags/'+encodeURIComponent(uid), { method:'DELETE' });
    await loadTags();
    setMsg('listMsg', 'Deleted '+uid, true);
  } catch(e) {
    setMsg('listMsg', e.message, false);
  }
}

async function registerTag() {
  const name = document.getElementById('regName').value.trim();
  if (!name) { setMsg('regMsg', 'Enter a name', false); return; }
  try {
    await api('/api/tags', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ name })
    });
    document.getElementById('regName').value = '';
    setMsg('regMsg', 'Registered', true);
    await loadTags();
    await refreshLast();
  } catch(e) {
    setMsg('regMsg', e.message, false);
  }
}

refreshLast();
loadTags();
setInterval(refreshLast, 2000);
</script>
</body>
</html>
)HTML";

static void sendJson(AsyncWebServerRequest* request, int code, const String& body) {
  request->send(code, "application/json", body);
}

static void sendError(AsyncWebServerRequest* request, int code, const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  String out;
  serializeJson(doc, out);
  sendJson(request, code, out);
}

static String uidFromTagsPath(const String& url) {
  // /api/tags/<uid>
  const char* prefix = "/api/tags/";
  if (!url.startsWith(prefix)) return "";
  String uid = url.substring(strlen(prefix));
  int q = uid.indexOf('?');
  if (q >= 0) uid = uid.substring(0, q);
  uid.toLowerCase();
  return uid;
}

static String collectBody(uint8_t* data, size_t len, size_t index, size_t total, String& acc) {
  if (index == 0) {
    acc = "";
    acc.reserve(total);
  }
  for (size_t i = 0; i < len; i++) {
    acc += (char)data[i];
  }
  return acc;
}

bool begin() {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN, false, WIFI_AUTH_WPA2_WPA3_PSK);
  // bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
  if (!ok) {
    ESP_LOGE("WEB", "softAP failed");
    return false;
  }
  IPAddress ip = WiFi.softAPIP();
  ESP_LOGI("WEB", "AP '%s' PASS: %s IP: %s", WIFI_AP_SSID, WIFI_AP_PASS, ip.toString().c_str());
  
  if (!MDNS.begin(MDNS_HOSTNAME)) {
    ESP_LOGE("WEB", "Error setting up MDNS responder!");
      }
  ESP_LOGI("WEB", "mDNS responder started for http://" MDNS_HOSTNAME ".local");

  // Setup routes for captive portal checks
  dnsServer.setTTL(3600);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  if(!dnsServer.start(53, "*", WiFi.softAPIP())) {
    ESP_LOGW("WEB", "Could not start DNS server for captive portal requests.");
  }
  // Modified from https://github.com/CDFER/Captive-Portal-ESP32/blob/main/src/main.cpp
  const String localIPURL = "http://" MDNS_HOSTNAME ".local";
  // Background responses: Probably not all are Required, but some are. Others might speed things up?
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	        // windows 11 captive portal workaround
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								    // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)
  server.on("/generate_204", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		    // android captive portal redirect
  server.on("/redirect", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			    // microsoft redirect
  server.on("/hotspot-detect.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });    // apple call home
  server.on("/canonical.html", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	        // firefox captive portal call home
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					                // firefox captive portal call home
  server.on("/ncsi.txt", [localIPURL](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			    // windows call home

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.onNotFound([&](AsyncWebServerRequest *request){
      ESP_LOGW("WEB", "NOT FOUND: %s - Serving index", request->url());
      request->send(404); 
  });

  server.on("/api/tags", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendJson(request, 200, TagDb::toJsonArray());
  });

  server.on("/api/last_uid", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["uid"] = Rfid::lastUid();
    String out;
    serializeJson(doc, out);
    sendJson(request, 200, out);
  });

  server.on(
    "/api/tags", HTTP_POST,
    [](AsyncWebServerRequest* request) {},
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      static String body;
      collectBody(data, len, index, total, body);
      if (index + len < total) return;

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, body);
      if (err) {
        sendError(request, 400, "invalid json");
        return;
      }

      const char* name = doc["name"] | "";
      if (strlen(name) == 0) {
        sendError(request, 400, "name required");
        return;
      }

      String uid = doc["uid"] | "";
      if (uid.length() == 0) uid = Rfid::lastUid();
      if (uid.length() == 0) {
        sendError(request, 400, "no tag presented");
        return;
      }
      uid.toLowerCase();

      uint32_t tokens = 0;
      String existingName;
      uint32_t existingTokens = 0;
      if (TagDb::get(uid, existingName, existingTokens)) {
        tokens = existingTokens;
      }
      if (!doc["tokens"].isNull()) {
        tokens = doc["tokens"].as<uint32_t>();
      }

      if (!TagDb::upsert(uid, String(name), tokens)) {
        sendError(request, 500, "save failed");
        return;
      }
      sendJson(request, 200, "{\"ok\":true}");
    }
  );

  server.on(
    AsyncURIMatcher::prefix("/api/tags/"), HTTP_PUT,
    [](AsyncWebServerRequest* request) {},
    nullptr,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      static String body;
      collectBody(data, len, index, total, body);
      if (index + len < total) return;

      String uid = uidFromTagsPath(request->url());
      if (uid.length() == 0 || !TagDb::exists(uid)) {
        sendError(request, 404, "not found");
        return;
      }

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, body);
      if (err) {
        sendError(request, 400, "invalid json");
        return;
      }

      String name;
      uint32_t tokens = 0;
      TagDb::get(uid, name, tokens);
      if (!doc["name"].isNull()) name = doc["name"].as<const char*>();
      if (!doc["tokens"].isNull()) tokens = doc["tokens"].as<uint32_t>();

      if (!TagDb::upsert(uid, name, tokens)) {
        sendError(request, 500, "save failed");
        return;
      }
      sendJson(request, 200, "{\"ok\":true}");
    }
  );

  server.on(AsyncURIMatcher::prefix("/api/tags/"), HTTP_DELETE, [](AsyncWebServerRequest* request) {
    String uid = uidFromTagsPath(request->url());
    if (!TagDb::remove(uid)) {
      sendError(request, 404, "not found");
      return;
    }
    sendJson(request, 200, "{\"ok\":true}");
  });

  server.begin();
  return true;
}

void handleClient() {
  dnsServer.processNextRequest();
  // AsyncWebServer runs in the background; nothing to poll.
}

}  // namespace Web
