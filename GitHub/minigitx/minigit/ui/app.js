/* app.js - the behaviour of the minigit page.
   It talks to minigit.exe over three simple web addresses:
     GET  /api/state  -> everything the page needs to draw itself (JSON)
     POST /api/run    -> runs one minigit command, e.g. "commit" "-m" "message"
   (Writing data into files is done by the separate page editor.html / editor.js.)
   Sections: talking to the program / drawing the page / commit graph / buttons. */

(function(){
'use strict';
var $ = function(id){ return document.getElementById(id); };
var esc = function(s){ return String(s).replace(/[&<>"]/g, function(c){ return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]; }); };
var COLORS = ['#2563eb','#db2777','#059669','#d97706','#7c3aed','#0891b2'];
var S = null, lastJson = '', selected = null;

/* ---------- talking to the program ---------- */
function call(url, opts){
  opts = opts || {};
  opts.headers = Object.assign({'X-Minigit':'1'}, opts.headers || {});
  return fetch(url, opts);
}
async function exec(args){
  var r = await call('/api/run', {method:'POST', body: args.join('\n')});
  var j = await r.json();
  if (j.error && !('ok' in j)) return {ok:false, output:'', error:'fatal: ' + j.error + '\n'};
  return j;
}
function quote(a){ return /[\s"]/.test(a) ? '"' + a.replace(/"/g,'\\"') + '"' : a; }
function addLog(cmd, out, err, ok){
  var box = $('console');
  var d = document.createElement('div'); d.className = 'cmd'; d.textContent = '> ' + cmd; box.appendChild(d);
  if (out){ var o = document.createElement('div'); o.className = 'out'; o.textContent = out.replace(/\s+$/,''); box.appendChild(o); }
  if (err){ var e = document.createElement('div'); e.className = 'err'; e.textContent = err.replace(/\s+$/,''); box.appendChild(e); }
  if (!out && !err && ok){ var n = document.createElement('div'); n.className = 'note'; n.textContent = 'done'; box.appendChild(n); }
  while (box.children.length > 300) box.removeChild(box.firstChild);
  box.scrollTop = box.scrollHeight;
}
function note(text, bad){
  var box = $('console'), d = document.createElement('div');
  d.className = bad ? 'err' : 'note'; d.textContent = text; box.appendChild(d); box.scrollTop = box.scrollHeight;
}
async function run(args){
  var j = await exec(args);
  addLog('minigit ' + args.map(quote).join(' '), j.output, j.error, j.ok);
  await refresh(true);
  return j;
}
async function refresh(force){
  try{
    var r = await call('/api/state'); var t = await r.text();
    $('conn').style.display = 'none';
    if (!force && t === lastJson) return;
    lastJson = t; S = JSON.parse(t); render();
  }catch(e){ $('conn').style.display = 'block'; }
}

/* ---------- rendering ---------- */
function firstLine(s){ return String(s).split('\n')[0]; }
function fmtDate(ts){ return new Date(ts * 1000).toLocaleString([], {dateStyle:'medium', timeStyle:'short'}); }

function fillRepositorySelect(id, selected){
  var el = $(id); if (!el) return;
  var html = '<option value="">Select a repository</option>';
  (S.repositories || []).forEach(function(name){
    html += '<option value="' + esc(name) + '"' + (name === selected ? ' selected' : '') + '>' + esc(name) + '</option>';
  });
  el.innerHTML = html;
}
function render(){
  $('dir').textContent = S.cwd;
  fillRepositorySelect('repoSelect', '');
  fillRepositorySelect('repoSelectMain', S.selectedRepo || '');
  if (!S.selectedRepo || !S.initialized){
    $('initview').style.display = ''; $('mainview').style.display = 'none';
    $('repoTools').style.display = 'none';
    $('initdir').textContent = S.cwd + '/Repositories'; $('branchpill').innerHTML = '';
    $('bUndo').disabled = $('bRedo').disabled = $('bVerify').disabled = $('bStats').disabled = true;
    return;
  }
  $('initview').style.display = 'none'; $('mainview').style.display = '';
  $('repoTools').style.display = '';
  $('bUndo').disabled = !S.canUndo; $('bRedo').disabled = !S.canRedo;
  $('bVerify').disabled = $('bStats').disabled = false;
  $('branchpill').innerHTML = S.branch ? '<span class="pill branch">on ' + esc(S.branch) + '</span>'
                                       : '<span class="pill">detached HEAD</span>';
  renderChanges(); renderCommit(); renderBranches(); renderBanner(); renderHistory();
}

/* A short summary instead of a file list: how much is waiting to be staged. */
function renderChanges(){
  var untracked = S.untracked.length, modified = S.unstaged.length, staged = S.staged.length;
  var waiting = untracked + modified, parts = [];
  if (untracked) parts.push(untracked + ' new');
  if (modified) parts.push(modified + ' changed');
  var text = waiting ? 'Waiting to be staged: ' + parts.join(', ') + '.' : 'Nothing waiting to be staged.';
  if (staged) text += ' Staged and ready to commit: ' + staged + '.';
  $('changeinfo').textContent = text;
  $('bStageAll').disabled = !waiting;
}

function renderCommit(){
  var n = S.staged.length;
  $('bCommit').disabled = !(n > 0 || S.merging);
  $('stagedinfo').textContent = n ? n + ' file(s) staged and ready to commit.' : 'Nothing staged yet. Use "Stage all changes" in step 1.';
}

function renderBranches(){
  $('branches').innerHTML = S.branches.length ? S.branches.map(function(b){
    var cur = b.name === S.branch;
    return '<div class="bpill' + (cur ? ' cur' : '') + '">' + (cur ? '&#10003; ' : '') + esc(b.name) +
      ' <span class="tip">' + esc(b.tip.slice(0,7)) + '</span>' +
      (cur ? '' : '<button class="small" data-switch="' + esc(b.name) + '">Switch</button>') + '</div>';
  }).join('') : '<span class="empty">No branch yet. It appears after your first commit.</span>';
  var others = S.branches.filter(function(b){ return b.name !== S.branch; });
  $('mergeSel').innerHTML = others.length ? others.map(function(b){ return '<option>' + esc(b.name) + '</option>'; }).join('')
                                          : '<option value="">(no other branch)</option>';
  $('bMerge').disabled = !others.length || S.merging;
}

function renderBanner(){
  $('mergebanner').innerHTML = S.merging
    ? '<div class="banner warn"><span><b>Merge in progress.</b> Open the conflicted files in the file editor (look for &lt;&lt;&lt;&lt;&lt;&lt;&lt;), fix them, Stage all changes, then Commit.</span><button id="bAbort" class="danger">Abort merge</button></div>' : '';
}

/* ---------- commit graph ---------- */
function layout(commits){
  var lanes = [], pos = {};
  commits.forEach(function(c, row){
    var lane = lanes.indexOf(c.hash);
    if (lane < 0){ lane = lanes.indexOf(null); if (lane < 0){ lane = lanes.length; lanes.push(null); } }
    for (var i = 0; i < lanes.length; i++) if (i !== lane && lanes[i] === c.hash) lanes[i] = null;
    pos[c.hash] = {row: row, lane: lane};
    lanes[lane] = c.parents.length ? c.parents[0] : null;
    for (var k = 1; k < c.parents.length; k++){
      var p = c.parents[k];
      if (lanes.indexOf(p) < 0){ var e = lanes.indexOf(null); if (e < 0){ e = lanes.length; lanes.push(null); } lanes[e] = p; }
    }
  });
  return pos;
}

function renderHistory(){
  var C = S.commits, box = $('hist');
  if (!C.length){ box.innerHTML = '<div class="empty">No commits yet. Stage a file, then press Commit.</div>'; $('detail').style.display = 'none'; return; }
  var pos = layout(C), maxLane = 0;
  Object.keys(pos).forEach(function(h){ if (pos[h].lane > maxLane) maxLane = pos[h].lane; });
  var LW = 20, RH = 46, GW = (maxLane + 1) * LW + 20;
  function X(l){ return 14 + l * LW; }
  function Y(r){ return r * RH + RH / 2; }
  var tips = {}; S.branches.forEach(function(b){ (tips[b.tip] = tips[b.tip] || []).push(b.name); });

  var svg = '<svg width="' + GW + '" height="' + (C.length * RH) + '">', dots = '';
  C.forEach(function(c){
    var a = pos[c.hash];
    c.parents.forEach(function(p, i){
      var b = pos[p]; if (!b) return;
      var col = COLORS[(i ? b.lane : a.lane) % COLORS.length];
      var x1 = X(a.lane), y1 = Y(a.row), x2 = X(b.lane), y2 = Y(b.row);
      svg += x1 === x2 ? '<line x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '" stroke="' + col + '" stroke-width="2.5"/>'
        : '<path d="M' + x1 + ' ' + y1 + ' C ' + x1 + ' ' + (y1 + RH * .8) + ', ' + x2 + ' ' + (y2 - RH * .8) + ', ' + x2 + ' ' + y2 +
          '" fill="none" stroke="' + col + '" stroke-width="2.5"/>';
    });
    var col2 = COLORS[a.lane % COLORS.length], isHead = c.hash === S.head, isSel = c.hash === selected;
    dots += '<circle cx="' + X(a.lane) + '" cy="' + Y(a.row) + '" r="' + (isSel ? 8 : 6.5) + '" fill="' + col2 + '" stroke="' +
            (isHead ? '#ffffff' : '#111111') + '" stroke-width="' + (isHead ? 3 : 2) + '"/>';
  });
  svg += dots + '</svg>';

  box.innerHTML = svg + C.map(function(c){
    var tags = '';
    if (c.hash === S.head) tags += '<span class="tag head">HEAD' + (S.branch ? ' &rarr; ' + esc(S.branch) : '') + '</span> ';
    (tips[c.hash] || []).forEach(function(n){ if (n !== S.branch || c.hash !== S.head) tags += '<span class="tag br">' + esc(n) + '</span> '; });
    return '<div class="crow' + (c.hash === selected ? ' sel' : '') + '" data-commit="' + c.hash + '" style="height:' + RH + 'px;padding-left:' + GW + 'px">' +
      '<span class="hash">' + c.hash.slice(0, 7) + '</span><span class="msg">' + esc(firstLine(c.message)) + '</span>' + tags +
      '<span class="meta">' + esc(c.author) + ' &middot; ' + esc(fmtDate(c.timestamp)) + '</span></div>';
  }).join('');
  renderDetail();
}

function renderDetail(){
  var d = $('detail'), c = null;
  S.commits.forEach(function(x){ if (x.hash === selected) c = x; });
  if (!c){ d.style.display = 'none'; return; }
  d.style.display = '';
  d.innerHTML = '<div class="kv">commit <code>' + c.hash + '</code></div>' +
    '<div class="msgfull">' + esc(c.message) + '</div>' +
    '<div class="kv">' + esc(c.author) + ' &lt;' + esc(c.email) + '&gt; &middot; ' + esc(fmtDate(c.timestamp)) + '</div>' +
    '<div class="kv">parents: ' + (c.parents.length ? c.parents.map(function(p){ return '<code>' + p.slice(0,7) + '</code>'; }).join(' ') : 'none (first commit)') +
    (c.parents.length > 1 ? ' &mdash; this is a merge commit' : '') + '</div>' +
    '<div class="row" style="margin-top:10px"><button id="bShowCommit">Show what changed</button>' +
    '<button id="bCheckoutCommit">Checkout this commit</button></div>';
}

/* ---------- diff pane ---------- */
function showDiff(title, text){
  $('diffcard').style.display = ''; $('difftitle').textContent = title;
  $('diffbox').innerHTML = esc(text).split('\n').map(function(l){
    var c = l.indexOf('###') === 0 ? 't' : l.indexOf('+++') === 0 || l.indexOf('---') === 0 ? 'f'
          : l.charAt(0) === '+' ? 'a' : l.charAt(0) === '-' ? 'd' : l.indexOf('@@') === 0 ? 'h'
          : l.indexOf('diff ') === 0 || l.indexOf('index ') === 0 || l.indexOf('new file') === 0 || l.indexOf('deleted file') === 0 ? 'f' : '';
    return c ? '<span class="' + c + '">' + l + '</span>' : l;
  }).join('\n');
}

/* ---------- actions ---------- */
async function showChanges(){
  var un = await exec(['diff']), stg = await exec(['diff','--staged']), t = '';
  if (un.output) t += '### Not staged yet\n' + un.output;
  if (stg.output) t += (t ? '\n' : '') + '### Staged (will go into the next commit)\n' + stg.output;
  showDiff('Working changes', t || 'No changes. Edit a file and save it.');
}
async function doSearch(){
  var q = $('sQ').value.trim();
  if (!q){ $('results').innerHTML = ''; return; }
  var j = await exec(['search'].concat(q.split(/\s+/)));
  var lines = (j.output || '').split('\n').filter(Boolean);
  $('results').innerHTML = j.ok && lines.length && lines[0] !== 'no commits match'
    ? lines.map(function(l){ var i = l.indexOf(' '); return '<div data-hash="' + esc(l.slice(0, i)) + '"><code>' + esc(l.slice(0, i)) + '</code>' + esc(l.slice(i + 1)) + '</div>'; }).join('')
    : '<div class="empty">No commits match.</div>';
}
function selectCommit(h){ selected = h; renderHistory(); }

document.addEventListener('click', async function(ev){
  var t = ev.target, id = t.id;
  if (t.dataset.switch) return run(['checkout', t.dataset.switch]);
  var row = t.closest ? t.closest('[data-commit]') : null;
  if (row) return selectCommit(row.dataset.commit);
  var hit = t.closest ? t.closest('[data-hash]') : null;
  if (hit){ var h = hit.dataset.hash; S.commits.forEach(function(c){ if (c.hash.indexOf(h) === 0) selectCommit(c.hash); }); return; }
  if (id === 'bInit'){
    var repoName = $('iRepo').value.trim();
    if (!repoName){ note('Enter a repository name first.', true); return; }
    var response = await call('/api/repos', {method:'POST', body: repoName + '\n' + $('iName').value.trim() + '\n' + $('iMail').value.trim()});
    var created = await response.json();
    if (created.error){ note(created.error, true); return; }
    note('Created repository: ' + created.name, false);
    await refresh(true);
  }
  else if (id === 'bOpenRepo'){
    var openName = $('repoSelect').value;
    if (!openName){ note('Select a repository first.', true); return; }
    var opened = await call('/api/select', {method:'POST', body:openName});
    var openedJson = await opened.json();
    if (openedJson.error){ note(openedJson.error, true); return; }
    await refresh(true);
  }
  else if (id === 'bSwitchRepo'){
    var switchName = $('repoSelectMain').value;
    if (!switchName){ note('Select a repository first.', true); return; }
    var switched = await call('/api/select', {method:'POST', body:switchName});
    var switchedJson = await switched.json();
    if (switchedJson.error){ note(switchedJson.error, true); return; }
    selected = null; await refresh(true);
  }
  else if (id === 'bManageRepos'){
    await call('/api/select', {method:'POST', body:''}); selected = null; await refresh(true);
  }
  else if (id === 'bStageAll') await run(['add','.']);
  else if (id === 'bChanges') await showChanges();
  else if (id === 'bCommit'){
    var msg = $('cMsg').value.replace(/\s+/g,' ').trim();
    var j = await run(msg ? ['commit','-m', msg] : ['commit']);
    if (j.ok) $('cMsg').value = '';
  }
  else if (id === 'bNewBranch'){
    var nb = $('nbName').value.trim(); if (!nb){ note('Type a branch name first.', true); return; }
    var j2 = await run(['checkout','-b', nb]); if (j2.ok) $('nbName').value = '';
  }
  else if (id === 'bMerge'){ var b = $('mergeSel').value; if (b) await run(['merge', b]); }
  else if (id === 'bAbort') await run(['merge','--abort']);
  else if (id === 'bUndo') await run(['undo']);
  else if (id === 'bRedo') await run(['redo']);
  else if (id === 'bVerify') await run(['verify']);
  else if (id === 'bStats') await run(['stats']);
  else if (id === 'bSearch') await doSearch();
  else if (id === 'bClear') $('console').innerHTML = '';
  else if (id === 'bShowCommit'){
    var j3 = await exec(['diff', selected]);
    showDiff('Commit ' + selected.slice(0, 7), j3.output || j3.error || 'No file changes in this commit.');
  }
  else if (id === 'bCheckoutCommit') await run(['checkout', selected]);
});
$('cMsg').addEventListener('keydown', function(e){ if (e.key === 'Enter' && !$('bCommit').disabled) $('bCommit').click(); });
$('sQ').addEventListener('keydown', function(e){ if (e.key === 'Enter') doSearch(); });
$('nbName').addEventListener('keydown', function(e){ if (e.key === 'Enter') $('bNewBranch').click(); });

refresh(true);
setInterval(function(){ if (!document.hidden) refresh(false); }, 2500);
})();
