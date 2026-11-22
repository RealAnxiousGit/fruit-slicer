# Authenticates Wrangler (if needed), creates/locates the SCORES KV namespace,
# updates wrangler.toml, deploys fruit-slicer-lb, and runs check-leaderboard-backend.py.
$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot

Write-Host "== 1. Checking Cloudflare Wrangler login..."
$who = & npx --yes wrangler whoami 2>&1 | Out-String
if ($who -match "You are not authenticated") {
    Write-Host "Opening Cloudflare OAuth login in your browser..."
    & npx --yes wrangler login
    if ($LASTEXITCODE -ne 0) { throw "wrangler login failed." }
}

Write-Host "== 2. Ensuring SCORES KV namespace exists..."
$tomlPath = Join-Path $PSScriptRoot "wrangler.toml"
$toml = Get-Content $tomlPath -Raw
if ($toml -match 'REPLACE_WITH_KV_NAMESPACE_ID') {
    $kvOut = & npx --yes wrangler kv namespace create SCORES 2>&1 | Out-String
    Write-Host $kvOut
    $kvId = $null
    if ($kvOut -match '"id"\s*:\s*"([0-9a-fA-F]{32})"') {
        $kvId = $Matches[1]
    } elseif ($kvOut -match 'id\s*=\s*"([0-9a-fA-F]{32})"') {
        $kvId = $Matches[1]
    } else {
        $listJson = & npx --yes wrangler kv namespace list 2>&1 | Out-String
        if ($listJson -match '"id"\s*:\s*"([0-9a-fA-F]{32})"') {
            $kvId = $Matches[1]
        }
    }
    if (-not $kvId) { throw "Could not determine KV namespace ID from wrangler output." }
    $toml = $toml -replace 'REPLACE_WITH_KV_NAMESPACE_ID', $kvId
    Set-Content -Path $tomlPath -Value $toml -NoNewline
    Write-Host "Updated wrangler.toml with KV namespace id = $kvId"
}

Write-Host "== 3. Deploying fruit-slicer-lb Cloudflare Worker..."
$depOut = & npx --yes wrangler deploy 2>&1 | Out-String
Write-Host $depOut
$url = "https://fruit-slicer-lb.realanxiousgit.workers.dev"
if ($depOut -match '(https://fruit-slicer-lb\.[a-zA-Z0-9_-]+\.workers\.dev)') {
    $url = $Matches[1]
}
Write-Host "Deployed Worker URL: $url"

Write-Host "== 4. Verifying live leaderboard endpoint (TLS 1.2 + /health + /v1/scores)..."
& python (Join-Path $PSScriptRoot "..\check-leaderboard-backend.py") --url $url
