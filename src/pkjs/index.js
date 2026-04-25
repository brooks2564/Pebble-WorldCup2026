// ── World Cup 2026 Live  ·  PebbleKit JS ──────────────────────────────────
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);

// Message keys — must match #define KEY_* in main.c exactly
var KEY_AWAY_ABBR    = 1;
var KEY_HOME_ABBR    = 2;
var KEY_AWAY_SCORE   = 3;
var KEY_HOME_SCORE   = 4;
var KEY_MATCH_MIN    = 5;
var KEY_MATCH_PERIOD = 6;
var KEY_STATUS       = 7;
var KEY_TEAM_IDX     = 8;
var KEY_START_TIME   = 9;
var KEY_GROUP_INFO   = 10;
var KEY_VIBRATE      = 11;
var KEY_NEXT_MATCH   = 12;
var KEY_BATTERY_BAR  = 13;
var KEY_TICKER       = 14;
var KEY_TV_NETWORK   = 15;
var KEY_TICKER_SPEED = 16;

// ESPN World Cup scoreboard — free, no API key
var SCOREBOARD_URL = "https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard";

// 48 teams — same order as TEAM_ABBR[] in main.c (index must match)
var TEAMS = [
  {abbr:"ALB",name:"Albania"},    {abbr:"ARG",name:"Argentina"},
  {abbr:"AUS",name:"Australia"},  {abbr:"AUT",name:"Austria"},
  {abbr:"BEL",name:"Belgium"},    {abbr:"BRA",name:"Brazil"},
  {abbr:"CMR",name:"Cameroon"},   {abbr:"CAN",name:"Canada"},
  {abbr:"CIV",name:"Ivory Coast"},{abbr:"COL",name:"Colombia"},
  {abbr:"CRC",name:"Costa Rica"}, {abbr:"CRO",name:"Croatia"},
  {abbr:"CZE",name:"Czech Rep."},  {abbr:"DEN",name:"Denmark"},
  {abbr:"ECU",name:"Ecuador"},    {abbr:"EGY",name:"Egypt"},
  {abbr:"ENG",name:"England"},    {abbr:"ESP",name:"Spain"},
  {abbr:"FRA",name:"France"},     {abbr:"GER",name:"Germany"},
  {abbr:"GHA",name:"Ghana"},      {abbr:"GRE",name:"Greece"},
  {abbr:"HON",name:"Honduras"},   {abbr:"HUN",name:"Hungary"},
  {abbr:"IRN",name:"Iran"},       {abbr:"IRQ",name:"Iraq"},
  {abbr:"JPN",name:"Japan"},      {abbr:"JOR",name:"Jordan"},
  {abbr:"KOR",name:"South Korea"},{abbr:"MLI",name:"Mali"},
  {abbr:"MAR",name:"Morocco"},    {abbr:"MEX",name:"Mexico"},
  {abbr:"NED",name:"Netherlands"},{abbr:"NGA",name:"Nigeria"},
  {abbr:"NZL",name:"New Zealand"},{abbr:"PAN",name:"Panama"},
  {abbr:"PAR",name:"Paraguay"},   {abbr:"POL",name:"Poland"},
  {abbr:"POR",name:"Portugal"},   {abbr:"SAU",name:"Saudi Arabia"},
  {abbr:"SCO",name:"Scotland"},   {abbr:"SEN",name:"Senegal"},
  {abbr:"SRB",name:"Serbia"},     {abbr:"SUI",name:"Switzerland"},
  {abbr:"TUR",name:"Turkey"},     {abbr:"UKR",name:"Ukraine"},
  {abbr:"URU",name:"Uruguay"},    {abbr:"USA",name:"USA"}
];

// ── Settings ───────────────────────────────────────────────────────────────
var gTeamIdx     = 47;     // USA
var gVibrate     = true;
var gBatteryBar  = true;
var gTickerSpeed = "5000";

function validSpeed(s) {
  return s === "5000" || s === "10000" || s === "30000" || s === "60000";
}

function loadFromClay() {
  var cs = {};
  try { cs = JSON.parse(localStorage.getItem("clay-settings")) || {}; } catch(e) {}
  var pIdx = parseInt(cs.TEAM_IDX, 10);
  if (!isNaN(pIdx) && pIdx >= 0 && pIdx < TEAMS.length) gTeamIdx = pIdx;
  if (cs.VIBRATE     !== undefined) gVibrate    = !!cs.VIBRATE;
  if (cs.BATTERY_BAR !== undefined) gBatteryBar = !!cs.BATTERY_BAR;
  var spd = cs.TICKER_SPEED !== undefined ? String(cs.TICKER_SPEED) : null;
  if (validSpeed(spd)) gTickerSpeed = spd;
}
loadFromClay();

// ── Date utilities ─────────────────────────────────────────────────────────
function pad2(n) { return n < 10 ? "0" + n : "" + n; }
function toYMD(d) {
  return "" + d.getFullYear() + pad2(d.getMonth()+1) + pad2(d.getDate());
}
function todayStr()     { return toYMD(new Date()); }
function yesterdayStr() { var d = new Date(); d.setDate(d.getDate()-1); return toYMD(d); }
function tomorrowStr()  { var d = new Date(); d.setDate(d.getDate()+1); return toYMD(d); }

function formatStartTime(isoStr) {
  if (!isoStr) return "";
  try {
    var d = new Date(isoStr);
    if (isNaN(d.getTime())) return "";
    var h = d.getHours(), m = d.getMinutes();
    var ampm = h >= 12 ? "PM" : "AM";
    h = h % 12; if (h === 0) h = 12;
    return h + ":" + (m < 10 ? "0" + m : m) + " " + ampm;
  } catch(e) { return ""; }
}

function formatDayOfWeek(isoStr) {
  if (!isoStr) return "";
  try {
    var d = new Date(isoStr);
    if (isNaN(d.getTime())) return "";
    return ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][d.getDay()];
  } catch(e) { return ""; }
}

function isWithinTwoHours(isoStr) {
  if (!isoStr) return false;
  try {
    var gameMs = new Date(isoStr).getTime();
    var nowMs  = Date.now();
    return gameMs > nowMs && (gameMs - nowMs) <= 2 * 60 * 60 * 1000;
  } catch(e) { return false; }
}

// ── ESPN data helpers ──────────────────────────────────────────────────────
function getCompetitors(ev) {
  var comps = (ev.competitions && ev.competitions[0] && ev.competitions[0].competitors) || [];
  var away = null, home = null;
  for (var i = 0; i < comps.length; i++) {
    if (comps[i].homeAway === "away") away = comps[i];
    else home = comps[i];
  }
  if (!away && comps.length > 0) away = comps[0];
  if (!home && comps.length > 1) home = comps[1];
  return { away: away, home: home };
}

function getAbbr(comp) {
  if (!comp || !comp.team) return "---";
  return (comp.team.abbreviation || "---").toUpperCase().substring(0, 4);
}

function getScore(comp) {
  if (!comp) return 0;
  return parseInt(comp.score || "0", 10) || 0;
}

function getGroupInfo(ev) {
  var notes = (ev.competitions && ev.competitions[0] && ev.competitions[0].notes) || [];
  for (var i = 0; i < notes.length; i++) {
    if (notes[i].headline) return notes[i].headline.substring(0, 20);
  }
  return "";
}

function getMatchMin(ev) {
  var state = (ev.status && ev.status.type && ev.status.type.state) || "";
  var period = (ev.status && ev.status.period) || 0;
  var clock  = (ev.status && ev.status.displayClock) || "";
  if (state === "in") {
    var half = period === 1 ? "1H" : period === 2 ? "2H" : period === 3 ? "ET" : "PK";
    // displayClock is often "45:00" — strip trailing :00
    var min = clock.replace(/:00$/, "").replace(/:.*$/, "");
    if (min && min !== "0") return (min + "' " + half).substring(0, 10);
    return half;
  } else if (state === "post") {
    if (period >= 4) return "FT (PK)";
    if (period >= 3) return "FT (ET)";
    return "FT";
  }
  return "";
}

function getTV(ev) {
  var comp = ev.competitions && ev.competitions[0];
  if (!comp) return "";
  var bcs = comp.broadcasts || [];
  var national = ["FOX", "ESPN", "TNT", "Telemundo", "Peacock", "Apple TV+", "FS1", "TBS"];
  for (var i = 0; i < bcs.length; i++) {
    var names = bcs[i].names || [];
    for (var j = 0; j < names.length; j++) {
      for (var k = 0; k < national.length; k++) {
        if (names[j].indexOf(national[k]) !== -1) return names[j].substring(0, 20);
      }
    }
  }
  // Any broadcast
  for (var i = 0; i < bcs.length; i++) {
    if (bcs[i].names && bcs[i].names[0]) return bcs[i].names[0].substring(0, 20);
  }
  return "";
}

// ── Event lookup helpers ───────────────────────────────────────────────────
function hasTeam(ev, target) {
  var comps = (ev.competitions && ev.competitions[0] && ev.competitions[0].competitors) || [];
  for (var i = 0; i < comps.length; i++) {
    if ((comps[i].team && (comps[i].team.abbreviation || "")).toUpperCase() === target) return true;
  }
  return false;
}

function findByState(events, target, state) {
  for (var i = 0; i < events.length; i++) {
    var ev = events[i];
    var s  = (ev.status && ev.status.type && ev.status.type.state) || "";
    if (s === state && hasTeam(ev, target)) return ev;
  }
  return null;
}

function findByStateAndDate(events, target, state, dateStr) {
  for (var i = 0; i < events.length; i++) {
    var ev = events[i];
    if (ev._fetchDate !== dateStr) continue;
    var s  = (ev.status && ev.status.type && ev.status.type.state) || "";
    if (s === state && hasTeam(ev, target)) return ev;
  }
  return null;
}

// ── Ticker builder ─────────────────────────────────────────────────────────
function buildTicker(allEvents, today, myTarget) {
  var parts = [];
  for (var i = 0; i < allEvents.length; i++) {
    var ev = allEvents[i];
    if (ev._fetchDate !== today) continue;
    var comps = getCompetitors(ev);
    if (!comps.away || !comps.home) continue;
    var awayAbbr = getAbbr(comps.away);
    var homeAbbr = getAbbr(comps.home);
    if (awayAbbr === myTarget || homeAbbr === myTarget) continue;

    var state = (ev.status && ev.status.type && ev.status.type.state) || "";
    var entry = "";
    if (state === "pre") {
      var t = formatStartTime(ev.date);
      entry = awayAbbr + " vs " + homeAbbr + (t ? " " + t : "");
    } else if (state === "post") {
      entry = awayAbbr + " " + getScore(comps.away) + "-" +
              homeAbbr + " " + getScore(comps.home) + " FT";
    } else {
      var min = getMatchMin(ev);
      entry = awayAbbr + " " + getScore(comps.away) + "-" +
              homeAbbr + " " + getScore(comps.home) +
              (min ? " " + min : "");
    }
    if (entry.length > 21) entry = entry.substring(0, 21);
    parts.push(entry);
  }
  return parts.join("|");
}

// ── Data fetch ─────────────────────────────────────────────────────────────
function fetchDate(dateStr, callback) {
  var url = SCOREBOARD_URL + "?dates=" + dateStr;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var data   = JSON.parse(xhr.responseText);
        var events = data.events || [];
        for (var i = 0; i < events.length; i++) events[i]._fetchDate = dateStr;
        callback(events);
      } catch(e) {
        console.log("[WC] Parse error for " + dateStr + ": " + e);
        callback([]);
      }
    } else {
      console.log("[WC] HTTP " + xhr.status + " for " + dateStr);
      callback([]);
    }
  };
  xhr.onerror = function() { callback([]); };
  xhr.send();
}

function fetchMatchData(teamIdx) {
  if (teamIdx < 0 || teamIdx >= TEAMS.length) { sendOffMessage(); return; }
  var abbr      = TEAMS[teamIdx].abbr;
  var today     = todayStr();
  var yesterday = yesterdayStr();
  var tomorrow  = tomorrowStr();

  console.log("[WC] Fetching for " + abbr);

  var allEvents = [];
  var pending   = 3;

  function onDate(events) {
    for (var i = 0; i < events.length; i++) allEvents.push(events[i]);
    if (--pending === 0) processEvents(allEvents, abbr, today, yesterday, tomorrow);
  }

  fetchDate(yesterday, onDate);
  fetchDate(today,     onDate);
  fetchDate(tomorrow,  onDate);
}

function processEvents(allEvents, abbr, today, yesterday, tomorrow) {
  var target = abbr.toUpperCase();
  var ev = null;

  // Priority: live → pre within 2h → final today → final yesterday → pre today → pre tomorrow
  ev = findByState(allEvents, target, "in");
  if (!ev) {
    var pre = findByStateAndDate(allEvents, target, "pre", today);
    if (pre && isWithinTwoHours(pre.date)) ev = pre;
  }
  if (!ev) ev = findByStateAndDate(allEvents, target, "post", today);
  if (!ev) ev = findByStateAndDate(allEvents, target, "post", yesterday);
  if (!ev) ev = findByStateAndDate(allEvents, target, "pre",  today);
  if (!ev) ev = findByStateAndDate(allEvents, target, "pre",  tomorrow);

  if (!ev) { sendOffMessage(); return; }

  var state  = (ev.status && ev.status.type && ev.status.type.state) || "";
  var status = state === "in" ? "live" : state === "post" ? "final" : "pre";

  var comps    = getCompetitors(ev);
  var awayAbbr = getAbbr(comps.away);
  var homeAbbr = getAbbr(comps.home);

  // Next match for final/off screens
  var nextMatch = "";
  if (status === "final") {
    var nextEv = findByStateAndDate(allEvents, target, "pre", today) ||
                 findByStateAndDate(allEvents, target, "pre", tomorrow);
    if (nextEv) {
      var nc = getCompetitors(nextEv);
      var opp = getAbbr(nc.away) === target ? getAbbr(nc.home) : getAbbr(nc.away);
      var day = formatDayOfWeek(nextEv.date);
      var t   = formatStartTime(nextEv.date);
      nextMatch = (opp + (day ? " " + day : "") + (t ? " " + t : "")).substring(0, 22);
    }
  }

  var msg = {};
  msg[KEY_AWAY_ABBR]    = awayAbbr;
  msg[KEY_HOME_ABBR]    = homeAbbr;
  msg[KEY_AWAY_SCORE]   = getScore(comps.away);
  msg[KEY_HOME_SCORE]   = getScore(comps.home);
  msg[KEY_MATCH_MIN]    = getMatchMin(ev);
  msg[KEY_MATCH_PERIOD] = (ev.status && ev.status.period) || 0;
  msg[KEY_STATUS]       = status;
  msg[KEY_START_TIME]   = status === "pre" ? formatStartTime(ev.date) : "";
  msg[KEY_GROUP_INFO]   = getGroupInfo(ev);
  msg[KEY_VIBRATE]      = gVibrate ? 1 : 0;
  msg[KEY_NEXT_MATCH]   = nextMatch;
  msg[KEY_BATTERY_BAR]  = gBatteryBar ? 1 : 0;
  msg[KEY_TICKER]       = buildTicker(allEvents, today, target);
  msg[KEY_TV_NETWORK]   = status === "pre" ? getTV(ev) : "";

  sendMessage(msg);
}

function sendOffMessage() {
  var msg = {};
  msg[KEY_STATUS] = "off";
  sendMessage(msg);
}

function sendMessage(dict) {
  // Send TICKER separately to stay under 512-byte AppMessage inbox limit
  var ticker = dict[KEY_TICKER];
  delete dict[KEY_TICKER];

  Pebble.sendAppMessage(dict,
    function() {
      console.log("[WC] Message sent OK");
      if (ticker !== undefined) {
        var tm = {};
        tm[KEY_TICKER] = ticker;
        Pebble.sendAppMessage(tm,
          function()  { console.log("[WC] Ticker sent OK"); },
          function(e) { console.log("[WC] Ticker NACK: " + JSON.stringify(e)); }
        );
      }
    },
    function(e) { console.log("[WC] NACK: " + JSON.stringify(e)); }
  );
}

// ── Pebble events ──────────────────────────────────────────────────────────
Pebble.addEventListener("ready", function() {
  console.log("[WC] Ready — team: " + TEAMS[gTeamIdx].abbr);
  fetchMatchData(gTeamIdx);
});

Pebble.addEventListener("appmessage", function(e) {
  var msg = e.payload;
  var idx = parseInt(msg[KEY_TEAM_IDX], 10);
  if (!isNaN(idx) && idx >= 0 && idx < TEAMS.length) gTeamIdx = idx;
  fetchMatchData(gTeamIdx);
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e || !e.response || e.response === "CANCELLED") return;
  try {
    loadFromClay();
    fetchMatchData(gTeamIdx);
  } catch(ex) {
    console.log("[WC] webviewclosed error: " + ex);
  }
});
