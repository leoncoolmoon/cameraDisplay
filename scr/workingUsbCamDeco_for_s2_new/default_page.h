#pragma once

const char *DEFAULT_INDEX_HTML = R"rawhtml(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Camera</title>
<style>
  :root { --bg:#0d0d0d; --fg:#e8e8e8; --accent:#00e5ff; --dim:#444; }
  * { box-sizing:border-box; margin:0; padding:0; }
  body { background:var(--bg); color:var(--fg); font-family:monospace;
         display:flex; flex-direction:column; align-items:center;
         min-height:100vh; padding:16px; gap:12px; }
  h1 { font-size:1rem; letter-spacing:.2em; color:var(--accent);
       text-transform:uppercase; margin-top:8px; }
  #stream-wrap { position:relative; width:100%; max-width:640px; }
  #stream { width:100%; display:block; border:1px solid var(--dim); }
  #overlay { display:none; position:absolute; inset:0;
             background:rgba(0,0,0,.85); color:var(--accent);
             font-size:.9rem; align-items:center; justify-content:center;
             text-align:center; letter-spacing:.05em; padding:16px; }
  #overlay.show { display:flex; }
  section { width:100%; max-width:640px; border:1px solid var(--dim); padding:12px; }
  section h2 { font-size:.75rem; color:var(--dim); letter-spacing:.15em;
               text-transform:uppercase; margin-bottom:10px; }
  .row { display:flex; gap:8px; align-items:center; flex-wrap:wrap; margin-bottom:8px; }
  input[type=file], input[type=text] {
    background:#1a1a1a; border:1px solid var(--dim); color:var(--fg);
    padding:4px 8px; font-family:monospace; font-size:.85rem; flex:1; min-width:0; }
  button { background:transparent; border:1px solid var(--accent); color:var(--accent);
           padding:4px 14px; font-family:monospace; font-size:.85rem;
           cursor:pointer; white-space:nowrap; }
  button:hover { background:var(--accent); color:#000; }
  #status { font-size:.75rem; color:var(--dim); min-height:1.2em; }
</style>
</head>
<body>
<h1>ESP32 · Camera</h1>

<div id="stream-wrap">
  <img id="stream" src="/stream" alt="stream">
  <div id="overlay"></div>
</div>

<section>
  <h2>File</h2>
  <div class="row">
    <input type="file" id="file-input" accept=".html,.jpg">
    <button onclick="uploadFile()">Upload</button>
  </div>
  <div class="row">
    <input type="text" id="del-name" placeholder="index.html / QRcode.jpg">
    <button onclick="deleteFile()">Delete</button>
  </div>
</section>

<section>
  <h2>OTA Firmware</h2>
  <div class="row">
    <input type="file" id="fw-input" accept=".bin">
    <button onclick="uploadFirmware()">Flash</button>
  </div>
</section>

<div id="status"></div>

<script>
const overlay = document.getElementById('overlay');
const status  = document.getElementById('status');

function showOverlay(msg) {
  overlay.textContent = msg;
  overlay.classList.add('show');
}
function hideOverlay() { overlay.classList.remove('show'); }
function setStatus(msg) { status.textContent = msg; }

async function uploadFile() {
  const f = document.getElementById('file-input').files[0];
  if (!f) return setStatus('请选择文件');
  if (f.name !== 'index.html' && f.name !== 'QRcode.jpg')
    return setStatus('仅支持 index.html 或 QRcode.jpg');

  showOverlay('上传中，请稍候…');
  setStatus('');
  const fd = new FormData();
  fd.append('upload', f);
  try {
    const r = await fetch('/upload', { method:'POST', body:fd });
    setStatus(await r.text());
  } catch(e) { setStatus('上传失败: ' + e); }
  hideOverlay();
}

async function deleteFile() {
  const name = document.getElementById('del-name').value.trim();
  if (!name) return setStatus('请输入文件名');
  const fd = new FormData();
  fd.append('filename', name);
  try {
    const r = await fetch('/delete', { method:'POST', body:fd });
    setStatus(await r.text());
  } catch(e) { setStatus('删除失败: ' + e); }
}

async function uploadFirmware() {
  const f = document.getElementById('fw-input').files[0];
  if (!f) return setStatus('请选择固件文件');

  showOverlay('OTA 上传中，请勿关闭页面…');
  setStatus('');
  const fd = new FormData();
  fd.append('update', f);
  try {
    const r = await fetch('/update', { method:'POST', body:fd });
    const msg = await r.text();
    if (msg === 'OK') {
      showOverlay('固件更新成功，设备重启中…\n请稍后刷新页面');
    } else {
      hideOverlay();
      setStatus('OTA 失败: ' + msg);
    }
  } catch(e) {
    // 设备重启导致连接断开，属于正常现象
    showOverlay('设备重启中…\n请稍后手动刷新页面');
  }
}
</script>
</body>
</html>
)rawhtml";
