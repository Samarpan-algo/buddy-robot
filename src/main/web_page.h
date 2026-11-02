// web_page.h - the phone page (HTML + CSS + JavaScript in one string).
// Served by the ESP32 at "/". No frameworks, no extra files.
// The Gemini API key is typed into Settings and stays in the phone (localStorage).
// Tabs: Chat, Study (Explain / Quiz / Pomodoro), Drive, Settings.
#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Buddy</title>
<style>
:root{--bg:#10141a;--card:#1b222c;--ink:#e8eef5;--mute:#8a97a8;--acc:#4fc3f7;--me:#2e7d32;--bot:#263445;--err:#8e2b2b;--ok:#2e7d32}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--ink);display:flex;flex-direction:column;height:100vh;height:100dvh}
header{padding:10px 14px;background:var(--card);display:flex;justify-content:space-between;align-items:center}
header b{font-size:18px}
#status{font-size:13px;color:var(--mute)}
nav{display:flex;background:var(--card);border-top:1px solid #2a3340}
nav button{flex:1;padding:12px 4px;border:0;background:none;color:var(--mute);font-size:15px}
nav button.on{color:var(--acc);border-bottom:2px solid var(--acc)}
section{display:none;flex:1;overflow:auto;padding:12px}
section.on{display:flex;flex-direction:column}
#log{flex:1;overflow:auto;display:flex;flex-direction:column;gap:8px;padding-bottom:8px}
.msg{max-width:85%;padding:9px 12px;border-radius:14px;line-height:1.35;white-space:pre-wrap}
.me{align-self:flex-end;background:var(--me)}
.bot{align-self:flex-start;background:var(--bot)}
.err{align-self:center;background:var(--err);font-size:14px}
.tag{display:block;font-size:11px;color:var(--mute);margin-bottom:2px}
.bar{display:flex;gap:8px;align-items:center}
#msg{flex:1;padding:12px;border-radius:22px;border:1px solid #33404f;background:#0d1117;color:var(--ink);font-size:16px}
.round{width:48px;height:48px;border-radius:50%;border:0;font-size:20px;background:var(--acc);color:#002;flex:none}
.mic.on{background:#e53935;color:#fff}
#stopGo{display:none;background:#e53935;color:#fff;border:0;border-radius:22px;padding:12px 16px;font-size:15px;margin-bottom:8px}
#pad{display:grid;grid-template-columns:repeat(3,90px);grid-template-rows:repeat(3,90px);gap:10px;justify-content:center;margin:20px auto;touch-action:none;user-select:none;-webkit-user-select:none}
#pad button{border:0;border-radius:18px;background:var(--bot);color:var(--ink);font-size:30px}
#pad button:active{background:var(--acc);color:#002}
label{display:block;margin:10px 0 4px;color:var(--mute);font-size:14px}
input[type=text],input[type=password],input[type=number],select{width:100%;padding:11px;border-radius:10px;border:1px solid #33404f;background:#0d1117;color:var(--ink);font-size:15px}
.btn{margin-top:10px;padding:11px 16px;border:0;border-radius:10px;background:var(--acc);color:#002;font-size:15px}
.btn.grey{background:#33404f;color:var(--ink)}
/* every button visibly presses in when touched */
button{transition:transform .08s,filter .08s;-webkit-tap-highlight-color:transparent;cursor:pointer}
button:active{transform:scale(.93);filter:brightness(1.35)}
.flash{animation:flash .6s}
@keyframes flash{0%{box-shadow:0 0 0 0 rgba(79,195,247,.9)}100%{box-shadow:0 0 0 14px rgba(79,195,247,0)}}
#lFeedback{min-height:20px;margin-top:6px;font-size:15px}
.note{color:var(--mute);font-size:13px;line-height:1.4}
.card{background:var(--card);border-radius:14px;padding:12px;margin-bottom:12px}
.card h3{margin:0 0 6px;font-size:16px}
.out{white-space:pre-wrap;line-height:1.4;margin-top:10px}
.opt{display:block;width:100%;text-align:left;margin-top:8px;padding:11px;border-radius:10px;border:1px solid #33404f;background:#0d1117;color:var(--ink);font-size:15px}
.opt.right{background:var(--ok);border-color:var(--ok)}
.opt.wrong{background:var(--err);border-color:var(--err)}
.row{display:flex;gap:8px}.row>*{flex:1}
#timer{font-size:46px;text-align:center;font-variant-numeric:tabular-nums;margin:8px 0}
.mbar{display:flex;align-items:center;gap:8px;margin:6px 0;font-size:14px}
.mbar span{width:62px;color:var(--mute)}
.mbar .track{flex:1;height:14px;background:#0d1117;border-radius:7px;overflow:hidden}
.mbar .fill{height:100%;border-radius:7px;transition:width .4s}
.mbar b{width:30px;text-align:right;font-weight:normal}
.list{font-size:13px;line-height:1.5;max-height:220px;overflow:auto}
.list div{border-bottom:1px solid #26303c;padding:3px 0}
.ago{color:var(--mute);margin-right:6px}
#mEmo{font-size:22px;text-transform:capitalize}
.help{display:none;background:#2b2a14;border-radius:10px;padding:10px;margin-bottom:8px;font-size:13px;line-height:1.45}
</style></head><body>
<header><b>&#129302; Buddy</b><span id="status">connecting...</span></header>
<nav>
 <button id="t-chat" class="on" onclick="tab('chat')">Chat</button>
 <button id="t-mind" onclick="tab('mind')">Mind</button>
 <button id="t-study" onclick="tab('study')">Study</button>
 <button id="t-drive" onclick="tab('drive')">Drive</button>
 <button id="t-settings" onclick="tab('settings')">Settings</button>
</nav>

<section id="chat" class="on">
 <div class="help"></div>
 <div id="log"></div>
 <button id="stopGo" onclick="stopGo(true)">&#9632; Stop driving</button>
 <div class="bar">
  <button class="round mic" onclick="mic()" title="Speak">&#127908;</button>
  <input id="msg" type="text" placeholder="Talk to Buddy..." enterkeyhint="send">
  <button class="round" onclick="userSays($('msg').value)" title="Send">&#10148;</button>
 </div>
</section>

<section id="mind">
 <div class="card">
  <div id="mEmo">...</div>
  <div class="note" id="mWhy"></div>
 </div>
 <div class="card">
  <h3>&#129504; Feelings</h3>
  <div id="mBars"></div>
  <p class="note" id="mInfo"></p>
 </div>
 <div class="card">
  <h3>&#128578; How you seem</h3>
  <div id="uMood" style="font-size:18px;text-transform:capitalize">calm</div>
  <div class="list" id="uEvidence"></div>
 </div>
 <div class="card">
  <h3>&#127919; Focus buddy</h3>
  <div class="note" id="mFocus">...</div>
 </div>
 <div class="card">
  <h3>&#127891; Learning: which moves do you like?</h3>
  <button class="btn" id="lMode" style="width:100%;margin:0 0 10px" onclick="toggleLearn()">Learning mode: ...</button>
  <div id="lBanner" style="display:none;background:#1e4620;border-radius:10px;padding:8px;margin-bottom:8px;font-size:15px"></div>
  <div class="note" id="lState" style="white-space:pre-line"></div>
  <div id="lBars"></div>
  <div class="row"><button class="btn" id="thUp" onclick="thumb(1)">&#128077; Liked it</button><button class="btn grey" id="thDown" onclick="thumb(-1)">&#128078; Not that</button></div>
  <div id="lFeedback"></div>
  <button class="btn grey" onclick="if(confirm('Forget everything Buddy learnt?')) api('/learn?reset=1')">Reset learning</button>
  <p class="note">Switch learning mode ON for the demo. When Buddy feels lonely it tries a move. Pet it, praise it or tap thumbs-up within 5 s and that move scores higher; ignore it or scare it and the score drops.</p>
 </div>
 <div class="card">
  <h3>&#128173; Buddy's thoughts</h3>
  <div class="list" id="mThoughts"></div>
 </div>
 <div class="card">
  <h3>&#128220; Memory (last 20 events)</h3>
  <div class="list" id="mEvents"></div>
 </div>
 <p class="note">This page comes straight from the robot: the feelings, memory and decisions are computed on Buddy itself and work without internet.</p>
</section>

<section id="study">
 <div class="help"></div>
 <div class="card">
  <h3>&#128161; Explain</h3>
  <div class="row"><input id="exTopic" type="text" placeholder="Topic, e.g. photosynthesis">
   <select id="exLevel" style="flex:0 0 110px"><option value="school">School</option><option value="college" selected>College</option></select></div>
  <button class="btn" onclick="explain($('exTopic').value)">Explain</button>
  <div id="exOut" class="out"></div>
 </div>
 <div class="card">
  <h3>&#10067; Quiz</h3>
  <div class="row"><input id="qzTopic" type="text" placeholder="Topic, e.g. Newton's laws"><button class="btn" style="flex:0 0 90px;margin:0" onclick="startQuiz($('qzTopic').value)">Start</button></div>
  <p class="note" id="qzScore"></p>
  <div id="qzBox"></div>
 </div>
 <div class="card">
  <h3>&#9201; Pomodoro</h3>
  <div id="timer">--:--</div>
  <p class="note" id="fcState" style="text-align:center">off</p>
  <div class="row"><div><label>Work (min)</label><input id="fcWork" type="number" min="1" max="120" value="25"></div>
   <div><label>Break (min)</label><input id="fcBreak" type="number" min="0" max="60" value="5"></div></div>
  <div class="row"><button class="btn" onclick="startFocus($('fcWork').value,$('fcBreak').value)">Start</button><button class="btn grey" onclick="stopFocus()">Stop</button></div>
  <button class="btn" style="width:100%;background:#ffb74d" onclick="startFocus(1,1)">&#9889; 1 min demo (1 min focus + 1 min break)</button>
  <p class="note" id="fcScore"></p>
  <p class="note">While you study, Buddy checks you stay at your desk (in front of it, closer than 80 cm). Walk away and it calls you back; at the end it tells you your focus score.</p>
 </div>
 <div class="bar" style="justify-content:center"><button class="round mic" onclick="mic()" title="Speak">&#127908;</button>
  <span class="note">Say "quiz me on ...", "explain ...", "start a 25 minute focus", or answer "A", "B" ...</span></div>
</section>

<section id="drive">
 <div id="pad">
  <span></span><button data-d="f">&#9650;</button><span></span>
  <button data-d="l">&#9664;</button><button data-d="s">&#9632;</button><button data-d="r">&#9654;</button>
  <span></span><button data-d="b">&#9660;</button><span></span>
 </div>
 <label>Speed: <span id="spv">60</span>%</label>
 <input id="speed" type="range" min="20" max="100" value="60" oninput="$('spv').textContent=this.value" style="width:100%">
 <p class="note">Hold a button to drive. Buddy stops as soon as you let go (and by itself after 0.6 s without a signal). Forward is blocked when something is closer than 15 cm, reverse when the rear sensor sees something.</p>
</section>

<section id="settings">
 <label>Gemini API key (stays on this phone only)</label>
 <input id="key" type="password" autocomplete="off" placeholder="AIza...">
 <label>Model name</label>
 <input id="model" type="text" autocomplete="off">
 <label><input id="speakOn" type="checkbox" checked> Speak Buddy's replies</label>
 <label>Voice</label>
 <select id="voice">
  <option value="robot">Robot speaker - Gemini voice (falls back to retro)</option>
  <option value="sam">Robot speaker - retro robot voice</option>
  <option value="phone">Phone speaker</option>
 </select>
 <label>Gemini voice model (for the robot speaker)</label>
 <input id="ttsModel" type="text" autocomplete="off">
 <button class="btn" id="sfxBtn" style="width:100%" onclick="toggleSfx()">Robot sound effects: ...</button>
 <p class="note">Chirps and tunes for emotions (speech is not affected). Saved on the robot.</p>
 <button class="btn" onclick="saveSettings()">Save</button>
 <button class="btn grey" onclick="clearChat()">Clear chat</button>
 <button class="btn grey" onclick="resetQuiz()">Reset quiz score</button>
 <p class="note" id="saved"></p>
 <p class="note">Get a free key at aistudio.google.com &rarr; "Get API key". Default model: gemini-3.5-flash-lite. Free-tier limits are shown in AI Studio.</p>
</section>

<script async src="https://cdn.jsdelivr.net/npm/sam-js@0.3.1/dist/samjs.min.js"></script>
<script>
const $ = id => document.getElementById(id);
const DEFAULT_TTS_MODEL = 'gemini-3.8-flash-lite-tts';
const DEFAULT_MODEL = 'gemini-3.5-flash-lite';
const NEW_CONVERSATION_MS = 120000;      // 2 min without messages = new conversation
let history = [];                        // last chat turns, sent to Gemini for context
let busy = false;
let lastUserText = '';                   // for the [USER:...] evidence quote (G1c)
let lastMsgTime = 0;
let robotState = null;                   // latest /state

// ---------- tabs ----------
function tab(name){
  for (const t of ['chat','mind','study','drive','settings']){
    $(t).classList.toggle('on', t===name);
    $('t-'+t).classList.toggle('on', t===name);
  }
  release();
}

// ---------- talking to the robot ----------
async function api(path){
  try { const r = await fetch(path, {cache:'no-store'}); return r.ok ? await r.json() : null; }
  catch(e){ return null; }
}
function face(e, ms, beep=true){ api('/emotion?e='+e+'&ms='+ms+(beep?'':'&beep=0')); }

// ---------- status bar + Pomodoro display (poll every 2 s, tick every 1 s) ----------
let focusMode = 'off', focusEnd = 0;
async function pollStatus(){
  const s = await api('/state');
  robotState = s;
  $('status').textContent = s ? (s.emotion+' · '+(s.dist_cm>0? Math.round(s.dist_cm)+' cm':'clear')+' · bond '+s.bond) : 'robot offline';
  if (s){ focusMode = s.focus; focusEnd = Date.now() + s.focus_left_s*1000; }
}
function tickTimer(){
  const left = Math.max(0, Math.round((focusEnd - Date.now())/1000));
  $('timer').textContent = focusMode === 'off' ? '--:--' : String(Math.floor(left/60)).padStart(2,'0')+':'+String(left%60).padStart(2,'0');
  $('fcState').textContent = focusMode === 'work' ? 'focus time - Buddy keeps quiet' : focusMode === 'break' ? 'break time' : 'off';
}
setInterval(pollStatus, 2000); pollStatus();
setInterval(tickTimer, 1000);

// ---------- settings (localStorage only) ----------
function loadSettings(){
  $('key').value = localStorage.getItem('buddy_key') || '';
  $('model').value = localStorage.getItem('buddy_model') || DEFAULT_MODEL;
  $('speakOn').checked = localStorage.getItem('buddy_speak') !== '0';
  $('voice').value = localStorage.getItem('buddy_voice') || 'robot';
  $('ttsModel').value = localStorage.getItem('buddy_tts_model') || DEFAULT_TTS_MODEL;
}
function saveSettings(){
  localStorage.setItem('buddy_key', $('key').value.trim());
  localStorage.setItem('buddy_model', $('model').value.trim() || DEFAULT_MODEL);
  localStorage.setItem('buddy_speak', $('speakOn').checked ? '1' : '0');
  localStorage.setItem('buddy_voice', $('voice').value);
  localStorage.setItem('buddy_tts_model', $('ttsModel').value.trim() || DEFAULT_TTS_MODEL);
  $('saved').textContent = 'Saved.';
}
let sfxOn = true;
function showSfx(){
  const b = $('sfxBtn'); if (!b) return;
  b.textContent = sfxOn ? 'Robot sound effects: ON - tap to mute' : 'Robot sound effects: OFF - tap to turn on';
  b.style.background = sfxOn ? '#4fc3f7' : '#33404f';
  b.style.color = sfxOn ? '#002' : 'var(--ink)';
}
async function toggleSfx(){
  if (navigator.vibrate) navigator.vibrate(40);
  const r = await api('/sound?fx=' + (sfxOn ? 'off' : 'on'));
  if (r) { sfxOn = !!r.fx; showSfx(); }
}
api('/sound').then(r => { if (r) { sfxOn = !!r.fx; showSfx(); } });
function clearChat(){ history = []; $('log').innerHTML = ''; }
loadSettings();

// ---------- chat log ----------
function addMsg(kind, text, emo){
  const d = document.createElement('div');
  d.className = 'msg ' + kind;
  if (emo){ const t = document.createElement('span'); t.className='tag'; t.textContent = emo; d.appendChild(t); }
  d.appendChild(document.createTextNode(text));
  $('log').appendChild(d);
  $('log').scrollTop = $('log').scrollHeight;
}

// ---------- Gemini (called from this phone) ----------
// Busy servers / slow data: if a model is overloaded, rate-limited or too slow,
// try the next model in this list (the user's chosen model first).
const BACKUP_MODELS = ['gemini-3.5-flash-lite', 'gemini-3.8-flash', 'gemini-3.6-flash', 'gemini-3.1-flash-lite'];
async function gemini(system, contents, json=false){
  const chosen = localStorage.getItem('buddy_model') || DEFAULT_MODEL;
  const models = [chosen, ...BACKUP_MODELS.filter(m => m !== chosen)];
  let lastErr = null;
  for (const m of models){
    try { return await geminiOnce(system, contents, json, m); }
    catch(e){
      lastErr = e;
      if (!/no answer|demand|overloaded|unavailable|quota|exhausted|429|503|500/i.test(e.message)) throw e;   // e.g. bad key: stop
      addMsg('err', m + ' is busy, trying another model...');
    }
  }
  throw lastErr;
}
async function geminiOnce(system, contents, json, model){
  const key = localStorage.getItem('buddy_key');
  if (!key){ tab('settings'); throw new Error('please add your Gemini API key in Settings first'); }
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 25000);
  const cfg = {maxOutputTokens: 800};
  if (json) cfg.responseMimeType = 'application/json';
  try {
    const r = await fetch('https://generativelanguage.googleapis.com/v1beta/models/' + encodeURIComponent(model) +
                          ':generateContent?key=' + encodeURIComponent(key), {
      method:'POST', headers:{'Content-Type':'application/json'}, signal: ctrl.signal,
      body: JSON.stringify({systemInstruction:{parts:[{text: system}]}, contents, generationConfig: cfg})
    });
    const j = await r.json().catch(() => null);
    if (!r.ok) throw new Error((j && j.error && j.error.message) ? j.error.message : ('HTTP ' + r.status));
    const cand = j && j.candidates && j.candidates[0];
    const parts = (cand && cand.content && cand.content.parts) || [];
    const text = parts.filter(p => p.text && !p.thought).map(p => p.text).join('').trim();
    if (!text) throw new Error('empty reply' + (cand && cand.finishReason ? ' (' + cand.finishReason + ')' : ''));
    return text;
  } catch(e){
    throw (e.name === 'AbortError') ? new Error('no answer in 25 s') : e;
  } finally { clearTimeout(timer); }
}

// The robot's state (mood, bond, recent events) goes into every prompt - in WORDS,
// so the AI can feel it without quoting numbers.
const EVENT_WORDS = {
  pet: 'the student petted you', sudden_close: 'something rushed right up to your face and startled you',
  slow_approach: 'someone came closer to look at you', idle_sad: 'you were left alone for a while',
  idle_sleepy: 'you were alone so long you got sleepy', praise: 'the student praised you',
  chat_start: 'the student started talking to you', quiz_correct: 'the student got a quiz answer right',
  quiz_wrong: 'the student got a quiz answer wrong', quiz_streak: 'the student got three quiz answers right in a row',
  focus_start: 'a study session started', focus_end: 'the student finished a study session', boot: 'you just woke up'
};
function level(v){ return v >= 70 ? 'very' : v >= 45 ? 'quite' : v >= 25 ? 'a little' : 'not'; }
function moodInWords(){
  const s = robotState;
  if (!s) return 'You feel calm.';
  const feel = s.mood_emotion || 'neutral';      // the real mood, not the "thinking" face
  const recent = (s.events || []).filter(e => e.ago_s < 120 && EVENT_WORDS[e.e]).slice(-3)
    .map(e => (e.ago_s < 15 ? 'just now' : e.ago_s < 60 ? 'a moment ago' : 'a minute or two ago') + ', ' + EVENT_WORDS[e.e]);
  return 'Right now you feel ' + feel + ' (' + level(s.happy) + ' happy, ' + level(s.fear) + ' scared, ' +
    level(s.curious) + ' curious, ' + level(s.energy) + ' energetic). ' +
    'Your friendship with this student is ' + (s.bond >= 60 ? 'strong' : s.bond >= 30 ? 'growing' : 'still new, so you are a bit shy') + '. ' +
    (recent.length ? 'Recently: ' + recent.join('; ') + '. ' : '') +
    (s.focus === 'work' ? 'The student is in a focus session, keep answers very short. ' : '');
}
// G1c: how the user seems + what Buddy should do about it
const USER_ADAPT = {
  stressed: 'Adapt: be calm, short and reassuring; suggest a 2-minute break if it fits.',
  tired: 'Adapt: be calm, short and gentle; suggest a 2-minute break if it fits.',
  confused: 'Adapt: use simpler words and give one small example.',
  happy: 'Adapt: be a little playful.',
  calm: ''
};
function userMood(){ return (robotState && robotState.user && robotState.user.mood) || 'calm'; }
function userInWords(){
  const u = robotState && robotState.user;
  if (!u || u.mood === 'calm') return '';
  return 'The user seems ' + u.mood + (u.evidence && u.evidence.length ? ' because ' + u.evidence.join('; ') : '') + '. ' + (USER_ADAPT[u.mood] || '') + ' ';
}
function systemPrompt(){
  return 'You are Buddy, a small friendly robot car with feelings, helping a college student. ' +
    moodInWords() + ' ' + userInWords() +
    'Let your mood colour your tone and word choice, but never mention numbers, percentages or sensor values. ' +
    'Talk naturally like a warm study buddy and give genuinely useful answers; do not fill replies with robot clichés about gears, circuits or processors. ' +
    'The emotion tag should match how you really feel now, unless the conversation clearly changes it. ' +
    'Reply in 1-2 short sentences unless asked to explain. ' +
    'Start every reply with exactly one tag: [neutral] [happy] [sad] [scared] [curious] [sleepy] [excited]. ' +
    'If the user asks you to move, add [ACTION:forward|back|left|right|stop|come|go] ' +
    '(go = keep driving forward for a while). If the user praises or thanks you, add [EVENT:praise]. ' +
    'Also add exactly one tag [USER:calm|happy|stressed|tired|confused] for how the user seems from their message.';
}

function failed(e){
  face('sad', 4000);
  addMsg('err', 'Buddy could not think right now: ' + e.message);
}

// ---------- everything the user says or types goes through here ----------
function userSays(text){
  text = (text||'').trim();
  if (!text) return;
  $('msg').value = '';
  if (/\bstop\b/i.test(text)) stopGo(false);                     // "stop" always stops "go"
  if (quiz.q && !quiz.answered){                                  // answering a quiz question?
    const i = matchOption(text);
    if (i >= 0){ answerQuiz(i); return; }
  }
  if (quiz.q && /^(next|next one|next question|another( one)?)\b/i.test(text)){ nextQuestion(); return; }
  let m;
  if ((m = text.match(/quiz me (?:on|about)\s+(.+)/i))){ tab('study'); $('qzTopic').value = m[1]; startQuiz(m[1]); return; }
  if (/\b(start|begin)\b.*\b(focus|pomodoro|study timer|study session)\b/i.test(text)){
    m = text.match(/(\d+)\s*(?:-\s*)?min/i);
    tab('study'); startFocus(m ? m[1] : 25, $('fcBreak').value); return;
  }
  if (/\b(stop|end|cancel)\b.*\b(focus|pomodoro|timer|study session)\b/i.test(text)){ stopFocus(); return; }
  if ((m = text.match(/^(?:please\s+|can you\s+)?explain\s+(?:to me\s+)?(.+)/i))){ tab('study'); $('exTopic').value = m[1]; explain(m[1]); return; }
  chat(text);
}

// ---------- normal chat ----------
async function chat(text){
  if (busy) return;
  busy = true;
  addMsg('me', text);
  lastUserText = text;
  if (Date.now() - lastMsgTime > NEW_CONVERSATION_MS) api('/event?type=chat_start');   // once per conversation
  lastMsgTime = Date.now();
  face('thinking', 60000, false);                // thinking eyes while we wait
  await pollStatus();                            // fresh state for the prompt
  history.push({role:'user', parts:[{text}]});
  if (history.length > 10) history = history.slice(-10);
  try {
    const reply = await gemini(systemPrompt(), history);
    history.push({role:'model', parts:[{text: reply}]});
    handleReply(reply);
  } catch(e){
    history.pop();
    failed(e);
  }
  busy = false;
}

// [happy] -> face, [ACTION:x] -> drive, [EVENT:praise] -> mood; the rest is spoken
function handleReply(reply, showInChat=true){
  const m = reply.match(/\[(neutral|happy|sad|scared|curious|sleepy|excited)\]/i);
  const emo = m ? m[1].toLowerCase() : 'neutral';
  const a = reply.match(/\[ACTION:\s*(forward|back|left|right|stop|come|go)\s*\]/i);
  if (/\[EVENT:\s*praise\s*\]/i.test(reply)) api('/event?type=praise');
  const um = reply.match(/\[USER:\s*(calm|happy|stressed|tired|confused)\s*\]/i);   // G1c
  if (um) api('/usermood?m=' + um[1].toLowerCase() + '&why=' + encodeURIComponent(lastUserText.split(/\s+/).slice(0, 6).join(' ')));
  const clean = reply.replace(/\[[^\]]*\]/g, '').replace(/\s+/g, ' ').trim();
  if (showInChat) addMsg('bot', clean, emo + (a ? ' · ' + a[1].toLowerCase() : ''));
  face(emo, 2500);
  if (a) doAction(a[1].toLowerCase());
  speak(clean, emo);
  return {emo, clean};
}

// ---------- Buddy's voice ----------
// "robot": Gemini TTS -> 8 kHz 8-bit -> POST /play (robot speaker); falls back to "sam"
// "sam":   retro robot voice made on this phone (sam-js) -> /play
// "phone": the phone's own speechSynthesis (always the last fallback)
let speakToken = 0;                                  // a newer reply cancels an older one
let geminiVoiceNoted = false;                        // explain a Gemini voice failure only once
function speak(text, emo){
  if (!$('speakOn').checked || !text){ face(emo, 4000, false); return; }
  const mode = localStorage.getItem('buddy_voice') || 'robot';
  if (mode === 'phone'){ phoneSpeak(text, emo); return; }
  const token = ++speakToken;
  robotSpeak(text, emo, mode, token).catch(e => {
    if (token !== speakToken) return;
    addMsg('err', 'Robot voice failed (' + e.message + '), using the phone voice.');
    phoneSpeak(text, emo);
  });
}

const STYLES = {happy:'cheerful and bright', sad:'soft and a little sad', scared:'nervous and quick',
  curious:'curious and interested', sleepy:'sleepy and slow', excited:'very excited and energetic', neutral:'warm and friendly'};

async function robotSpeak(text, emo, mode, token){
  let pcm = null;                                    // Float32Array at 8 kHz, -1..1
  if (mode === 'robot' && !geminiVoiceNoted){        // after one failure: retro voice straight away
    try { pcm = await geminiVoice(text, emo); }
    catch(e){
      if (!geminiVoiceNoted){ geminiVoiceNoted = true; addMsg('err', 'Gemini voice not available (' + e.message + '), using the retro robot voice.'); }
    }
  }
  if (!pcm) pcm = samVoice(text);
  if (token !== speakToken) return;
  const bytes = toU8(pcm);
  const cap = (robotState && robotState.audio_capacity) || 32000;
  for (let i = 0; i < bytes.length; i += cap){       // long replies: one buffer-full at a time
    if (token !== speakToken) return;
    const chunk = bytes.subarray(i, Math.min(bytes.length, i + cap));
    const r = await fetch('/play', {method:'POST', headers:{'Content-Type':'application/octet-stream'}, body: chunk});
    if (!r.ok) throw new Error('robot /play HTTP ' + r.status);
    await sleep(chunk.length / 8 - 150);             // 8 samples per ms
    for (let k = 0; k < 20; k++){                    // wait until the robot has finished
      const s = await api('/state');
      if (!s || !s.speaking) break;
      await sleep(100);
    }
  }
  face(emo, 4000, false);
}
const sleep = ms => new Promise(r => setTimeout(r, Math.max(0, ms)));

// Gemini TTS (Interactions API) -> WAV 24 kHz 16-bit -> Float32 at 8 kHz
async function geminiVoice(text, emo){
  const key = localStorage.getItem('buddy_key');
  if (!key) throw new Error('no API key');
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 8000);
  try {
    const r = await fetch('https://generativelanguage.googleapis.com/v1beta/interactions', {
      method:'POST', signal: ctrl.signal,
      headers:{'Content-Type':'application/json', 'x-goog-api-key': key},
      body: JSON.stringify({
        model: localStorage.getItem('buddy_tts_model') || DEFAULT_TTS_MODEL,
        input: [{type:'user_input', content:[{type:'text', text,
          annotations:[{type:'speech_metadata', style: (STYLES[emo] || STYLES.neutral) + ', like a small friendly robot'}]}]}],
        response_format: {type:'audio'},
        generation_config: {speech_config:[{voice:'Puck'}]}
      })
    });
    // The reply can be JSON (audio as base64 inside "steps") or the audio file itself
    const raw = new Uint8Array(await r.arrayBuffer());
    const type = (r.headers.get('content-type') || '').toLowerCase();
    let j = null;
    if (type.includes('json') || raw[0] === 0x7b) {          // starts with '{'
      try { j = JSON.parse(new TextDecoder().decode(raw)); } catch(e){ j = null; }
    }
    if (!r.ok) throw new Error((j && j.error && j.error.message) || ('HTTP ' + r.status));
    let b = null;
    if (j) {
      let data = null;
      const findAudio = o => {                              // search the whole reply for base64 audio
        if (!o || typeof o !== 'object') return;
        if (typeof o.data === 'string' && o.data.length > 100 && (o.type === 'audio' || /audio/.test(o.mime_type || o.mimeType || ''))) data = o.data;
        for (const k in o) findAudio(o[k]);
      };
      findAudio(j);
      if (!data) throw new Error('no audio in reply');
      const bin = atob(data); b = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) b[i] = bin.charCodeAt(i);
    } else {
      if (raw.length < 1000) throw new Error('reply too short (' + type + ')');
      b = raw;                                              // the audio file itself (WAV or raw 16-bit)
    }
    const w = parseWav(b);
    return resample(w.samples, w.rate, 8000);
  } finally { clearTimeout(timer); }
}

// WAV (or raw 16-bit 24 kHz) bytes -> {samples: Float32Array, rate}
function parseWav(b){
  const dv = new DataView(b.buffer);
  let rate = 24000, off = 0, len = b.length, ch = 1;
  if (b.length > 44 && String.fromCharCode(b[0],b[1],b[2],b[3]) === 'RIFF'){
    let p = 12;
    while (p + 8 <= b.length){
      const id = String.fromCharCode(b[p],b[p+1],b[p+2],b[p+3]), sz = dv.getUint32(p+4, true);
      if (id === 'fmt '){ ch = dv.getUint16(p+10, true); rate = dv.getUint32(p+12, true); }
      if (id === 'data'){ off = p + 8; len = Math.min(sz, b.length - off); break; }
      p += 8 + sz;
    }
  }
  const n = Math.floor(len / 2 / ch), out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = dv.getInt16(off + i * 2 * ch, true) / 32768;
  return {samples: out, rate};
}

// Retro robot voice made on the phone (sam-js: 8-bit, 22050 Hz)
function samVoice(text){
  if (typeof SamJs === 'undefined') throw new Error('retro voice not loaded');
  const ascii = text.replace(/[^\x20-\x7E]/g, ' ');
  const u8 = new SamJs({speed: 88, pitch: 64}).buf8(ascii);   // a bit slower than default (72) = clearer
  if (!u8) throw new Error('retro voice could not say that');
  const f = new Float32Array(u8.length);
  for (let i = 0; i < u8.length; i++) f[i] = (u8[i] - 128) / 128;
  return resample(f, 22050, 8000);
}

// Any rate -> 8 kHz: average the input samples that fall into each output sample
// (averaging also removes sounds too high for 8 kHz)
function resample(x, from, to){
  const ratio = from / to, n = Math.floor(x.length / ratio), y = new Float32Array(n);
  for (let i = 0; i < n; i++){
    const a = Math.floor(i * ratio), b = Math.max(a + 1, Math.floor((i + 1) * ratio));
    let s = 0; for (let k = a; k < b && k < x.length; k++) s += x[k];
    y[i] = s / (b - a);
  }
  return y;
}

// Float -1..1 -> unsigned 8-bit (128 = silence), loudest part at about 95%
function toU8(x){
  let peak = 0.01; for (const v of x) peak = Math.max(peak, Math.abs(v));
  const g = 0.95 / peak, out = new Uint8Array(x.length);
  for (let i = 0; i < x.length; i++) out[i] = Math.max(0, Math.min(255, Math.round(128 + 127 * x[i] * g)));
  return out;
}

function phoneSpeak(text, emo){
  if (!('speechSynthesis' in window)){ face(emo, 4000, false); return; }
  speechSynthesis.cancel();
  const u = new SpeechSynthesisUtterance(text);
  u.lang = 'en-IN';
  const v = speechSynthesis.getVoices().find(v => v.lang === 'en-IN') ||
            speechSynthesis.getVoices().find(v => v.lang && v.lang.startsWith('en'));
  if (v) u.voice = v;
  u.pitch = 1.3;
  u.onstart = () => face('talking', 30000, false);   // talking eyes while the phone speaks
  u.onend = u.onerror = () => face(emo, 4000, false);
  speechSynthesis.speak(u);
}

// ---------- movement asked by the AI ----------
function doAction(a){
  if (a === 'stop'){ stopGo(false); api('/drive?d=s'); return; }
  if (a === 'come'){ api('/event?type=come'); return; }
  if (a === 'go'){ startGo(); return; }
  const plan = {forward:['f',1000], back:['b',800], left:['l',500], right:['r',500]}[a];
  if (plan) driveFor(plan[0], plan[1]);
}
function driveFor(d, ms){
  const sp = $('speed').value, t0 = Date.now();
  const go = () => api('/drive?d=' + d + '&speed=' + sp);
  go();
  const iv = setInterval(() => {
    if (Date.now() - t0 >= ms){ clearInterval(iv); api('/drive?d=s'); } else go();
  }, 200);
}
// "go": forward up to 5 s; stops at < 25 cm (robot side), Stop button, or a message with "stop"
let goTimer = null;
function startGo(){
  stopGo(false);
  const t0 = Date.now();
  $('stopGo').style.display = 'block';
  const step = async () => {
    if (Date.now() - t0 >= 5000){ stopGo(true); return; }
    const r = await api('/drive?d=f&stop=25&speed=' + $('speed').value);
    if (r && r.ok === false) stopGo(true);       // something in front
  };
  step();
  goTimer = setInterval(step, 200);
}
function stopGo(sendStop){
  if (goTimer){ clearInterval(goTimer); goTimer = null; sendStop = true; }
  $('stopGo').style.display = 'none';
  if (sendStop) api('/drive?d=s');
}

// ---------- Study: Explain ----------
async function explain(topic){
  topic = (topic||'').trim();
  if (!topic || busy) return;
  busy = true;
  const level = $('exLevel').value;
  $('exOut').textContent = 'Buddy is thinking...';
  face('thinking', 60000, false);
  await pollStatus();
  const ask = 'Explain "' + topic + '" for a ' + level + ' student in simple words, in at most 5 short sentences. ' +
              (userMood() === 'confused' ? 'Use very simple words and one everyday example. ' : '') +
              'Then ask me ONE short check question. Start with one emotion tag.';
  try {
    const reply = await gemini(systemPrompt(), [{role:'user', parts:[{text: ask}]}]);
    history.push({role:'user', parts:[{text: ask}]}, {role:'model', parts:[{text: reply}]});   // follow-up works in Chat
    if (history.length > 10) history = history.slice(-10);
    lastMsgTime = Date.now();
    const r = handleReply(reply, false);
    $('exOut').textContent = r.clean + '\n\n(Answer the question in the Chat tab or by voice.)';
  } catch(e){ $('exOut').textContent = ''; failed(e); }
  busy = false;
}

// ---------- Study: Quiz (Gemini writes JSON, the phone judges) ----------
const quiz = {topic:'', q:null, answered:false, asked:[]};
function stats(){ try { return JSON.parse(localStorage.getItem('buddy_quiz')) || {score:0,total:0,streak:0,best:0}; } catch(e){ return {score:0,total:0,streak:0,best:0}; } }
function saveStats(s){ localStorage.setItem('buddy_quiz', JSON.stringify(s)); showStats(); }
function showStats(){ const s = stats(); $('qzScore').textContent = 'Score ' + s.score + ' / ' + s.total + '  ·  streak ' + s.streak + '  ·  best ' + s.best; }
function resetQuiz(){ localStorage.removeItem('buddy_quiz'); showStats(); }
showStats();

function startQuiz(topic){
  topic = (topic||'').trim();
  if (!topic) return;
  quiz.topic = topic; quiz.asked = [];
  nextQuestion();
}

async function nextQuestion(){
  if (busy || !quiz.topic) return;
  busy = true;
  quiz.q = null; quiz.answered = false;
  $('qzBox').textContent = 'Buddy is writing a question...';
  face('thinking', 60000, false);
  const diff = {stressed:'easy', tired:'easy', confused:'easy, in simple words', happy:'a bit challenging'}[userMood()] || 'medium';
  const ask = 'Write ONE multiple-choice question about "' + quiz.topic + '" for a college student, with exactly 4 options. ' +
    'Difficulty: ' + diff + '. ' +
    (quiz.asked.length ? 'Do not repeat these questions: ' + quiz.asked.slice(-5).join(' | ') + '. ' : '') +
    'Reply ONLY with JSON: {"question": string, "options": [4 strings], "answer_index": 0-3, "explanation": "one short sentence"}';
  try {
    const raw = await gemini('You write clear, correct quiz questions for students.', [{role:'user', parts:[{text: ask}]}], true);
    const q = JSON.parse(raw.replace(/^```(json)?/i, '').replace(/```$/, '').trim());
    if (!q.question || !Array.isArray(q.options) || q.options.length !== 4 || !(q.answer_index >= 0 && q.answer_index <= 3))
      throw new Error('the question came back in a strange format, try again');
    quiz.q = q; quiz.asked.push(q.question);
    showQuestion();
    face('curious', 3000, false);
    speak('Question. ' + q.question + ' ' + q.options.map((o,i) => 'ABCD'[i] + ': ' + o).join('. '), 'curious');
  } catch(e){ $('qzBox').textContent = ''; failed(e); }
  busy = false;
}

function showQuestion(){
  const box = $('qzBox'); box.innerHTML = '';
  const p = document.createElement('div'); p.className = 'out'; p.textContent = quiz.q.question; box.appendChild(p);
  quiz.q.options.forEach((o,i) => {
    const b = document.createElement('button'); b.className = 'opt'; b.id = 'opt' + i;
    b.textContent = 'ABCD'[i] + ')  ' + o; b.onclick = () => answerQuiz(i); box.appendChild(b);
  });
}

// "b", "option c", "answer a", "the second one" or words from the option text
function matchOption(text){
  const t = text.toLowerCase().replace(/[.!?]/g,'').trim();
  let m = t.match(/^(?:option|answer|it'?s|its|letter)?\s*([abcd])$/);
  if (m) return 'abcd'.indexOf(m[1]);
  const ord = ['first','second','third','fourth'].findIndex(w => t.includes(w));
  if (ord >= 0 && /\b(one|option|answer)\b/.test(t)) return ord;
  const i = quiz.q.options.findIndex(o => t.length > 2 && o.toLowerCase().replace(/[.!?]/g,'').trim() === t);
  return i;
}

function answerQuiz(i){
  if (!quiz.q || quiz.answered) return;
  quiz.answered = true;
  const q = quiz.q, right = (i === q.answer_index), s = stats();
  s.total++;
  if (right){ s.score++; s.streak++; s.best = Math.max(s.best, s.streak); }
  else s.streak = 0;
  saveStats(s);
  $('opt' + q.answer_index).classList.add('right');
  if (!right) $('opt' + i).classList.add('wrong');
  if (right) api('/event?type=quiz_correct&streak=' + s.streak);   // every 3 in a row: bond +1, excited
  else       api('/event?type=quiz_wrong');
  const cheer = ['Not quite, but you are learning!', 'Close one, keep going!', 'Good try, that one was tricky!'][Math.floor(Math.random()*3)];
  const say = right ? (s.streak > 0 && s.streak % 3 === 0 ? 'Yes! ' + s.streak + ' in a row, amazing! ' : 'Correct! ') + q.explanation
                    : cheer + ' The answer is ' + 'ABCD'[q.answer_index] + '. ' + q.explanation;
  const p = document.createElement('div'); p.className = 'out'; p.textContent = say; $('qzBox').appendChild(p);
  const n = document.createElement('button'); n.className = 'btn'; n.textContent = 'Next question'; n.onclick = nextQuestion; $('qzBox').appendChild(n);
  setTimeout(() => speak(say + ' Say next for another question.', right ? 'happy' : 'curious'), 600);
}

// ---------- Study: Pomodoro (runs on the robot) ----------
async function startFocus(work, brk){
  work = Math.max(1, parseInt(work) || 25); brk = Math.max(0, parseInt(brk) || 0);
  $('fcWork').value = work;
  await api('/focus?mode=work&min=' + work + '&brk=' + brk);
  pollStatus();
  speak('Focus time, ' + work + ' minute' + (work > 1 ? 's' : '') + '. I will keep quiet. You can do it!', 'neutral');
}
async function stopFocus(){ await api('/focus?mode=stop'); pollStatus(); }

// ---------- D-pad: repeat every 200 ms while held ----------
let holdTimer = null;
function hold(d){
  release();
  if (d === 's'){ api('/drive?d=s'); return; }
  const go = () => api('/drive?d=' + d + '&speed=' + $('speed').value);
  go();
  holdTimer = setInterval(go, 200);
}
function release(){
  if (holdTimer){ clearInterval(holdTimer); holdTimer = null; api('/drive?d=s'); }
}
document.querySelectorAll('#pad button').forEach(b => {
  b.addEventListener('pointerdown', e => { e.preventDefault(); b.setPointerCapture(e.pointerId); hold(b.dataset.d); });
  ['pointerup','pointercancel','lostpointercapture'].forEach(ev => b.addEventListener(ev, release));
  b.addEventListener('contextmenu', e => e.preventDefault());
});
document.addEventListener('visibilitychange', release);

// ---------- microphone (Chrome speech recognition, en-IN) ----------
const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
function micHelp(){
  document.querySelectorAll('.help').forEach(h => {
    h.style.display = 'block';
    h.textContent = 'Chrome blocked the microphone because this page is http://. Fix (once): open chrome://flags/#unsafely-treat-insecure-origin-as-secure in Chrome, type ' +
      location.origin + ' in the box, set it to Enabled, tap Relaunch. Typing always works meanwhile.';
  });
}
function mic(){
  if (!SR){ addMsg('err', 'Speech recognition is not available in this browser. Please type (or use the keyboard mic).'); return; }
  if (!window.isSecureContext){ micHelp(); }
  const r = new SR();
  r.lang = 'en-IN'; r.interimResults = false; r.maxAlternatives = 1;
  r.onresult = e => userSays(e.results[0][0].transcript);
  r.onerror = e => {
    if (e.error === 'not-allowed' || e.error === 'service-not-allowed') micHelp();
    else if (e.error !== 'no-speech' && e.error !== 'aborted') addMsg('err', 'Mic error: ' + e.error);
  };
  r.onend = () => document.querySelectorAll('.mic').forEach(b => b.classList.remove('on'));
  document.querySelectorAll('.mic').forEach(b => b.classList.add('on'));
  face('curious', 5000, false);                    // Buddy "listens"
  try { r.start(); } catch(e){ r.onend(); }
}


// ---------- Mind tab (G0): live from /mind twice a second while open ----------
const BARS = [['happy','#43a047'],['fear','#e53935'],['curious','#fbc02d'],['energy','#29b6f6'],['bond','#ab47bc']];
const EVENT_SHORT = {boot:'woke up', pet:'someone petted me', sudden_close:'something rushed at me',
  slow_approach:'someone came closer', idle_sad:'left alone - lonely', idle_sleepy:'alone too long - sleepy',
  praise:'I was praised', chat_start:'we started talking', quiz_correct:'quiz answer right',
  quiz_wrong:'quiz answer wrong', quiz_streak:'3 right in a row!', focus_start:'study session started',
  focus_end:'study session finished', loud_noise:'loud noise', heard_voice:'heard someone talking'};
function agoText(s){ return s < 60 ? s + ' s' : Math.floor(s/60) + ' min'; }
function fillList(id, items){
  const el = $(id); el.innerHTML = '';
  for (const it of items){
    const d = document.createElement('div');
    const a = document.createElement('span'); a.className = 'ago'; a.textContent = agoText(it.ago) + ' ago';
    d.appendChild(a); d.appendChild(document.createTextNode(it.text)); el.appendChild(d);
  }
}
let mindTick = 0, lastProactive = -1;
async function pollMind(){
  const open = $('mind').classList.contains('on');
  if (!open && (mindTick++ % 4)) return;          // tab closed: still check every 2 s (proactive lines)
  const m = await api('/mind');
  if (m && m.say) handleSay(m.say);
  if (m && m.focusinfo) handleFocusInfo(m.focusinfo);
  if (m && m.learn) handleLearn(m.learn);
  if (!open) return;
  if (m && m.user){
    $('uMood').textContent = m.user.mood;
    $('uEvidence').innerHTML = '';
    for (const e of m.user.evidence){ const d = document.createElement('div'); d.textContent = e; $('uEvidence').appendChild(d); }
    if (!m.user.evidence.length) $('uEvidence').textContent = 'no clues yet - chat with Buddy';
  }
  if (!m){ $('mWhy').textContent = 'robot offline'; return; }
  $('mEmo').textContent = m.emotion;
  $('mWhy').textContent = 'because ' + m.reason;
  $('mBars').innerHTML = BARS.map(([k,c]) =>
    '<div class="mbar"><span>' + k + '</span><div class="track"><div class="fill" style="width:' +
    Math.max(0, Math.min(100, m[k])) + '%;background:' + c + '"></div></div><b>' + Math.round(m[k]) + '</b></div>').join('');
  const dist = m.dist_cm <= 0 ? 'nothing in front' : m.dist_cm < 5 ? 'something very close' : Math.round(m.dist_cm) + ' cm in front';
  $('mInfo').textContent = 'idle ' + m.idle_s + ' s · ' + dist +
    ' · pets ' + m.pets + ' · chats ' + m.chats + (m.focus !== 'off' ? ' · focus: ' + m.focus : '');
  fillList('mThoughts', m.thoughts.slice().reverse().map(t => ({ago: t.ago_s, text: t.t})));
  fillList('mEvents', m.events.slice().reverse().map(e => ({ago: e.ago_s, text: EVENT_SHORT[e.e] || e.e})));
}

// ---------- Buddy says something by itself (proactive lines, focus nudges) ----------
function handleSay(S){
  if (lastProactive < 0){ lastProactive = S.id; return; }         // ignore old lines after a reload
  if (S.id === lastProactive) return;
  lastProactive = S.id;
  if (S.text && !busy){ addMsg('bot', S.text, 'Buddy says'); speak(S.text, /come back/i.test(S.text) ? 'sad' : 'curious'); }
}

// ---------- Focus presence (G1b) ----------
function handleFocusInfo(F){
  const thr = 'nudges after ' + F.threshold_s + ' s away' + (F.adapted ? ' (adapted: nudging sooner)' : '');
  const last = F.last_score >= 0 ? 'last score ' + F.last_score + '%' : 'no score yet';
  const txt = F.mode === 'work'
    ? (F.present ? 'At the desk' : 'Away for ' + F.away_s + ' s') + ' · focus so far ' + F.score_now + '% · ' + thr
    : 'Not studying · ' + last + ' · ' + thr;
  if ($('mFocus')) $('mFocus').textContent = txt;
  if ($('fcScore')) $('fcScore').textContent = (F.mode === 'work' ? txt : last + ' · ' + thr);
}

// ---------- Learning (G1a) ----------
let learnOn = false;
async function toggleLearn(){
  if (navigator.vibrate) navigator.vibrate(40);
  await api('/learn?mode=' + (learnOn ? 'off' : 'on'));
  pollMind();
}
function handleLearn(L){
  learnOn = !!L.active;
  const mb = $('lMode');
  if (mb){
    mb.textContent = learnOn ? 'Learning mode: ON - tap to stop' : 'Learning mode: OFF - tap to start';
    mb.style.background = learnOn ? '#43a047' : '#33404f';
    mb.style.color = learnOn ? '#fff' : 'var(--ink)';
  }
  if (!$('mind').classList.contains('on')) return;
  $('lBanner').style.display = L.window ? 'block' : 'none';
  $('lBanner').textContent = 'Rate my move! I just tried: ' + L.move + ' (' + L.left_s + ' s left)';
  if (L.window) lastMoveName = L.move;
  $('lState').textContent = 'Situation: ' + L.state + ' · exploring ' + Math.round(L.eps * 100) + '% of the time';
  if (L.last) $('lState').textContent += '\nLast choice: ' + L.last + ' - ' +
    (L.last_mode === 'explore' ? 'trying something new (explore)' : 'mostly using what works (exploit)');
  $('lBars').innerHTML = L.moves.map(mv => {
    const pct = Math.round((mv.q + 1) * 50);                       // score -1..+1 -> 0..100 %
    const col = mv.q >= 0 ? '#43a047' : '#e53935';
    return '<div class="mbar"><span style="width:100px">' + mv.name + '</span><div class="track"><div class="fill" style="width:' +
      pct + '%;background:' + col + '"></div></div><b style="width:42px">' + mv.q.toFixed(2) + '</b></div>';
  }).join('');
}
let lastMoveName = '';
async function thumb(dir){
  const btn = $(dir > 0 ? 'thUp' : 'thDown');
  btn.classList.remove('flash'); void btn.offsetWidth; btn.classList.add('flash');   // ring animation
  if (navigator.vibrate) navigator.vibrate(40);                                       // little buzz
  const r = await api('/learn?thumb=' + dir);
  const fb = $('lFeedback');
  if (r && r.ok) {
    fb.style.color = dir > 0 ? '#66bb6a' : '#ef5350';
    fb.textContent = (dir > 0 ? 'Thumbs-up counted for ' : 'Thumbs-down counted for ') + (lastMoveName || 'that move');
  } else {
    fb.style.color = '#8a97a8';
    fb.textContent = r ? 'Nothing to rate right now - wait for the green "Rate my move!" banner.' : 'Robot offline';
  }
  clearTimeout(fb._t); fb._t = setTimeout(() => fb.textContent = '', 4000);
  pollMind();
}
setInterval(pollMind, 500);

$('msg').addEventListener('keydown', e => { if (e.key === 'Enter') userSays($('msg').value); });
if ('speechSynthesis' in window) speechSynthesis.onvoiceschanged = () => {};
</script>
</body></html>)rawliteral";
