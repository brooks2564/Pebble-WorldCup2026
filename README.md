# World Cup 2026 Live

A live FIFA World Cup 2026 scoreboard watchface for Pebble smartwatches. Scores, match status, group info, and a full tournament ticker — all on your wrist.

---

## What It Shows

**Score area:**
- Away and home team abbreviations
- Live score updated every minute
- Match minute and period (1H / 2H / ET / PK)
- Group or stage info (e.g. `Group A`, `Round of 16`)

**Pre-match** (shown before kickoff):
- Scheduled kickoff time
- TV network carrying the match

**Final:**
- Final score
- Next scheduled match for your team

**Ticker strip:**
- Scrolling strip showing scores from all other matches today
- Cycles through automatically
- Speed is adjustable in settings

**Always visible:**
- Current time (large)
- Date
- Battery bar at bottom of screen (optional)

**Team colors** (Pebble Time 2 only):
- Team abbreviations rendered in each nation's primary color with black outline

---

## Settings

Open the Pebble app → tap the watchface → **Settings**

- **Favorite Team** — choose from all 48 World Cup nations
- **Vibrate on Goal** — double pulse when your team scores
- **Battery Bar** — toggle the battery indicator
- **Ticker Speed** — how long each match is shown (5s, 10s, 30s, 60s)

---

## Supported Watches

Runs on 6 Pebble platforms:

- **Pebble** and **Pebble Steel** — original B&W classics
- **Pebble Time** and **Pebble Time Steel** — full color
- **Pebble Time Round** — color, round screen
- **Pebble Time 2** — color, large screen (team colors enabled)
- **Pebble 2 SE** and **Pebble 2 HR** — heart rate models
- **Pebble 2 Duo** — B&W rectangular

---

## Data Source

All data comes from the [ESPN Soccer API](https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard) — free, no API key required. Match data refreshes every minute while the watchface is active.

---

## Building

Requires the [Pebble SDK](https://developer.rebble.io/developer.pebble.com/sdk/index.html).

```bash
pebble build
pebble install --phone YOUR_PHONE_IP
```

---

## Project Structure

```
src/c/main.c          — Watchface C code (drawing, animation, message handling)
src/pkjs/index.js     — PebbleKit JS (fetches scores, sends data to watch)
src/pkjs/config.json  — Clay settings configuration (48-team dropdown, toggles)
package.json          — Pebble project config and message keys
```
