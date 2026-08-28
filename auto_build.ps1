# auto_build.ps1
# ============================================================================
# Auto remote-build helper for the ESP32-C3 piano firmware.
#
# When the local network cannot fetch the RISC-V toolchain, this script:
#   1. Reads a GitHub PAT from the user (masked input).
#   2. Validates the PAT and fetches the GitHub login.
#   3. Asks for a repository name (default: esp32c3-piano).
#   4. Creates the repository under the user's account via REST API.
#   5. Configures origin and runs `git push -u origin main`.
#   6. Polls the GitHub Actions workflow until it succeeds or times out.
#   7. Downloads the `piano-firmware` artifact and unpacks it to dist/.
# ============================================================================

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

# ---- 1. Read GitHub PAT ---------------------------------------------------
Write-Host '========================================' -ForegroundColor Cyan
Write-Host '  ESP32-C3 piano - remote build automation' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''
Write-Host 'A GitHub Personal Access Token (PAT) is required.'
Write-Host 'If you do not have one, create it at:'
Write-Host '  https://github.com/settings/tokens/new'
Write-Host 'Scopes needed: repo, workflow.  The token is shown only once.'
Write-Host ''

$securePat = Read-Host 'Paste your GitHub PAT here' -AsSecureString
if (-not $securePat -or $securePat.Length -eq 0) {
    Write-Host 'No PAT entered, aborting.' -ForegroundColor Red
    exit 1
}
$BSTR = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePat)
$Pat = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto($BSTR)
[System.Runtime.InteropServices.Marshal]::ZeroFreeBSTR($BSTR)

$Headers = @{
    'Authorization'        = "token $Pat"
    'Accept'               = 'application/vnd.github+json'
    'X-GitHub-Api-Version' = '2022-11-28'
    'User-Agent'           = 'esp32c3-piano-autobuild'
}

# ---- 2. Validate PAT and fetch login --------------------------------------
Write-Host ''
Write-Host '[1/6] Validating PAT...' -ForegroundColor Yellow
try {
    $resp = Invoke-RestMethod -Uri 'https://api.github.com/user' -Headers $Headers -Method Get
    $GithubUser = $resp.login
    Write-Host "  -> Auth OK, user: $GithubUser" -ForegroundColor Green
} catch {
    Write-Host "  PAT validation failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 2
}

# ---- 3. Ask for repository name -------------------------------------------
$defaultName = 'esp32c3-piano'
$input = Read-Host "[2/6] Repo name (Enter for default: $defaultName)"
if ([string]::IsNullOrWhiteSpace($input)) { $RepoName = $defaultName } else { $RepoName = $input }
$RepoDesc = 'ESP32-C3 8-key touch piano firmware (auto-built)'

# ---- 4. Create the repository --------------------------------------------
Write-Host "[3/6] Creating repository $GithubUser/$RepoName ..." -ForegroundColor Yellow
$createBody = @{
    name        = $RepoName
    description = $RepoDesc
    private     = $false
    auto_init   = $false
} | ConvertTo-Json

$createHeaders = $Headers.Clone()
$createHeaders['Content-Type'] = 'application/json'
try {
    $null = Invoke-RestMethod -Uri 'https://api.github.com/user/repos' `
        -Headers $createHeaders -Method Post -Body $createBody
    Write-Host '  -> Repository created' -ForegroundColor Green
} catch {
    $errBody = ''
    try {
        $errBody = ($_.Exception.Response | ConvertTo-Json -Depth 3)
    } catch { }
    if ($errBody -match 'name already exists' -or $errBody -match '"message":"name already exists') {
        Write-Host '  -> Repository already exists, continuing' -ForegroundColor Yellow
    } else {
        Write-Host "  Failed to create: $errBody" -ForegroundColor Red
        exit 3
    }
}

# ---- 5. Push source ------------------------------------------------------
Write-Host '[4/6] Pushing source to GitHub ...' -ForegroundColor Yellow
$remote = "https://${Pat}@github.com/${GithubUser}/${RepoName}.git"
try {
    & git remote remove origin 2>$null
    if ($LASTEXITCODE -ne 0) { Write-Host '  (no prior origin to remove)' }
    & git remote add origin $remote
    $pushOutput = & git push -u origin main 2>&1
    foreach ($line in $pushOutput) { Write-Host "  $line" }
    Write-Host '  -> push complete' -ForegroundColor Green
} catch {
    Write-Host "  push failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 4
}

# ---- 6. Poll Actions workflow --------------------------------------------
Write-Host '[5/6] Polling GitHub Actions build ...' -ForegroundColor Yellow
$startTime = Get-Date
$timeoutMinutes = 15
$latest = $null
$done = $false
$attempt = 0

while (-not $done -and (((Get-Date) - $startTime).TotalMinutes -lt $timeoutMinutes)) {
    Start-Sleep -Seconds 15
    $attempt++
    try {
        $runs = Invoke-RestMethod `
            -Uri "https://api.github.com/repos/${GithubUser}/${RepoName}/actions/runs?per_page=5" `
            -Headers $Headers -Method Get
        $latest = @($runs.workflow_runs) | Sort-Object -Property created_at -Descending | Select-Object -First 1
        if ($latest) {
            $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds)
            $status = $latest.status
            $conclusion = $latest.conclusion
            Write-Host "  [${elapsed}s] workflow #$($latest.run_number): status=$status conclusion=$conclusion"
            if ($status -eq 'completed') {
                if ($conclusion -eq 'success') {
                    $done = $true
                } else {
                    Write-Host "  build FAILED. See https://github.com/${GithubUser}/${RepoName}/actions/runs/$($latest.id)" -ForegroundColor Red
                    exit 5
                }
            }
        } else {
            $elapsed = [math]::Round(((Get-Date) - $startTime).TotalSeconds)
            Write-Host "  [${elapsed}s] waiting for workflow to appear..."
        }
    } catch {
        Write-Host "  poll failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

if (-not $done) {
    Write-Host "  Timed out (${timeoutMinutes} min). Check the Actions tab manually." -ForegroundColor Red
    exit 6
}

# ---- 7. Download the artifact --------------------------------------------
Write-Host '[6/6] Downloading piano-firmware artifact ...' -ForegroundColor Yellow
$artifacts = Invoke-RestMethod `
    -Uri "https://api.github.com/repos/${GithubUser}/${RepoName}/actions/runs/$($latest.id)/artifacts" `
    -Headers $Headers -Method Get

$artifact = @($artifacts.artifacts) | Where-Object { $_.name -eq 'piano-firmware' } | Select-Object -First 1
if (-not $artifact) {
    Write-Host '  artifact piano-firmware not found on the workflow run.' -ForegroundColor Red
    exit 7
}

$zipPath = Join-Path $ScriptDir 'piano-firmware.zip'
Invoke-WebRequest -Uri $artifact.archive_download_url `
    -Headers @{ 'Authorization' = "token $Pat"; 'User-Agent' = 'esp32c3-piano-autobuild' } `
    -OutFile $zipPath -UseBasicParsing
Write-Host "  -> Downloaded: $zipPath" -ForegroundColor Green

# ---- 8. Extract firmware.bin --------------------------------------------
Add-Type -AssemblyName System.IO.Compression.FileSystem
$distDir = Join-Path $ScriptDir 'dist'
if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir | Out-Null }
$extractDir = Join-Path $ScriptDir 'piano-firmware-extracted'
if (Test-Path $extractDir) {
    Get-ChildItem -Path $extractDir -Recurse -ErrorAction SilentlyContinue | ForEach-Object { Remove-Item -Path $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
}
New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
[System.IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $extractDir)
$binPath = Join-Path $extractDir 'firmware.bin'
if (Test-Path $binPath) {
    Copy-Item $binPath (Join-Path $distDir 'firmware.bin') -Force
    $size = (Get-Item $binPath).Length
    Write-Host "  -> Extracted firmware.bin ($([math]::Round($size/1024,1)) KiB) -> $distDir\firmware.bin" -ForegroundColor Green
} else {
    Write-Host '  zip did not contain firmware.bin' -ForegroundColor Red
    exit 8
}

Write-Host ''
Write-Host '==========================================' -ForegroundColor Green
Write-Host '  DONE!  dist\firmware.bin is ready' -ForegroundColor Green
Write-Host '==========================================' -ForegroundColor Green
Write-Host ''
Write-Host 'To flash (replace COMx with your port):' -ForegroundColor Cyan
Write-Host '  python -m esptool --chip esp32c3 -p COMx write_flash 0x0 dist\firmware.bin'
Write-Host '  or just run flash.bat and pick the COM port.'
Write-Host ''
Write-Host "GitHub repository: https://github.com/${GithubUser}/${RepoName}"
Write-Host '  (login and delete or set to private if you wish)'
