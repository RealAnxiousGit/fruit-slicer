# Fruit Slicer Leaderboard Backend

Zero-cost Cloudflare Worker + Workers KV backend shared across Windows, Linux, Android, and Nintendo 3DS (`libcurl` + `mbedTLS` TLS 1.2).

## Deploy in 3 commands
```bash
cd scripts/leaderboard_server
npx wrangler login
npx wrangler kv namespace create SCORES   # paste the returned id into wrangler.toml
npx wrangler deploy
```

Put your deployed URL in `source/leaderboard.cpp` (`kDefaultBaseUrl`) or test without recompiling via:
- Environment variable `FRUITSLICER_LB_URL` (desktop/Linux), or
- A single-line `lb_url.txt` file inside the game save folder.

Verify anytime with:
```bash
python3 scripts/check-leaderboard-backend.py --url https://<your-worker>.workers.dev
```
