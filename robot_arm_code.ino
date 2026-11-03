/*
  ROBOT ARM - 5 SERVOS (ESP32)       file/folder name: arm_5_joints
  Library: ESP32Servo

    Joint 1  Shoulder     MG995  GPIO 13
    Joint 2  Elbow        MG995  GPIO 14
    Joint 3  Wrist        SG90   GPIO 27
    Joint 4  Wrist rotate SG90   GPIO 26
    Joint 5  Gripper      MG90S  GPIO 25   (open / close only)

  Joints 1-4: the saved MIN position counts as 0 degrees and the angle grows
  toward MAX (works even if MIN is the bigger raw number).
  Gripper: save an OPEN angle and a CLOSE angle, then use Open / Close.

  Servos stay OFF at boot. Press "Attach" for the joint you want to test.
  Servo power from the buck, common GND with the ESP32.

  WiFi: "RobotArm" / 12345678  ->  http://192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESP32Servo.h>

const int NJ = 5;
const int GRIP = 4;                               // index of the gripper
const int PIN[NJ] = {13, 14, 27, 26, 25};

// ---------- DEFAULT RAW VALUES (software angles 0..180) ----------
//                         J1    J2   J3   J4   Grip
const int DEF_MIN[NJ]  = { 180,   0,   0,   0,  40 };  // gripper: CLOSE angle
const int DEF_HOME[NJ] = { 150,  90,  90,  90, 110 };
const int DEF_MAX[NJ]  = {  90, 180, 180, 180, 110 };  // gripper: OPEN angle
// Joint 1 = your tested values. The rest are placeholders: set the real
// ones on the page (Save buttons, or type the values and press Apply).
// -----------------------------------------------------------------

// Delay per 1 degree (bigger = slower, gentler on weight and power)
const int STEP_MS[NJ] = {35, 35, 20, 20, 15};

Servo servo[NJ];
Preferences prefs;
WebServer server(80);

int cur[NJ], tgt[NJ], homeA[NJ], minA[NJ], maxA[NJ];
bool att[NJ] = {false, false, false, false, false};
unsigned long last[NJ] = {0, 0, 0, 0, 0};

int dirSign(int j) { return (maxA[j] < minA[j]) ? -1 : 1; }

void saveAll(int j) {
  prefs.putInt(("h" + String(j)).c_str(), homeA[j]);
  prefs.putInt(("n" + String(j)).c_str(), minA[j]);
  prefs.putInt(("x" + String(j)).c_str(), maxA[j]);
}

int J() { return constrain(server.arg("j").toInt(), 0, NJ - 1); }

void sendState(bool ok = true) {
  String s = "{\"ok\":" + String(ok ? 1 : 0) + ",\"j\":[";
  for (int j = 0; j < NJ; j++) {
    s += "{\"cur\":" + String(cur[j]) + ",\"tgt\":" + String(tgt[j]) +
         ",\"home\":" + String(homeA[j]) + ",\"min\":" + String(minA[j]) +
         ",\"max\":" + String(maxA[j]) + ",\"att\":" + String(att[j] ? 1 : 0) + "}";
    if (j < NJ - 1) s += ",";
  }
  s += "]}";
  server.send(200, "application/json", s);
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0c2f57">
<title>Robot arm</title>
<style>
:root{--bg:#0c2f57;--ln:#5f8fc4;--tx:#eaf3ff;--mu:#9bb8d9;--or:#ff7a2f;--gr:#5fe0b0}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:auto;max-width:480px;padding:14px 14px 40px;color:var(--tx);background:var(--bg);font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
background-image:linear-gradient(#ffffff0d 1px,transparent 1px),linear-gradient(90deg,#ffffff0d 1px,transparent 1px);background-size:24px 24px}
h1{font-size:20px;margin:0 0 8px}
.mu{color:var(--mu);font-size:14px}
.tabs{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-bottom:8px}
.tabs button{padding:8px 0;font-size:14px;line-height:1.25}
.tabs button small{font-size:15px;font-weight:700}
.tabs button.on{background:var(--or);border-color:var(--or);color:#2a1100}
#att{width:100%;margin:8px 0 4px;padding:16px;font-size:17px}
#att.on{background:var(--gr);border-color:var(--gr);color:#032a22}
.big{text-align:center;margin:12px 0 0}
.big b{font-size:76px;line-height:1;color:var(--or);font-variant-numeric:tabular-nums}
.big span{font-size:30px;color:var(--or)}
.sec{border-top:1px solid var(--ln);margin-top:14px;padding-top:12px}
.sec h2{font-size:16px;margin:0 0 8px}
.row{display:flex;gap:8px;margin:8px 0}
button{flex:1;font:600 17px system-ui,sans-serif;color:var(--tx);background:#ffffff14;border:1.5px solid var(--ln);border-radius:12px;padding:15px 4px}
button:active{transform:scale(.96);background:#ffffff2a}
.dn{border-color:#9bb8d9}.up{background:#ff7a2f33;border-color:var(--or)}
.sm{font-size:14px;padding:12px 4px;font-weight:500}
.huge{padding:22px 4px;font-size:20px}
input[type=number]{width:100%;font:600 18px system-ui,sans-serif;padding:12px 4px;border-radius:10px;border:1.5px solid var(--ln);background:#0a2745;color:var(--tx);text-align:center}
.f{flex:1;font-size:13px;color:var(--mu)}
table{width:100%;border-collapse:collapse;font-size:15px}
td,th{padding:6px 4px;text-align:left;border-bottom:1px solid #5f8fc455}
th{color:var(--mu);font-weight:500;font-size:13px}
#toast{position:fixed;top:10px;left:50%;transform:translateX(-50%);background:var(--gr);color:#032a22;padding:9px 16px;border-radius:20px;font-weight:600;display:none}
</style></head><body>
<h1>Robot arm</h1>
<div class="tabs" id="tabs"></div>
<div class="mu" id="st">Connecting…</div>
<button id="att" onclick="toggle()">Attach servo</button>

<div class="big"><b id="ang">--</b><span>°</span></div>
<div class="mu" id="raw" style="text-align:center"></div>

<div class="sec"><h2>Nudge (tap once = one move)</h2>
 <div class="row"><button class="dn" onclick="n(-10)">−10</button><button class="dn" onclick="n(-5)">−5</button><button class="dn" onclick="n(-2)">−2</button><button class="dn" onclick="n(-1)">−1</button></div>
 <div class="row"><button class="up" onclick="n(1)">+1</button><button class="up" onclick="n(2)">+2</button><button class="up" onclick="n(5)">+5</button><button class="up" onclick="n(10)">+10</button></div>
 <div class="mu" id="hint"></div></div>

<div id="jpart">
<div class="sec"><h2>Save this position as</h2>
 <div class="row"><button class="sm" onclick="sv('min')">Min (0°)</button><button class="sm" onclick="sv('home')">Home</button><button class="sm" onclick="sv('max')">Max</button></div>
 <div class="row"><button class="sm" onclick="go('min')">Go min</button><button class="sm" onclick="go('home')">Go home</button><button class="sm" onclick="go('max')">Go max</button></div></div>
<div class="sec"><h2>Type the values (software angles)</h2>
 <div class="row"><div class="f">Zero at<input type="number" id="zi" min="0" max="180"></div><div class="f">Home at<input type="number" id="hi" min="0" max="180"></div><div class="f">Max at<input type="number" id="mi" min="0" max="180"></div></div>
 <div class="row"><button class="sm" onclick="ap()">Apply values</button></div></div>
<div class="sec"><h2>Saved results</h2>
 <table><tr><th></th><th>Joint angle</th><th>Raw servo</th></tr>
 <tr><td>Min</td><td id="a0"></td><td id="r0"></td></tr>
 <tr><td>Home</td><td id="a1"></td><td id="r1"></td></tr>
 <tr><td>Max</td><td id="a2"></td><td id="r2"></td></tr></table></div>
</div>

<div id="gpart" style="display:none">
<div class="sec"><h2>Gripper</h2>
 <div class="row"><button class="huge" onclick="gr(1)">Open</button><button class="huge up" onclick="gr(0)">Close</button></div>
 <div class="mu" id="gstate"></div></div>
<div class="sec"><h2>Save this position as</h2>
 <div class="row"><button class="sm" onclick="sv('open')">Open angle</button><button class="sm" onclick="sv('close')">Close angle</button></div>
 <div class="mu">Use the nudge buttons to find the angles. Close it gently on an object, not against the jaws.</div></div>
<div class="sec"><h2>Type the values (software angles)</h2>
 <div class="row"><div class="f">Open at<input type="number" id="oi" min="0" max="180"></div><div class="f">Close at<input type="number" id="ci" min="0" max="180"></div></div>
 <div class="row"><button class="sm" onclick="ap()">Apply values</button></div></div>
</div>

<div class="sec"><div class="row"><button class="sm" onclick="rs()">Reset this joint to code defaults</button></div></div>
<div id="toast"></div>
<script>
const NAMES=["J1","J2","J3","J4","Grip"],$=i=>document.getElementById(i);
let S=null,sel=0,tt,filled=0;
$("tabs").innerHTML=NAMES.map((n,i)=>`<button id="t${i}" onclick="pick(${i})">${n}<br><small>--</small></button>`).join("");
function toast(m){const t=$("toast");t.innerText=m;t.style.display="block";clearTimeout(tt);tt=setTimeout(()=>t.style.display="none",1500);}
function ja(v,d,i){if(i==4)return v;const s=d.max<d.min?-1:1;return s*(v-d.min);}
function pick(i){sel=i;filled=0;if(S)render(S);}
function render(st){
 S=st;const d=st.j[sel],g=sel==4;
 if(!filled){$("zi").value=d.min;$("hi").value=d.home;$("mi").value=d.max;$("ci").value=d.min;$("oi").value=d.max;filled=1;}
 for(let i=0;i<5;i++){const e=$("t"+i);e.className=(i==sel?"on":"");e.innerHTML=NAMES[i]+"<br><small>"+ja(st.j[i].cur,st.j[i],i)+"°</small>";}
 $("jpart").style.display=g?"none":"block";$("gpart").style.display=g?"block":"none";
 $("ang").innerText=ja(d.cur,d,sel);$("raw").innerText=g?"Gripper servo angle":"Raw servo angle: "+d.cur+"°";
 $("hint").innerText=g?"Nudge changes the servo angle directly.":"− goes toward min (0°), + goes toward max.";
 if(!g){$("a0").innerText="0°";$("a1").innerText=ja(d.home,d,sel)+"°";$("a2").innerText=ja(d.max,d,sel)+"°";$("r0").innerText=d.min;$("r1").innerText=d.home;$("r2").innerText=d.max;}
 else{$("gstate").innerText="Saved: open "+d.max+"°, close "+d.min+"°"+(d.cur==d.max?"  (open)":d.cur==d.min?"  (closed)":"");}
 const nm=g?"gripper":"joint "+(sel+1);
 $("att").className=d.att?"on":"";$("att").innerText=d.att?nm+" servo ON (tap to release)":"Attach "+nm+" servo";
 $("st").innerText=d.att?(d.cur!=d.tgt?"Moving to "+ja(d.tgt,d,sel)+"°":"Holding position"):"Servo off. The horn moves freely.";
}
function api(u,m){u+=(u.includes("?")?"&":"?")+"j="+sel;return fetch(u).then(r=>r.json()).then(d=>{render(d);if(m)toast(d.ok?m:"Not allowed");return d;}).catch(()=>{$("st").innerText="Offline: reconnect to RobotArm WiFi";});}
function need(){if(!S||!S.j[sel].att){toast("Attach the servo first");return 1;}}
function toggle(){api("/att?on="+(S&&S.j[sel].att?0:1));}
function n(d){if(need())return;api("/nudge?d="+d);}
function go(w){if(need())return;api("/go?w="+w);}
function gr(o){if(need())return;api("/grip?open="+o);}
function sv(w){filled=0;api("/save?w="+w,w+" saved");}
function rs(){if(confirm("Reset this joint to the values written in the code?")){filled=0;api("/reset","Reset done");}}
function ap(){filled=0;if(sel==4)api("/setrange?min="+$("ci").value+"&home="+$("oi").value+"&max="+$("oi").value,"Applied");
 else api("/setrange?min="+$("zi").value+"&home="+$("hi").value+"&max="+$("mi").value,"Applied");}
api("/state");setInterval(()=>api("/state"),600);
</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  prefs.begin("arm2", false);        // same storage as before: saved joint 1 and 2 values are kept
  for (int t = 0; t < 4; t++) ESP32PWM::allocateTimer(t);
  for (int j = 0; j < NJ; j++) {
    homeA[j] = prefs.getInt(("h" + String(j)).c_str(), DEF_HOME[j]);
    minA[j]  = prefs.getInt(("n" + String(j)).c_str(), DEF_MIN[j]);
    maxA[j]  = prefs.getInt(("x" + String(j)).c_str(), DEF_MAX[j]);
    cur[j] = tgt[j] = (j == GRIP) ? maxA[j] : homeA[j];
    servo[j].setPeriodHertz(50);
    // Servos are NOT attached here, so nothing moves at boot.
  }

  WiFi.softAP("RobotArm", "12345678");
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/state", []() { sendState(); });

  server.on("/att", []() {
    int j = J();
    if (server.arg("on") == "1" && !att[j]) {
      cur[j] = tgt[j] = (j == GRIP) ? maxA[j] : homeA[j];   // home (gripper: open)
      servo[j].attach(PIN[j], 500, 2400);
      servo[j].write(cur[j]);
      att[j] = true;
    } else if (server.arg("on") == "0" && att[j]) {
      servo[j].detach();
      att[j] = false;
    }
    sendState();
  });

  // d: joints 1-4 in joint degrees (+ toward max), gripper in raw degrees
  server.on("/nudge", []() {
    int j = J();
    int d = server.arg("d").toInt();
    if (att[j]) tgt[j] = constrain(tgt[j] + ((j == GRIP) ? d : dirSign(j) * d), 0, 180);
    sendState();
  });

  server.on("/go", []() {
    int j = J();
    String w = server.arg("w");
    if (att[j] && j != GRIP) tgt[j] = (w == "min") ? minA[j] : (w == "max") ? maxA[j] : homeA[j];
    sendState();
  });

  server.on("/grip", []() {
    if (att[GRIP]) tgt[GRIP] = (server.arg("open") == "1") ? maxA[GRIP] : minA[GRIP];
    sendState();
  });

  server.on("/save", []() {
    int j = J();
    String w = server.arg("w");
    bool ok = true;
    if (j == GRIP) {
      if (w == "open") { maxA[j] = tgt[j]; homeA[j] = tgt[j]; }
      else if (w == "close") minA[j] = tgt[j];
      else ok = false;
    } else {
      if (w == "home") homeA[j] = tgt[j];
      else if (w == "min") { if (tgt[j] != maxA[j]) minA[j] = tgt[j]; else ok = false; }
      else if (w == "max") { if (tgt[j] != minA[j]) maxA[j] = tgt[j]; else ok = false; }
    }
    if (ok) saveAll(j);
    sendState(ok);
  });

  server.on("/setrange", []() {
    int j = J();
    int mn  = constrain(server.arg("min").toInt(), 0, 180);
    int hm  = constrain(server.arg("home").toInt(), 0, 180);
    int mxv = constrain(server.arg("max").toInt(), 0, 180);
    bool ok = (j == GRIP) ? true : (mn != mxv);
    if (ok) { minA[j] = mn; homeA[j] = hm; maxA[j] = mxv; saveAll(j); }
    sendState(ok);
  });

  server.on("/reset", []() {
    int j = J();
    homeA[j] = DEF_HOME[j]; minA[j] = DEF_MIN[j]; maxA[j] = DEF_MAX[j];
    saveAll(j);
    sendState();
  });

  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  for (int j = 0; j < NJ; j++) {
    if (!att[j] || now - last[j] < (unsigned long)STEP_MS[j]) continue;
    last[j] = now;
    if (cur[j] < tgt[j]) { cur[j]++; servo[j].write(cur[j]); }
    else if (cur[j] > tgt[j]) { cur[j]--; servo[j].write(cur[j]); }
  }
}
