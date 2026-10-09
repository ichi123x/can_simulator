# Claude Code の Stop フック: Claude が作業を終える前に、ビルドとテストを自動で実行する。
# setup.ps1 が .claude/hooks/run-tests.ps1 にコピーし、.claude/settings.json に登録する。
#
# - C++ のソースか CMakeLists.txt が前回の成功後に変わったときだけ実行する（会話だけのときは何もしない）
# - 失敗したら終了コード 2 で終わり、エラー内容を Claude に返して作業を続けさせる
# - 同じ停止で 2 回目に呼ばれたとき（stop_hook_active）は、無限ループを避けるため何もしない

$ErrorActionPreference = 'Continue'

# Claude Code（UTF-8 で読む）に日本語が化けずに届くよう、書き出す直前に出力を UTF-8 にする。
# 先に切り替えると、cmake・MSBuild の出力（コンソールのコードページ）を読み違えるため、書く直前だけにする
function Use-Utf8Output {
    try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false } catch { }
}

$raw = [Console]::In.ReadToEnd()
try { $payload = $raw | ConvertFrom-Json } catch { $payload = $null }
if ($payload -and $payload.stop_hook_active) { exit 0 }

$root = $env:CLAUDE_PROJECT_DIR
if (-not $root) { $root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path }
Set-Location $root

if (-not (Test-Path 'CMakeLists.txt')) { exit 0 }

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    if (-not ${env:ProgramFiles(x86)}) { return $null }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath
        if ($vs) {
            $p = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $p) { return $p }
        }
    }
    return $null
}

# 前回の成功以降に変わったファイルがあるか
$marker = Join-Path $root 'build/.last-test-ok'
$exts = @('.cpp', '.cc', '.cxx', '.h', '.hpp', '.inl')
$skip = @('build', '.git', '.vs', '.specify', '.claude', 'out', 'x64', 'Debug', 'Release')
$changed = Get-ChildItem -Path $root -Force -ErrorAction SilentlyContinue |
    Where-Object { -not ($_.PSIsContainer -and ($skip -contains $_.Name)) } |
    ForEach-Object { if ($_.PSIsContainer) { Get-ChildItem -Path $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue } else { $_ } } |
    Where-Object {
        $_.FullName -notmatch '[\\/](x64|Debug|Release)[\\/]' -and
        (($exts -contains $_.Extension.ToLower()) -or $_.Name -eq 'CMakeLists.txt')
    }
if (Test-Path $marker) {
    $last = (Get-Item -Force $marker).LastWriteTime
    $changed = $changed | Where-Object { $_.LastWriteTime -gt $last }
}
if (-not $changed) { exit 0 }

# テスト名（TEST の 1・2 番目の引数）に英数字以外があると、ctest が渡すフィルターが化けて
# 0 件実行のまま「成功」になる。ビルドの前に見つけて止める
function Find-NonAsciiTestNames {
    $hits = @()
    if (-not (Test-Path 'tests')) { return $hits }
    foreach ($f in (Get-ChildItem -Path 'tests' -Recurse -File -Include *.cpp, *.cc, *.cxx, *.h, *.hpp -ErrorAction SilentlyContinue)) {
        $n = 0
        foreach ($line in [System.IO.File]::ReadAllLines($f.FullName, [System.Text.Encoding]::UTF8)) {
            $n++
            if ($line -match '^\s*(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*([^,]+),\s*([^)]+)\)' -and ($Matches[2] + $Matches[3]) -match '[^\x00-\x7F]') {
                $hits += ('{0}:{1}: {2}' -f (Resolve-Path -Relative $f.FullName), $n, $line.Trim())
            }
        }
    }
    return $hits
}
$NonAsciiMessage = 'テスト名に日本語など英数字以外の文字があります。ctest が渡すフィルターが化けて、0 件実行のまま「成功」になってしまいます。テスト名（TEST の 1・2 番目）は英数字と _ だけにし、日本語はコメントとアサーションのメッセージに書いてください。'
$bad = @(Find-NonAsciiTestNames)
if ($bad.Count) {
    Use-Utf8Output
    [Console]::Error.WriteLine("$NonAsciiMessage`n$($bad -join "`n")")
    exit 2
}

$cmake = Find-CMake
if (-not $cmake) {
    Use-Utf8Output
    [Console]::Error.WriteLine('cmake が見つからないため、テストを実行できませんでした。setup.ps1 でツールを確認してください。')
    exit 0
}
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (-not (Test-Path $ctest)) { $ctest = 'ctest' }

if (-not (Test-Path 'build/CMakeCache.txt')) {
    $out = & $cmake -S . -B build 2>&1
    if ($LASTEXITCODE -ne 0) {
        $text = ($out | ForEach-Object { "$_" } | Select-Object -Last 60) -join "`n"
        Use-Utf8Output
    [Console]::Error.WriteLine("CMake の構成に失敗しました。直してから作業を終えてください。`n$text")
        exit 2
    }
}

$out = & $cmake --build build --config Debug 2>&1
if ($LASTEXITCODE -ne 0) {
    $text = ($out | ForEach-Object { "$_" } | Select-Object -Last 80) -join "`n"
    Use-Utf8Output
    [Console]::Error.WriteLine("ビルドに失敗しました。直してから作業を終えてください。`n$text")
    exit 2
}

$out = & $ctest --test-dir build -C Debug --output-on-failure -LE slow 2>&1
if ($LASTEXITCODE -ne 0) {
    $text = ($out | ForEach-Object { "$_" } | Select-Object -Last 120) -join "`n"
    Use-Utf8Output
    [Console]::Error.WriteLine("テストが失敗しています。テストや期待値は書き換えずに、実装を直してください（仕様の誤りが疑われるときは止めて人間に確認する）。`n$text")
    exit 2
}

New-Item -ItemType File -Force -Path $marker | Out-Null
exit 0
