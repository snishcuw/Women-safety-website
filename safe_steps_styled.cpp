// =============================================================================
//  Safe Steps - women safety website, written in C++17
//
//  One file, no third-party libraries. It contains:
//    * a small multi-threaded HTTP server (POSIX sockets)
//    * the website itself (HTML/CSS embedded below)
//    * smart triage logic ("What's happening?")              -> POST /triage
//    * a hand-written PDF generator for the safety plan       -> POST /plan
//
//  Colours: Blush Rose #DF4C74, Tomato Jam #C42B34, Vanilla Custard #FCE9AB,
//           Princeton Orange #FC8A2D, Olive #9E9820 (page and PDF share the palette).
//
//  Build:   g++ -std=c++17 -O2 -pthread safe_steps_styled.cpp -o safe_steps
//  Run:     ./safe_steps              (http://127.0.0.1:8080, this computer only)
//           ./safe_steps 9000 --lan   (custom port, reachable from your phone on Wi-Fi)
//
//  Privacy: nothing is stored and nothing is logged. Answers are used to build
//  the response and then discarded.
//
//  Note: the fake-call ringtone, the location lookup and the Quick Exit screen
//  need browser APIs, so a small script embedded in the page handles those.
//  Phones only allow location on HTTPS or localhost, so for the location alert
//  put the server behind an HTTPS proxy (or demo on the same machine).
// =============================================================================

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
//  The page (served at GET /)
// ---------------------------------------------------------------------------
const std::string PAGE = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#C42B34">
<meta name="color-scheme" content="light dark">
<title>Safe Steps – Women Safety</title>
<style>
/* ---------- Palette: Blush Rose, Tomato Jam, Vanilla Custard, Princeton Orange, Olive ---------- */
:root{
  --blush:#DF4C74; --tomato:#C42B34; --vanilla:#FCE9AB; --orange:#FC8A2D; --olive:#9E9820;
  --olive-deep:#6E6A14;      /* darker olive, for text and buttons that need contrast */
  --on-warm:#3A1014;         /* deep wine: text that sits on orange, olive or vanilla */

  /* light theme */
  --bg:#FFF6DC; --panel:#FFFDF5; --band:#FCE9AB; --field:#FFFDF5;
  --ink:#3A1014; --muted:#7A4A44; --heading:#C42B34;
  --line:#E8D18A; --edge:#B08F2C;
  --err:#B3202A; --okc:#6E6A14; --focus:#3A1014;

  --display:"Didot","Bodoni 72","Bodoni MT","Playfair Display",Georgia,"Times New Roman",serif;
  --sans:system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;

  box-sizing:border-box;
  padding-top:env(safe-area-inset-top,0px);
  padding-bottom:env(safe-area-inset-bottom,0px);
}
@media (prefers-color-scheme:dark){
  :root:not([data-theme="light"]){
    --bg:#220A0E; --panel:#31111A; --band:#3A1620; --field:#2A0D13;
    --ink:#FDEFC8; --muted:#D9BBA4; --heading:#F0708F;
    --line:#5B2A33; --edge:#9A606B; --err:#FF8FA3; --okc:#D8D26A; --focus:#FCE9AB;
  }
}
:root[data-theme="dark"]{
  --bg:#220A0E; --panel:#31111A; --band:#3A1620; --field:#2A0D13;
  --ink:#FDEFC8; --muted:#D9BBA4; --heading:#F0708F;
  --line:#5B2A33; --edge:#9A606B; --err:#FF8FA3; --okc:#D8D26A; --focus:#FCE9AB;
}

*,*::before,*::after{box-sizing:inherit}
html{height:100%;scroll-padding-top:calc(env(safe-area-inset-top,0px) + 72px);scroll-behavior:smooth}
body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--sans);font-size:17px;line-height:1.55;-webkit-font-smoothing:antialiased}
h1,h2,h3{font-family:var(--display);line-height:1.08;margin:0;letter-spacing:-.005em}
p{margin:0 0 .8em}
a{color:inherit}
:focus-visible{outline:3px solid var(--focus);outline-offset:3px;border-radius:6px}
.bar :focus-visible,.hero :focus-visible{outline-color:var(--vanilla)}
button,input,select,textarea{font:inherit;color:inherit}
::placeholder{color:var(--muted);opacity:.8}
.wrap{max-width:1040px;margin:0 auto;padding:0 20px}

/* ---------- top bar ---------- */
.bar{position:sticky;top:env(safe-area-inset-top,0px);z-index:50;background:var(--tomato);color:var(--vanilla);box-shadow:0 1px 0 rgba(58,16,20,.25)}
.bar .wrap{display:flex;align-items:center;justify-content:space-between;gap:12px;height:62px}
.brand{font-family:var(--display);font-weight:700;font-size:1.45rem;display:flex;align-items:center;gap:10px;text-decoration:none;color:var(--vanilla)}
.brand i{width:14px;height:14px;border-radius:50%;background:var(--orange);box-shadow:0 0 0 5px rgba(252,233,171,.3)}
.nav{display:none;gap:24px;font-size:.95rem}
.nav a{color:var(--vanilla);text-decoration:none}
.nav a:hover{text-decoration:underline;text-underline-offset:5px}
@media(min-width:800px){.nav{display:flex}}
.exit{background:var(--vanilla);color:var(--tomato);border:0;border-radius:999px;padding:10px 18px;font-weight:700;cursor:pointer;display:flex;align-items:center;gap:8px;min-height:44px}
.exit small{font-weight:500;display:none}
@media(min-width:520px){.exit small{display:inline}}
.exit:hover{background:#fff}

/* ---------- hero ---------- */
.hero{background:radial-gradient(120% 95% at 88% 0%,#D8394B 0%,var(--tomato) 52%,#B0252E 100%);color:var(--vanilla);padding:48px 0 60px;position:relative;overflow:hidden}
.hero .wrap{position:relative;z-index:1}
.petals{position:absolute;right:-90px;top:-80px;width:460px;height:460px;pointer-events:none;transform:rotate(-12deg)}
@media(max-width:700px){.petals{width:310px;height:310px;right:-120px;top:-70px;opacity:.38}}
.hero h1{font-size:clamp(2.5rem,7vw,4.7rem);font-weight:700;max-width:14ch;line-height:1.02}
.hero p.lead{font-size:1.15rem;color:rgba(252,233,171,.95);max-width:52ch;margin:18px 0 0}
.hero .danger{margin-top:22px;display:inline-flex;gap:10px;align-items:center;background:rgba(252,233,171,.14);border:1px solid rgba(252,233,171,.4);padding:10px 14px;border-radius:12px;font-size:.98rem}
.hero .danger a{color:var(--vanilla);font-weight:700;text-underline-offset:3px}
.actions{display:grid;gap:14px;margin-top:36px;grid-template-columns:1fr}
@media(min-width:760px){.actions{grid-template-columns:repeat(3,1fr)}}
.act{background:var(--orange);color:var(--on-warm);border:0;border-radius:20px;padding:20px;text-align:left;cursor:pointer;display:flex;flex-direction:column;gap:6px;min-height:122px;transition:transform .12s;text-decoration:none;box-shadow:0 6px 0 rgba(58,16,20,.22)}
.act:active{transform:translateY(3px);box-shadow:0 3px 0 rgba(58,16,20,.22)}
.act b{font-family:var(--display);font-size:1.45rem;font-weight:700;line-height:1.1}
.act span{font-size:.95rem}
.act.alt{background:var(--vanilla)}
.act.alt2{background:var(--olive)}

/* ---------- sections ---------- */
section.s{padding:68px 0}
section.s.tint{background:var(--band)}
.s h2{font-size:clamp(1.9rem,4.4vw,2.8rem);font-weight:700;margin-bottom:12px;color:var(--heading)}
.s .sub{color:var(--muted);max-width:58ch;margin-bottom:28px}
.two{display:grid;gap:28px;grid-template-columns:1fr}
@media(min-width:860px){.two{grid-template-columns:1fr 1fr;align-items:start}}

label{display:block;font-weight:600;font-size:.95rem;margin:14px 0 6px}
input[type=text],input[type=tel],select,textarea{width:100%;padding:12px 14px;border-radius:12px;border:1.5px solid var(--edge);background:var(--field);color:var(--ink);min-height:46px}
textarea{min-height:92px;resize:vertical}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;border:0;border-radius:12px;padding:13px 22px;font-weight:700;cursor:pointer;text-decoration:none;min-height:46px;background:var(--tomato);color:var(--vanilla)}
.btn:hover{filter:brightness(1.1)}
.btn.lamp{background:var(--orange);color:var(--on-warm)}
.btn.ghost{background:transparent;color:var(--ink);border:1.5px solid var(--edge)}
.btn.green{background:var(--olive-deep);color:var(--vanilla)}
.btn[disabled]{opacity:.55;cursor:not-allowed}
.row{display:flex;flex-wrap:wrap;gap:10px;margin-top:18px}
.note{font-size:.9rem;color:var(--muted)}
.preview{margin-top:18px;padding:16px;border-radius:14px;background:var(--field);border:1.5px dashed var(--edge);font-size:.95rem;word-break:break-word}
.status{margin-top:12px;font-weight:600;min-height:1.4em}
.status.err{color:var(--err)}
.status.ok{color:var(--okc)}

/* ---------- triage ---------- */
.chips{display:flex;flex-wrap:wrap;gap:10px;margin:0 0 18px}
.chip{border:1.5px solid var(--edge);background:var(--panel);color:var(--ink);border-radius:999px;padding:11px 18px;font-weight:600;cursor:pointer;min-height:44px}
.chip:hover{border-color:var(--tomato)}
.chip[aria-pressed=true]{background:var(--tomato);color:var(--vanilla);border-color:var(--tomato)}
.triage-find{display:flex;gap:10px;flex-wrap:wrap}
.triage-find input{flex:1 1 240px}
.result{margin-top:28px;border-left:7px solid var(--blush);padding:6px 0 6px 22px}
.result h3{font-size:1.8rem;margin-bottom:12px;color:var(--heading)}
.result ol{padding-left:1.2em;margin:0 0 18px}
.result li{margin-bottom:8px}
.result li::marker{color:var(--blush);font-weight:700}
.help{display:flex;flex-wrap:wrap;gap:10px;margin-top:6px}
.help a{background:var(--tomato);color:var(--vanilla);text-decoration:none;border-radius:14px;padding:10px 16px;font-weight:700;display:flex;flex-direction:column;line-height:1.2;min-width:150px}
.help a:first-child{background:var(--orange);color:var(--on-warm)}
.help a span{font-weight:500;font-size:.82rem}
.help a b{font-size:1.2rem}

/* ---------- plan ---------- */
fieldset{border:0;padding:0;margin:0}
.checks{display:grid;grid-template-columns:1fr;gap:8px;margin-top:6px}
@media(min-width:520px){.checks{grid-template-columns:1fr 1fr}}
.checks label{display:flex;align-items:center;gap:10px;font-weight:500;margin:0;padding:10px 12px;border:1.5px solid var(--edge);border-radius:12px;cursor:pointer;background:var(--field)}
.checks input{width:20px;height:20px;accent-color:var(--olive-deep)}
.pair{display:grid;gap:10px;grid-template-columns:1fr}
@media(min-width:520px){.pair{grid-template-columns:1fr 1fr}}
.qn{font-family:var(--display);font-weight:700;color:var(--heading);font-size:1.1em}

/* ---------- privacy + roadmap ---------- */
.cols{display:grid;gap:40px;grid-template-columns:1fr}
@media(min-width:860px){.cols{grid-template-columns:1fr 1fr}}
.road{list-style:none;padding:0;margin:0;display:grid;gap:12px}
.road li{padding:16px 18px;border-radius:14px;background:var(--panel);border:1px solid var(--line);border-left:7px solid var(--olive)}
.road b{display:block;font-family:var(--display);font-size:1.25rem;color:var(--heading)}
.priv{list-style:none;padding:0;margin:0}
.priv li{margin-bottom:12px;padding-left:26px;position:relative}
.priv li::before{content:"";position:absolute;left:0;top:.55em;width:12px;height:12px;border-radius:50% 0 50% 0;background:var(--olive)}
footer{background:var(--olive);color:var(--on-warm);padding:30px 0 42px;font-size:.95rem;font-weight:500}

/* ---------- overlays ---------- */
.overlay{position:fixed;inset:0;z-index:1000;display:none}
.overlay.on{display:block}

/* fake call: deliberately looks like a normal phone, not like this site */
#call{background:linear-gradient(180deg,#2a2d33,#0f1114);color:#fff;text-align:center;padding:calc(env(safe-area-inset-top,0px) + 12vh) 24px calc(env(safe-area-inset-bottom,0px) + 40px);display:none;flex-direction:column;align-items:center;justify-content:space-between}
#call.on{display:flex}
#call .avatar{width:112px;height:112px;border-radius:50%;background:#5b6270;display:grid;place-items:center;font-size:3rem;font-weight:700;margin:0 auto 22px;font-family:var(--sans)}
#call .who{font-size:2rem;font-weight:600;font-family:var(--sans)}
#call .state{color:#b9bec9;margin-top:6px;font-size:1.1rem}
#call .btns{display:flex;gap:70px;justify-content:center}
.round{width:76px;height:76px;border-radius:50%;border:0;color:#fff;font-size:1.8rem;cursor:pointer;display:grid;place-items:center}
.round.red{background:#e0393e}.round.grn{background:#2fb457}
.cap{font-size:.85rem;color:#b9bec9;margin-top:8px}

/* quick exit: deliberately looks like an ordinary weather app */
#weather{background:linear-gradient(180deg,#4a90d9,#7fb6ea 55%,#bcd9f2);color:#fff;overflow:auto;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}
#weather .w{max-width:480px;margin:0 auto;padding:calc(env(safe-area-inset-top,0px) + 54px) 22px 60px;text-align:center}
#weather .city{font-size:1.8rem;font-weight:400}
#weather .temp{font-size:6rem;font-weight:200;line-height:1;margin:8px 0;user-select:none;-webkit-user-select:none}
#weather .cond{font-size:1.2rem;opacity:.95}
#weather .panel{background:rgba(255,255,255,.2);border-radius:18px;padding:14px 16px;margin-top:26px;text-align:left;backdrop-filter:blur(6px)}
#weather .panel h4{margin:0 0 10px;font-weight:500;font-size:.85rem;opacity:.85}
#weather .hours{display:flex;gap:18px;overflow-x:auto;text-align:center}
#weather .hours div{min-width:50px}
#weather .day{display:flex;justify-content:space-between;padding:9px 0;border-top:1px solid rgba(255,255,255,.25)}
#weather .day:first-of-type{border-top:0}
#weather .back{margin-top:34px;font-size:.7rem;opacity:.25}

@media (prefers-reduced-motion:reduce){html{scroll-behavior:auto}.act{transition:none}}
</style>
</head>
<body>

<header class="bar">
  <div class="wrap">
    <a class="brand" href="#top"><i aria-hidden="true"></i>Safe Steps</a>
    <nav class="nav" aria-label="Sections">
      <a href="#triage">What's happening?</a>
      <a href="#plan">Safety plan</a>
      <a href="#privacy">Privacy</a>
      <a href="#roadmap">Roadmap</a>
    </nav>
    <button class="exit" id="quickExit" aria-label="Quick exit. Switches to a weather page right now. Shortcut: press Escape.">Quick exit <small>(Esc)</small></button>
  </div>
</header>

<main id="top">
<section class="hero">
  <svg class="petals" viewBox="0 0 400 400" aria-hidden="true" focusable="false">
    <g transform="translate(200 200)">
      <g fill="#DF4C74">
        <path d="M0 0C-72-40-80-152 0-192C80-152 72-40 0 0Z"/>
        <path transform="rotate(120)" d="M0 0C-72-40-80-152 0-192C80-152 72-40 0 0Z"/>
        <path transform="rotate(240)" d="M0 0C-72-40-80-152 0-192C80-152 72-40 0 0Z"/>
      </g>
      <g fill="#FCE9AB" opacity=".3">
        <path d="M0-22C-30-52-34-112 0-142C34-112 30-52 0-22Z"/>
        <path transform="rotate(120)" d="M0-22C-30-52-34-112 0-142C34-112 30-52 0-22Z"/>
        <path transform="rotate(240)" d="M0-22C-30-52-34-112 0-142C34-112 30-52 0-22Z"/>
      </g>
      <g fill="#FC8A2D">
        <path transform="rotate(60)" d="M0 0C-50-30-54-116 0-156C54-116 50-30 0 0Z"/>
        <path transform="rotate(180)" d="M0 0C-50-30-54-116 0-156C54-116 50-30 0 0Z"/>
        <path transform="rotate(300)" d="M0 0C-50-30-54-116 0-156C54-116 50-30 0 0Z"/>
      </g>
      <g stroke="#C42B34" stroke-width="3.5" stroke-linecap="round" fill="none">
        <g transform="rotate(60)"><path d="M-10-40L-14-96M0-36L0-104M10-40L14-96"/></g>
        <g transform="rotate(180)"><path d="M-10-40L-14-96M0-36L0-104M10-40L14-96"/></g>
        <g transform="rotate(300)"><path d="M-10-40L-14-96M0-36L0-104M10-40L14-96"/></g>
      </g>
      <g stroke="#9E9820" stroke-width="3" stroke-linecap="round" fill="#9E9820">
        <g transform="rotate(15)"><path d="M0 0V-84"/><circle cy="-86" r="6"/></g>
        <g transform="rotate(75)"><path d="M0 0V-70"/><circle cy="-72" r="6"/></g>
        <g transform="rotate(135)"><path d="M0 0V-90"/><circle cy="-92" r="6"/></g>
        <g transform="rotate(200)"><path d="M0 0V-76"/><circle cy="-78" r="6"/></g>
        <g transform="rotate(255)"><path d="M0 0V-88"/><circle cy="-90" r="6"/></g>
        <g transform="rotate(320)"><path d="M0 0V-72"/><circle cy="-74" r="6"/></g>
      </g>
      <circle r="15" fill="#FCE9AB"/>
    </g>
  </svg>
  <div class="wrap">
    <h1>If you feel unsafe, start here.</h1>
    <p class="lead">Three things you can do in the next ten seconds. Nothing you do here is saved.</p>
    <div class="danger">In immediate danger? Call <a href="tel:112">112</a> now.</div>
    <div class="actions">
      <button class="act" id="openCall"><b>Fake call</b><span>A realistic incoming call, so you have a reason to leave.</span></button>
      <a class="act alt" href="#alert"><b>Send my location</b><span>A ready-to-send message to someone you trust.</span></a>
      <a class="act alt2" href="#triage"><b>What's happening?</b><span>Tell us the situation, get the next steps and the right helpline.</span></a>
    </div>
  </div>
</section>

<section class="s" id="call-setup">
  <div class="wrap two">
    <div>
      <h2>Fake call</h2>
      <p class="sub">Need to step away from someone or somewhere? Start a call that looks and sounds real. Answer it, talk, and walk out.</p>
    </div>
    <div>
      <label for="callerName">Who is calling?</label>
      <input type="text" id="callerName" value="Mom" maxlength="30" autocomplete="off">
      <label for="callDelay">Ring after</label>
      <select id="callDelay">
        <option value="0">Right now</option>
        <option value="5">5 seconds</option>
        <option value="15">15 seconds</option>
        <option value="30">30 seconds</option>
      </select>
      <div class="row"><button class="btn lamp" id="startCall">Start fake call</button></div>
      <p class="note" style="margin-top:12px">Sound plays after you tap, so turn your volume up. Phone vibrates where supported.</p>
    </div>
  </div>
</section>

<section class="s tint" id="alert">
  <div class="wrap two">
    <div>
      <h2>Alert a trusted contact</h2>
      <p class="sub">One tap builds a message with a map link to where you are right now. You choose how to send it: WhatsApp or SMS.</p>
    </div>
    <div>
      <label for="contactName">Contact's name (optional)</label>
      <input type="text" id="contactName" maxlength="40" autocomplete="off" placeholder="Priya">
      <label for="contactPhone">Contact's phone, with country code (optional)</label>
      <input type="tel" id="contactPhone" inputmode="tel" placeholder="919876543210" autocomplete="off">
      <p class="note">Leave it blank and WhatsApp will let you pick the contact.</p>
      <div class="row"><button class="btn lamp" id="getLoc">Get my location and prepare alert</button></div>
      <div class="status" id="locStatus" role="status" aria-live="polite"></div>
      <div id="locOut" hidden>
        <div class="preview" id="locPreview"></div>
        <div class="row">
          <a class="btn green" id="sendWa" target="_blank" rel="noopener">Send on WhatsApp</a>
          <a class="btn" id="sendSms">Send by SMS</a>
        </div>
      </div>
    </div>
  </div>
</section>

<section class="s" id="triage">
  <div class="wrap">
    <h2>What's happening?</h2>
    <p class="sub">Pick what fits best, or describe it in a few words. You'll get steps for right now and the helpline to call.</p>
    <div class="chips" role="group" aria-label="Situation">
      <button class="chip" data-t="followed" aria-pressed="false">I'm being followed</button>
      <button class="chip" data-t="online" aria-pressed="false">I'm being harassed online</button>
      <button class="chip" data-t="domestic" aria-pressed="false">Violence at home</button>
    </div>
    <div class="triage-find">
      <input type="text" id="triageText" placeholder="Or type it here, e.g. someone keeps messaging me" aria-label="Describe your situation">
      <button class="btn" id="triageGo">Show me what to do</button>
    </div>
    <div id="triageOut" aria-live="polite"></div>
  </div>
</section>

<section class="s tint" id="plan">
  <div class="wrap">
    <h2>Your personal safety plan</h2>
    <p class="sub">Answer seven short questions and download a one-page PDF you can keep somewhere private. Your answers are used only to build the PDF, then discarded. Nothing is saved.</p>
    <form method="post" action="/plan" autocomplete="off">
    <div class="two">
      <div>
        <label for="q1"><span class="qn">1.</span> Your first name (optional)</label>
        <input type="text" id="q1" name="q1" maxlength="40" autocomplete="off">

        <label for="q2"><span class="qn">2.</span> What worries you most?</label>
        <select id="q2" name="q2">
          <option value="followed">Being followed or feeling unsafe outside</option>
          <option value="online">Harassment or threats online</option>
          <option value="domestic">Violence or abuse at home</option>
          <option value="other">Something else</option>
        </select>

        <label><span class="qn">3.</span> Two people you trust</label>
        <div class="pair">
          <input type="text" id="q3a" name="q3a" placeholder="Name 1" aria-label="Trusted person 1 name" maxlength="40">
          <input type="tel" id="q3ap" name="q3ap" placeholder="Phone 1" aria-label="Trusted person 1 phone" maxlength="20">
          <input type="text" id="q3b" name="q3b" placeholder="Name 2" aria-label="Trusted person 2 name" maxlength="40">
          <input type="tel" id="q3bp" name="q3bp" placeholder="Phone 2" aria-label="Trusted person 2 phone" maxlength="20">
        </div>

        <label for="q4"><span class="qn">4.</span> A safe place you can go quickly</label>
        <input type="text" id="q4" name="q4" placeholder="A friend's home, a 24-hour shop, a police station" maxlength="120">
      </div>
      <div>
        <label for="q5"><span class="qn">5.</span> A code word to tell your contacts you need help</label>
        <input type="text" id="q5" name="q5" placeholder="e.g. 'pineapple'" maxlength="40" autocomplete="off">

        <fieldset>
          <label><span class="qn">6.</span> Keep these ready</label>
          <div class="checks" id="q6">
            <label><input type="checkbox" name="item" value="ID documents"> ID documents</label>
            <label><input type="checkbox" name="item" value="Phone and charger"> Phone and charger</label>
            <label><input type="checkbox" name="item" value="Some cash"> Some cash</label>
            <label><input type="checkbox" name="item" value="Medicines"> Medicines</label>
            <label><input type="checkbox" name="item" value="Spare keys"> Spare keys</label>
            <label><input type="checkbox" name="item" value="Copies of messages or photos as evidence"> Copies of evidence</label>
          </div>
        </fieldset>

        <label for="q7"><span class="qn">7.</span> How you will leave if you need to</label>
        <textarea id="q7" name="q7" placeholder="Which exit, which route, who will meet you"></textarea>

        <div class="row"><button type="submit" class="btn lamp" id="makePdf">Download my safety plan (PDF)</button></div>
        
      </div>
    </div>
    </form>
  </div>
</section>

<section class="s" id="privacy">
  <div class="wrap cols">
    <div>
      <h2>Your privacy</h2>
      <ul class="priv">
        <li>We store nothing by default. No accounts, no sign-up.</li>
        <li>Your answers are used only to build your PDF or your triage result, then discarded. This server stores nothing and logs nothing.</li>
        <li>Your location is read only when you tap the button, and goes only into the message you choose to send.</li>
        <li>Quick exit hides this page at once. Press Escape any time.</li>
      </ul>
    </div>
    <div id="roadmap">
      <h2>Coming next</h2>
      <ul class="road">
        <li><b>Community forum</b>A moderated space to share experiences and advice.</li>
        <li><b>Evidence vault</b>A private place to keep screenshots, photos and notes.</li>
        <li><b>Counselor chat</b>Talk to a trained counselor when you are ready.</li>
      </ul>
    </div>
  </div>
</section>
</main>

<footer>
  <div class="wrap">Safe Steps is a guide, not a replacement for emergency services. In danger, call 112.</div>
</footer>

<!-- Fake incoming call -->
<div class="overlay" id="call" role="dialog" aria-modal="true" aria-label="Incoming call">
  <div>
    <div class="avatar" id="callAvatar" aria-hidden="true">M</div>
    <div class="who" id="callWho">Mom</div>
    <div class="state" id="callState">mobile</div>
  </div>
  <div class="btns">
    <div><button class="round red" id="declineCall" aria-label="Decline">✕</button><div class="cap" id="capL">Decline</div></div>
    <div><button class="round grn" id="acceptCall" aria-label="Answer">✆</button><div class="cap" id="capR">Accept</div></div>
  </div>
</div>

<!-- Quick exit disguise -->
<div class="overlay" id="weather" aria-hidden="true">
  <div class="w">
    <div class="city">Weather</div>
    <div class="temp" id="wTemp">31°</div>
    <div class="cond">Partly cloudy</div>
    <div class="cond" style="opacity:.8">H:33°  L:26°</div>
    <div class="panel"><h4>HOURLY FORECAST</h4>
      <div class="hours">
        <div>Now<br>☁️<br>31°</div><div>2 PM<br>⛅<br>32°</div><div>3 PM<br>⛅<br>33°</div><div>4 PM<br>☁️<br>32°</div><div>5 PM<br>🌤️<br>31°</div><div>6 PM<br>🌤️<br>30°</div><div>7 PM<br>☁️<br>28°</div>
      </div>
    </div>
    <div class="panel"><h4>5-DAY FORECAST</h4>
      <div class="day"><span>Today</span><span>⛅ 26° – 33°</span></div>
      <div class="day"><span>Sat</span><span>🌦️ 26° – 31°</span></div>
      <div class="day"><span>Sun</span><span>🌧️ 25° – 29°</span></div>
      <div class="day"><span>Mon</span><span>⛅ 26° – 31°</span></div>
      <div class="day"><span>Tue</span><span>☀️ 26° – 32°</span></div>
    </div>
    <div class="back" id="wBack">Triple-tap the temperature to return</div>
  </div>
</div>

<script>
(function(){
"use strict";
var $ = function(id){return document.getElementById(id)};
var origTitle = document.title;

/* =============== 1. QUICK EXIT =============== */
var weather = $("weather");
function quickExit(){
  weather.classList.add("on");
  document.title = "Weather";
  stopCall(true);
  window.scrollTo(0,0);
  weather.scrollTop = 0;
}
function returnFromExit(){
  weather.classList.remove("on");
  document.title = origTitle;
}
$("quickExit").addEventListener("click", quickExit);
document.addEventListener("keydown", function(e){
  if(e.key === "Escape" && !weather.classList.contains("on")) quickExit();
});
var taps = 0, tapTimer = null;
$("wTemp").addEventListener("click", function(){
  taps++; clearTimeout(tapTimer);
  tapTimer = setTimeout(function(){taps=0}, 600);
  if(taps >= 3){taps = 0; returnFromExit();}
});

/* =============== 2. FAKE CALL =============== */
var callEl = $("call"), ringCtx = null, ringTimer = null, vibTimer = null, delayTimer = null, callClock = null;
function startRing(){
  try{
    var AC = window.AudioContext || window.webkitAudioContext;
    if(!AC) return;
    ringCtx = ringCtx || new AC();
    if(ringCtx.state === "suspended") ringCtx.resume();
    var burst = function(){
      if(!ringCtx) return;
      var t = ringCtx.currentTime;
      [0, .45, 1.2, 1.65].forEach(function(off){
        var o = ringCtx.createOscillator(), g = ringCtx.createGain();
        o.type = "sine"; o.frequency.value = (off % 1.2 === 0) ? 880 : 660;
        g.gain.setValueAtTime(0, t+off);
        g.gain.linearRampToValueAtTime(.35, t+off+.03);
        g.gain.linearRampToValueAtTime(0, t+off+.38);
        o.connect(g); g.connect(ringCtx.destination);
        o.start(t+off); o.stop(t+off+.4);
      });
    };
    burst(); ringTimer = setInterval(burst, 3200);
  }catch(e){}
  try{
    if(navigator.vibrate){
      navigator.vibrate([600,300,600]);
      vibTimer = setInterval(function(){navigator.vibrate([600,300,600])}, 3200);
    }
  }catch(e){}
}
function stopRing(){
  clearInterval(ringTimer); ringTimer = null;
  clearInterval(vibTimer); vibTimer = null;
  try{ navigator.vibrate && navigator.vibrate(0) }catch(e){}
}
function showCall(){
  var name = ($("callerName").value || "Mom").trim() || "Mom";
  $("callWho").textContent = name;
  $("callAvatar").textContent = name.charAt(0).toUpperCase();
  $("callState").textContent = "mobile";
  $("capL").textContent = "Decline"; $("capR").textContent = "Accept";
  $("acceptCall").style.display = "";
  callEl.classList.add("on");
  startRing();
}
function stopCall(silent){
  clearTimeout(delayTimer); clearInterval(callClock);
  stopRing();
  callEl.classList.remove("on");
}
$("startCall").addEventListener("click", function(){
  // Prime audio inside the user gesture so the delayed ring can play.
  try{
    var AC = window.AudioContext || window.webkitAudioContext;
    if(AC){ ringCtx = ringCtx || new AC(); ringCtx.resume(); }
  }catch(e){}
  var d = parseInt($("callDelay").value, 10) || 0;
  clearTimeout(delayTimer);
  if(d === 0){ showCall(); }
  else {
    $("startCall").textContent = "Call arrives in " + d + "s…";
    $("startCall").disabled = true;
    delayTimer = setTimeout(function(){
      $("startCall").textContent = "Start fake call";
      $("startCall").disabled = false;
      showCall();
    }, d*1000);
  }
});
$("openCall").addEventListener("click", function(){
  $("callDelay").value = "0"; showCall();
});
$("declineCall").addEventListener("click", function(){ stopCall(); });
$("acceptCall").addEventListener("click", function(){
  stopRing();
  var s = 0;
  $("capL").textContent = "End"; $("capR").textContent = "";
  $("acceptCall").style.display = "none";
  var fmt = function(n){return (n<10?"0":"")+n};
  $("callState").textContent = "00:00";
  clearInterval(callClock);
  callClock = setInterval(function(){
    s++; $("callState").textContent = fmt(Math.floor(s/60))+":"+fmt(s%60);
  }, 1000);
});

/* =============== 3. LOCATION ALERT =============== */
function buildAlert(lat, lng, acc){
  var link = "https://www.google.com/maps?q=" + lat.toFixed(6) + "," + lng.toFixed(6);
  var who = $("contactName").value.trim();
  var msg = (who ? "Hi " + who + ", " : "") +
    "I don't feel safe right now. This is my live location: " + link +
    " Please call me or check on me. If I don't answer, please get help.";
  var phone = $("contactPhone").value.replace(/[^0-9]/g, "");
  $("locPreview").textContent = msg;
  $("sendWa").href = "https://wa.me/" + phone + "?text=" + encodeURIComponent(msg);
  $("sendSms").href = "sms:" + (phone ? "+" + phone : "") + "?&body=" + encodeURIComponent(msg);
  $("locOut").hidden = false;
  var s = $("locStatus"); s.className = "status ok";
  s.textContent = "Location found (accurate to about " + Math.round(acc) + " m). Choose how to send.";
}
$("getLoc").addEventListener("click", function(){
  var s = $("locStatus"); s.className = "status"; s.textContent = "Finding your location…";
  $("locOut").hidden = true;
  if(!navigator.geolocation){
    s.className = "status err";
    s.textContent = "This browser can't share location. Tell your contact where you are by voice or text.";
    return;
  }
  navigator.geolocation.getCurrentPosition(function(p){
    buildAlert(p.coords.latitude, p.coords.longitude, p.coords.accuracy || 0);
  }, function(err){
    s.className = "status err";
    s.textContent = err && err.code === 1
      ? "Location is blocked. Allow location for this page in your browser settings, then tap again."
      : "Couldn't get your location. Move to an open area and tap again, or text your contact where you are.";
  }, {enableHighAccuracy:true, timeout:12000, maximumAge:0});
});

/* =============== 4. TRIAGE (logic lives in the C++ server: POST /triage) =============== */
var chosen = null;
function setChip(key){
  document.querySelectorAll(".chip").forEach(function(c){
    c.setAttribute("aria-pressed", c.dataset.t === key ? "true" : "false");
  });
}
function ask(body){
  fetch("/triage", {method:"POST", headers:{"Content-Type":"application/x-www-form-urlencoded"}, body:body})
    .then(function(r){ return r.text(); })
    .then(function(h){
      var out = $("triageOut"); out.innerHTML = h;
      var r = out.firstElementChild;
      setChip(r && r.dataset.key !== "unsure" ? r.dataset.key : null);
      out.scrollIntoView({behavior:"smooth", block:"nearest"});
    })
    .catch(function(){ $("triageOut").textContent = "Couldn't reach the server. If you are in danger, call 112."; });
}
document.querySelectorAll(".chip").forEach(function(c){
  c.addEventListener("click", function(){
    chosen = c.dataset.t; ask("choice=" + encodeURIComponent(chosen));
  });
});
function goTriage(){
  var txt = $("triageText").value.trim();
  if(txt){ chosen = null; ask("text=" + encodeURIComponent(txt)); }
  else if(chosen){ ask("choice=" + encodeURIComponent(chosen)); }
  else { ask("text="); }
}
$("triageGo").addEventListener("click", goTriage);
$("triageText").addEventListener("keydown", function(e){ if(e.key === "Enter") goTriage(); });
$("triageOut").addEventListener("click", function(e){
  var b = e.target.closest("[data-plan]"); if(!b) return;
  $("q2").value = b.dataset.plan === "unsure" ? "other" : b.dataset.plan;
  $("plan").scrollIntoView({behavior:"smooth"});
});

})();
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
//  Helplines (India). Edit here to localise.
// ---------------------------------------------------------------------------
struct Line { const char* name; const char* num; const char* note; };

const std::map<std::string, Line> LINES = {
    {"emergency", {"Emergency", "112", "Police, fire, ambulance"}},
    {"women",     {"Women Helpline", "181", "Support and referrals"}},
    {"distress",  {"Women in distress (Police)", "1091", "Police women helpline"}},
    {"cyber",     {"Cyber Crime Helpline", "1930", "Online harassment, blackmail"}},
};

// ---------------------------------------------------------------------------
//  Triage: situations, steps and keyword matching
// ---------------------------------------------------------------------------
struct Triage {
    std::string key;
    std::string title;
    std::vector<std::string> steps;
    std::vector<std::string> lines;
};

const std::vector<Triage>& triageTable() {
    static const std::vector<Triage> t = {
        {"followed", "You're being followed",
         {"Don't go home or to a quiet place. Head to a busy, well-lit spot: a shop, petrol pump, station or hospital.",
          "Cross the road or change direction once to confirm. If they follow, treat it as real.",
          "Call 112 or 1091 and say where you are and what the person looks like.",
          "Use Fake call on this page to create a reason to speak out loud, and send your location to a trusted contact.",
          "Keep your phone in hand and keys between your fingers. Don't use headphones."},
         {"emergency", "distress", "women"}},
        {"online", "You're being harassed online",
         {"Don't reply and don't delete. Take screenshots showing the profile, messages, dates and links.",
          "Block and report the account on the platform.",
          "Report at cybercrime.gov.in or call 1930. Keep your screenshots to attach.",
          "Tell someone you trust. You don't have to deal with this alone.",
          "Tighten privacy: make accounts private, change passwords, and turn on two-step login."},
         {"cyber", "women", "emergency"}},
        {"domestic", "There is violence at home",
         {"If you are in danger right now, call 112. Your safety comes first.",
          "Get to a room with an exit and a phone, away from the kitchen and anything that could be used as a weapon.",
          "Call the Women Helpline 181 for support, shelter and legal help.",
          "Tell a trusted neighbour or relative your code word, so they know when to call for help.",
          "When you can, build your personal safety plan below and keep documents and some cash ready."},
         {"emergency", "women", "distress"}},
        {"unsure", "Not sure? Start with these",
         {"If anyone is in danger right now, call 112.",
          "Move to a safe, busy place and tell someone you trust where you are.",
          "Call the Women Helpline 181 to talk it through and find the right support.",
          "Use the buttons above: Fake call, Send my location."},
         {"emergency", "women", "cyber"}},
    };
    return t;
}

const Triage& findTriage(const std::string& key) {
    for (const auto& t : triageTable())
        if (t.key == key) return t;
    return triageTable().back();  // "unsure"
}

const std::vector<std::pair<std::string, std::vector<std::string>>>& keywordTable() {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> k = {
        {"followed", {"follow", "stalk", "behind me", "night", "street", "car ", "bike", "walking", "chasing", "unsafe"}},
        {"online",   {"online", "message", "msg", "dm", "instagram", "facebook", "whatsapp", "photo", "morph",
                      "blackmail", "fake profile", "cyber", "troll", "leak", "video", "email", "number"}},
        {"domestic", {"home", "husband", "partner", "hit", "beat", "abuse", "violence", "domestic", "family",
                      "in-law", "inlaw", "dowry", "slap", "boyfriend", "marital", "house"}},
    };
    return k;
}

std::string matchTriage(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    text = " " + text + " ";
    std::string best = "unsure";
    int bestScore = 0;
    for (const auto& entry : keywordTable()) {
        int score = 0;
        for (const auto& w : entry.second)
            if (text.find(w) != std::string::npos) ++score;
        if (score > bestScore) { bestScore = score; best = entry.first; }
    }
    return best;
}

// ---------------------------------------------------------------------------
//  Small helpers: HTML escaping, form decoding
// ---------------------------------------------------------------------------
std::string htmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o += c;
        }
    }
    return o;
}

std::string urlDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            o += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            o += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            o += s[i];
        }
    }
    return o;
}

using Form = std::vector<std::pair<std::string, std::string>>;

Form parseForm(const std::string& body) {
    Form f;
    size_t pos = 0;
    while (pos <= body.size()) {
        size_t amp = body.find('&', pos);
        if (amp == std::string::npos) amp = body.size();
        std::string pair = body.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) f.emplace_back(urlDecode(pair.substr(0, eq)), urlDecode(pair.substr(eq + 1)));
        pos = amp + 1;
    }
    return f;
}

std::string formGet(const Form& f, const std::string& key, size_t maxLen = 200) {
    for (const auto& kv : f)
        if (kv.first == key) {
            std::string v = kv.second.substr(0, maxLen);
            // trim
            size_t a = v.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            size_t b = v.find_last_not_of(" \t\r\n");
            return v.substr(a, b - a + 1);
        }
    return "";
}

// ---------------------------------------------------------------------------
//  Triage response (HTML fragment). No user input is echoed back.
// ---------------------------------------------------------------------------
std::string renderTriage(const Triage& t) {
    std::string h = "<div class=\"result\" data-key=\"" + t.key + "\"><h3>" + htmlEscape(t.title) + "</h3><ol>";
    for (const auto& s : t.steps) h += "<li>" + htmlEscape(s) + "</li>";
    h += "</ol><div class=\"help\">";
    for (const auto& k : t.lines) {
        const Line& L = LINES.at(k);
        h += std::string("<a href=\"tel:") + L.num + "\"><span>" + htmlEscape(L.name) + "</span><b>Call " + L.num +
             "</b></a>";
    }
    h += "</div><div class=\"row\"><button type=\"button\" class=\"btn ghost\" data-plan=\"" + t.key +
         "\">Use this in my safety plan</button></div></div>";
    return h;
}

// ---------------------------------------------------------------------------
//  Minimal PDF writer (A4, Helvetica). Enough for a clean one-page plan.
// ---------------------------------------------------------------------------
class Pdf {
public:
    Pdf() { newPage(); }

    void header(const std::string& title, const std::string& subtitle) {
        raw("0.769 0.169 0.204 rg 0 746 595 96 re f\n");   // Tomato Jam band
        raw("0.620 0.596 0.125 rg 0 746 595 5 re f\n");    // Olive stripe
        raw("0.988 0.541 0.176 rg 48 788 12 12 re f\n");   // Princeton Orange mark
        text(true, 22, 70, 47, title, "0.988 0.914 0.671");     // Vanilla Custard
        text(false, 10, 48, 76, subtitle, "0.988 0.914 0.671");
        y_ = 96;
    }

    void heading(const std::string& t) {
        ensure(40);
        y_ += 14;
        text(true, 12, kMargin, y_, t, "0.769 0.169 0.204");  // Tomato Jam
        y_ += 6;
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << "0.988 0.541 0.176 rg " << kMargin << ' ' << (842 - y_ - 2)
          << " 36 2 re f\n";
        raw(o.str());
        y_ += 16;
    }

    void para(const std::string& t) {
        for (const auto& line : wrap(t, 86)) {
            ensure(16);
            text(false, 11, kMargin, y_, line, "0.227 0.063 0.078");
            y_ += 15;
        }
    }

    void bullet(const std::string& t) {
        bool first = true;
        for (const auto& line : wrap(t, 80)) {
            ensure(16);
            if (first) {
                std::ostringstream o;
                o << std::fixed << std::setprecision(2) << "0.875 0.298 0.455 rg " << kMargin << ' ' << (842 - y_ + 1)
                  << " 5 5 re f\n";
                raw(o.str());
                first = false;
            }
            text(false, 11, kMargin + 16, y_, line, "0.227 0.063 0.078");
            y_ += 15;
        }
    }

    std::string build() const {
        std::string out = "%PDF-1.4\n";
        std::vector<size_t> offsets;
        auto add = [&](const std::string& body) {
            offsets.push_back(out.size());
            out += std::to_string(offsets.size()) + " 0 obj\n" + body + "\nendobj\n";
        };
        std::string kids;
        for (size_t i = 0; i < pages_.size(); ++i) kids += std::to_string(5 + 2 * i) + " 0 R ";
        add("<< /Type /Catalog /Pages 2 0 R >>");
        add("<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(pages_.size()) + " >>");
        add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
        add("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>");
        for (size_t i = 0; i < pages_.size(); ++i) {
            add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
                "/Contents " + std::to_string(6 + 2 * i) + " 0 R >>");
            add("<< /Length " + std::to_string(pages_[i].size()) + " >>\nstream\n" + pages_[i] + "endstream");
        }
        size_t xref = out.size();
        out += "xref\n0 " + std::to_string(offsets.size() + 1) + "\n0000000000 65535 f \n";
        char buf[32];
        for (size_t off : offsets) {
            std::snprintf(buf, sizeof buf, "%010zu 00000 n \n", off);
            out += buf;
        }
        out += "trailer\n<< /Size " + std::to_string(offsets.size() + 1) + " /Root 1 0 R >>\nstartxref\n" +
               std::to_string(xref) + "\n%%EOF\n";
        return out;
    }

private:
    static constexpr double kMargin = 48;
    std::vector<std::string> pages_;
    double y_ = 0;  // distance from top of page

    void newPage() {
        pages_.emplace_back();
        y_ = 56;
    }
    void ensure(double h) {
        if (y_ + h > 800) newPage();
    }
    void raw(const std::string& s) { pages_.back() += s; }

    static std::string pdfEscape(const std::string& s) {
        std::string o;
        for (unsigned char c : s) {
            if (c >= 0x80) {
                if ((c & 0xC0) != 0x80) o += '?';  // one '?' per non-ASCII character
            } else if (c < 0x20) {
                o += ' ';
            } else if (c == '(' || c == ')' || c == '\\') {
                o += '\\';
                o += static_cast<char>(c);
            } else {
                o += static_cast<char>(c);
            }
        }
        return o;
    }

    void text(bool bold, double size, double x, double yTop, const std::string& s, const char* rgb) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << "BT " << rgb << " rg /F" << (bold ? 2 : 1) << ' ' << size
          << " Tf " << x << ' ' << (842 - yTop) << " Td (" << pdfEscape(s) << ") Tj ET\n";
        raw(o.str());
    }

    static std::vector<std::string> wrap(const std::string& s, size_t maxChars) {
        std::vector<std::string> lines;
        std::istringstream words(s);
        std::string w, cur;
        while (words >> w) {
            while (w.size() > maxChars) {  // break very long words
                if (!cur.empty()) { lines.push_back(cur); cur.clear(); }
                lines.push_back(w.substr(0, maxChars));
                w = w.substr(maxChars);
            }
            if (cur.empty()) cur = w;
            else if (cur.size() + 1 + w.size() <= maxChars) cur += " " + w;
            else { lines.push_back(cur); cur = w; }
        }
        if (!cur.empty()) lines.push_back(cur);
        if (lines.empty()) lines.push_back("");
        return lines;
    }
};

struct Concern { const char* label; std::vector<std::string> lines; };

const Concern& concernFor(const std::string& key) {
    static const std::map<std::string, Concern> m = {
        {"followed", {"Being followed or feeling unsafe outside", {"emergency", "distress", "women"}}},
        {"online",   {"Harassment or threats online", {"cyber", "women", "emergency"}}},
        {"domestic", {"Violence or abuse at home", {"emergency", "women", "distress"}}},
        {"other",    {"Another safety concern", {"emergency", "women", "cyber"}}},
    };
    auto it = m.find(key);
    return it != m.end() ? it->second : m.at("other");
}

std::string buildPlanPdf(const Form& f) {
    const std::vector<std::string> allowedItems = {
        "ID documents", "Phone and charger", "Some cash", "Medicines", "Spare keys",
        "Copies of messages or photos as evidence"};

    std::string name = formGet(f, "q1", 40);
    const std::string concernKey = formGet(f, "q2", 20);
    const Concern& c = concernFor(concernKey);

    char date[64];
    std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof date, "%d %B %Y", std::localtime(&now));

    Pdf pdf;
    pdf.header("My Safety Plan", (name.empty() ? "" : "Prepared for " + name + "  |  ") + std::string(date) + "  |  Safe Steps");

    pdf.heading("What I am worried about");
    pdf.para(c.label);

    pdf.heading("People I trust");
    bool any = false;
    for (std::string suffix : {"a", "b"}) {
        std::string n = formGet(f, "q3" + suffix, 40), p = formGet(f, "q3" + suffix + "p", 20);
        if (n.empty() && p.empty()) continue;
        pdf.bullet((n.empty() ? "Contact" : n) + (p.empty() ? "" : "  -  " + p));
        any = true;
    }
    if (!any) pdf.para("Not filled in yet. Choose two people and write them here.");

    pdf.heading("My safe place");
    std::string q4 = formGet(f, "q4", 120);
    pdf.para(q4.empty() ? "Not filled in yet. Pick somewhere you can reach within 10 minutes." : q4);

    pdf.heading("My code word");
    std::string q5 = formGet(f, "q5", 40);
    pdf.para(q5.empty() ? "Not filled in yet. Choose a word that would not look unusual in a message."
                        : "\"" + q5 + "\"  - send this to my trusted people when I need them to call or come.");

    pdf.heading("Things I keep ready");
    bool anyItem = false;
    for (const auto& kv : f)
        if (kv.first == "item" &&
            std::find(allowedItems.begin(), allowedItems.end(), kv.second) != allowedItems.end()) {
            pdf.bullet(kv.second);
            anyItem = true;
        }
    if (!anyItem) pdf.para("Nothing selected yet.");

    pdf.heading("How I will leave");
    std::string q7 = formGet(f, "q7", 600);
    pdf.para(q7.empty() ? "Not filled in yet. Decide the exit, the route and who will meet you." : q7);

    pdf.heading("Who I can call");
    for (const auto& k : c.lines) {
        const Line& L = LINES.at(k);
        pdf.bullet(std::string(L.name) + ": " + L.num + "  (" + L.note + ")");
    }
    pdf.bullet("If I am in immediate danger: call 112.");

    pdf.heading("Reminders");
    pdf.bullet("Keep this plan somewhere private. Do not leave it where someone who may harm you can find it.");
    pdf.bullet("Update it when things change. Share the code word only with the people you trust.");

    return pdf.build();
}

// ---------------------------------------------------------------------------
//  HTTP server
// ---------------------------------------------------------------------------
struct Request {
    std::string method, path, body;
};

constexpr size_t kMaxHeader = 16 * 1024;
constexpr size_t kMaxBody = 64 * 1024;

bool readRequest(int fd, Request& req) {
    std::string buf;
    char tmp[4096];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        buf.append(tmp, static_cast<size_t>(n));
        headerEnd = buf.find("\r\n\r\n");
        if (headerEnd == std::string::npos && buf.size() > kMaxHeader) return false;
    }
    std::string head = buf.substr(0, headerEnd);
    std::istringstream hs(head);
    std::string target, version;
    hs >> req.method >> target >> version;
    if (req.method.empty() || target.empty()) return false;
    req.path = target.substr(0, target.find('?'));

    size_t contentLength = 0;
    std::string line;
    std::getline(hs, line);  // rest of request line
    while (std::getline(hs, line)) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.rfind("content-length:", 0) == 0) contentLength = std::strtoul(line.c_str() + 15, nullptr, 10);
    }
    if (contentLength > kMaxBody) return false;

    req.body = buf.substr(headerEnd + 4);
    while (req.body.size() < contentLength) {
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        req.body.append(tmp, static_cast<size_t>(n));
    }
    req.body.resize(contentLength);
    return true;
}

void respond(int fd, int code, const char* status, const std::string& type, const std::string& body,
             const std::string& extra = "") {
    std::ostringstream h;
    h << "HTTP/1.1 " << code << ' ' << status << "\r\n"
      << "Content-Type: " << type << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n"
      << "X-Content-Type-Options: nosniff\r\n"
      << "Referrer-Policy: no-referrer\r\n"
      << "Permissions-Policy: geolocation=(self)\r\n"
      << "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
         "img-src data:; connect-src 'self'; form-action 'self'; base-uri 'none'; frame-ancestors 'none'\r\n"
      << extra << "\r\n";
    std::string out = h.str() + body;
    size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = send(fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
}

void handle(int fd) {
    timeval tv{10, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    Request req;
    if (!readRequest(fd, req)) {
        respond(fd, 400, "Bad Request", "text/plain; charset=utf-8", "Bad request\n");
        close(fd);
        return;
    }

    if (req.path == "/" || req.path == "/index.html") {
        if (req.method != "GET") respond(fd, 405, "Method Not Allowed", "text/plain", "Use GET\n", "Allow: GET\r\n");
        else respond(fd, 200, "OK", "text/html; charset=utf-8", PAGE);
    } else if (req.path == "/triage") {
        if (req.method != "POST") {
            respond(fd, 405, "Method Not Allowed", "text/plain", "Use POST\n", "Allow: POST\r\n");
        } else {
            Form f = parseForm(req.body);
            std::string choice = formGet(f, "choice", 20);
            std::string key = choice.empty() ? matchTriage(formGet(f, "text", 300)) : findTriage(choice).key;
            respond(fd, 200, "OK", "text/html; charset=utf-8", renderTriage(findTriage(key)));
        }
    } else if (req.path == "/plan") {
        if (req.method != "POST") {
            respond(fd, 405, "Method Not Allowed", "text/plain", "Use POST\n", "Allow: POST\r\n");
        } else {
            respond(fd, 200, "OK", "application/pdf", buildPlanPdf(parseForm(req.body)),
                    "Content-Disposition: attachment; filename=\"my-safety-plan.pdf\"\r\n");
        }
    } else {
        respond(fd, 404, "Not Found", "text/plain; charset=utf-8", "Not found\n");
    }
    close(fd);
}

}  // namespace

int main(int argc, char** argv) {
    int port = 8080;
    bool lan = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--lan") lan = true;
        else port = std::atoi(a.c_str());
    }
    if (port <= 0 || port > 65535) {
        std::cerr << "Usage: " << argv[0] << " [port] [--lan]\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { std::perror("socket"); return 1; }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(lan ? INADDR_ANY : INADDR_LOOPBACK);

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) { std::perror("bind"); return 1; }
    if (listen(srv, 64) < 0) { std::perror("listen"); return 1; }

    std::cout << "Safe Steps is running at http://" << (lan ? "0.0.0.0" : "127.0.0.1") << ':' << port
              << "  (Ctrl+C to stop)\n";

    for (;;) {
        int client = accept(srv, nullptr, nullptr);
        if (client < 0) continue;
        std::thread(handle, client).detach();
    }
}
