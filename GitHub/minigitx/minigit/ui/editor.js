/* editor.js - behaviour of the file editor page (editor.html).
   Uses three web addresses of minigit.exe:
     GET  /api/state            -> list of files (and their status)
     GET  /api/file?name=...    -> read a file
     POST /api/save?name=...    -> write a file   (body = the text)
     POST /api/delete?name=...  -> delete a file */
(function(){
'use strict';
var $ = function(id){ return document.getElementById(id); };
var esc = function(s){ return String(s).replace(/[&<>"]/g, function(c){ return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]; }); };
var lastJson = '';

function call(url, opts){
  opts = opts || {};
  opts.headers = Object.assign({'X-Minigit':'1'}, opts.headers || {});
  return fetch(url, opts);
}

function say(text, bad){
  var box = $('savemsg');
  box.textContent = text;
  box.className = 'savemsg' + (bad ? ' bad' : '');
}

/* ---------- the file list ---------- */
async function loadList(force){
  var s;
  try{
    var r = await call('/api/state'); var t = await r.text();
    if (!force && t === lastJson) return;
    lastJson = t; s = JSON.parse(t);
  }catch(e){ say('Cannot reach the minigit program. Is its window still open?', true); return; }

  $('dir').textContent = s.cwd;
  var status = {};
  function add(p, text, cls){ (status[p] = status[p] || []).push([text, cls]); }
  if (s.initialized){
    s.staged.forEach(function(x){ add(x.p, 'staged', 'ok'); });
    s.unstaged.forEach(function(x){ add(x.p, x.s === 'D' ? 'deleted' : 'modified', 'warn'); });
    s.untracked.forEach(function(p){ add(p, 'new', 'muted'); });
  }
  var names = {};
  (s.files || []).forEach(function(p){ names[p] = 1; });
  if (s.initialized) s.staged.concat(s.unstaged).forEach(function(x){ names[x.p] = 1; });
  var list = Object.keys(names).sort();
  $('files').innerHTML = list.length ? list.map(function(p){
    var b = status[p] || [['saved', 'clean']];
    return '<li><span class="name" data-open="' + esc(p) + '">' + esc(p) + '</span>' +
      b.map(function(x){ return '<span class="badge ' + x[1] + '">' + x[0] + '</span>'; }).join('') + '</li>';
  }).join('') : '<li class="empty">No files yet. Type a name on the right and save.</li>';
}

/* ---------- actions ---------- */
async function openFile(name){
  var r = await call('/api/file?name=' + encodeURIComponent(name));
  if (!r.ok){ say('Could not open ' + name, true); return; }
  $('fName').value = name;
  $('fText').value = await r.text();
  say('Opened ' + name);
}

async function saveFile(){
  var name = $('fName').value.trim();
  if (!name){ say('Type a file name first.', true); return; }
  var r = await call('/api/save?name=' + encodeURIComponent(name), {method:'POST', body: $('fText').value});
  var j = await r.json();
  if (j.error){ say('Save failed: ' + j.error, true); return; }
  $('fName').value = j.name;
  say('Saved ' + j.name + '.');
  await loadList(true);
}

async function deleteFile(){
  var name = $('fName').value.trim();
  if (!name){ say('Open or type the file you want to delete.', true); return; }
  if (!confirm('Delete ' + name + ' from the folder?')) return;
  var r = await call('/api/delete?name=' + encodeURIComponent(name), {method:'POST', body:''});
  var j = await r.json();
  if (j.error){ say('Delete failed: ' + j.error, true); return; }
  $('fName').value = ''; $('fText').value = '';
  say('Deleted ' + name + '.');
  await loadList(true);
}

document.addEventListener('click', function(ev){
  var t = ev.target;
  if (t.dataset && t.dataset.open) return openFile(t.dataset.open);
  if (t.id === 'bSave') return saveFile();
  if (t.id === 'bDelete') return deleteFile();
  if (t.id === 'bNew'){ $('fName').value = ''; $('fText').value = ''; say(''); $('fName').focus(); }
});

/* Ctrl+S saves, like in any editor */
document.addEventListener('keydown', function(e){
  if ((e.ctrlKey || e.metaKey) && (e.key === 's' || e.key === 'S')){ e.preventDefault(); saveFile(); }
});

loadList(true);
setInterval(function(){ if (!document.hidden) loadList(false); }, 3000);
})();
