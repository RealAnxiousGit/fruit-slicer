// Cloudflare Worker backend for Fruit Slicer 2.0.0 cross-platform leaderboards.
// Bind a KV namespace named `SCORES` in wrangler.toml.
const SLUG_RE = /^[a-z0-9_]{1,24}$/;
const NAME_RE = /^[A-Za-z0-9 _-]{1,6}$/;
const CORS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type',
};

function jsonResp(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { 'Content-Type': 'application/json', ...CORS },
  });
}

function cleanName(raw) {
  const s = String(raw || '').replace(/[^A-Za-z0-9 _-]/g, '').trim().slice(0, 6);
  return s || 'PLAYER';
}

function kvKey(mode, isTest) {
  return (isTest ? 'test:' : 'live:') + mode;
}

function sortRows(rows) {
  rows.sort((a, b) => (b.score - a.score) || ((a.updated || 0) - (b.updated || 0)));
  return rows;
}

export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: CORS });
    const url = new URL(request.url);
    if (url.pathname === '/health') return jsonResp({ ok: true, service: 'fruit-slicer-lb', version: 1 });
    if (url.pathname !== '/v1/scores') return jsonResp({ ok: false, error: 'not_found' }, 404);

    if (request.method === 'GET') {
      const mode = url.searchParams.get('mode') || '';
      const playerId = (url.searchParams.get('player_id') || '').slice(0, 32);
      const limit = Math.min(25, Math.max(1, Number(url.searchParams.get('limit') || 10) || 10));
      const isTest = url.searchParams.get('test') === '1';
      if (!SLUG_RE.test(mode)) return jsonResp({ ok: false, error: 'bad_mode' }, 400);

      const raw = await env.SCORES.get(kvKey(mode, isTest));
      const rows = sortRows(raw ? JSON.parse(raw) : []);
      const idx = playerId ? rows.findIndex((r) => r.player_id === playerId) : -1;
      const top = rows.slice(0, limit).map(({ name, score, platform, player_id, updated }) => ({
        name, score, platform: platform || 'unknown', player_id, updated,
      }));
      return jsonResp({
        ok: true,
        mode,
        top,
        you: idx >= 0 ? { rank: idx + 1, name: rows[idx].name, score: rows[idx].score } : null,
        total: rows.length,
      });
    }

    if (request.method === 'POST') {
      let body;
      try { body = await request.json(); } catch { return jsonResp({ ok: false, error: 'bad_json' }, 400); }
      const mode = String(body.mode || '');
      const playerId = String(body.player_id || '').replace(/[^a-zA-Z0-9_-]/g, '').slice(0, 32);
      const score = Math.max(0, Math.floor(Number(body.score) || 0));
      const platform = String(body.platform || 'unknown').slice(0, 12);
      const isTest = body.test === 1 || body.test === true;
      if (!SLUG_RE.test(mode) || !playerId) return jsonResp({ ok: false, error: 'bad_params' }, 400);
      const name = cleanName(body.name);
      const now = Math.floor(Date.now() / 1000);

      const key = kvKey(mode, isTest);
      const raw = await env.SCORES.get(key);
      const rows = raw ? JSON.parse(raw) : [];
      const existing = rows.find((r) => r.player_id === playerId);
      if (existing) {
        existing.score = Math.max(Number(existing.score) || 0, score);
        existing.name = name;
        existing.platform = platform;
        existing.updated = now;
      } else {
        rows.push({ player_id: playerId, name, score, platform, updated: now });
      }
      sortRows(rows);
      if (rows.length > 500) rows.length = 500;
      await env.SCORES.put(key, JSON.stringify(rows), isTest ? { expirationTtl: 3600 } : undefined);

      const idx = rows.findIndex((r) => r.player_id === playerId);
      const top = rows.slice(0, 10).map((r) => ({
        name: r.name, score: r.score, platform: r.platform, player_id: r.player_id, updated: r.updated,
      }));
      return jsonResp({
        ok: true,
        mode,
        top,
        you: idx >= 0 ? { rank: idx + 1, name: rows[idx].name, score: rows[idx].score } : null,
        total: rows.length,
      });
    }

    if (request.method === 'DELETE') {
      const mode = url.searchParams.get('mode') || '';
      const playerId = url.searchParams.get('player_id') || '';
      const isTest = url.searchParams.get('test') === '1';
      if (!isTest || !SLUG_RE.test(mode) || !playerId) return jsonResp({ ok: false, error: 'forbidden' }, 403);
      const key = kvKey(mode, true);
      const raw = await env.SCORES.get(key);
      const rows = (raw ? JSON.parse(raw) : []).filter((r) => r.player_id !== playerId);
      await env.SCORES.put(key, JSON.stringify(rows), { expirationTtl: 3600 });
      return jsonResp({ ok: true });
    }

    return jsonResp({ ok: false, error: 'method_not_allowed' }, 405);
  },
};
