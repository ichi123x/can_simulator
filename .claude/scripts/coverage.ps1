<#
.SYNOPSIS
  カバレッジを測り、テストで実行されていない行（赤い行）の一覧を出す。/coverage コマンドが使う。

.DESCRIPTION
  1. Debug でビルドする（build/ が無ければ先に構成する）
  2. ctest（slow を除く）で全件成功を確かめる
  3. OpenCppCoverage の下でもう一度 ctest を実行し、build/coverage/ に HTML と Cobertura XML を出す
  4. ファイルごとの率と赤い行を build/coverage/uncovered.txt に書き、画面にも出す
     tests/coverage-hold.md に載っている行（人が「テストしない」と決めた行）は「保留」として分けて出す
  setup.ps1 がキットからコピーし、__SRC__ をテスト対象のフォルダーに置き換える。
  終了コード: 0 = 測れた、2 = 構成・ビルド・テスト・測定のどれかが失敗した、
             3 = 測れる行が 0 行（テストが対象のコードをまだ 1 行も呼んでいない）

.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File .claude/scripts/coverage.ps1
#>
[CmdletBinding()]
param(
    [string]$SourceDir = 'MFCApplication1/CanSim'
)

$ErrorActionPreference = 'Continue'

# Claude Code（UTF-8 で読む）に日本語が化けずに届くよう、書き出す直前に出力を UTF-8 にする。
# 先に切り替えると、cmake・MSBuild の出力（コンソールのコードページ）を読み違えるため、書く直前だけにする
function Use-Utf8Output {
    try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false } catch { }
}

$root = $env:CLAUDE_PROJECT_DIR
if (-not $root) { $root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path }
Set-Location $root

function Fail([string]$what, $out, [int]$lines = 80) {
    $text = ($out | ForEach-Object { "$_" } | Select-Object -Last $lines) -join "`n"
    Use-Utf8Output
    [Console]::Error.WriteLine("$what`n$text")
    exit 2
}

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

function Find-OpenCppCoverage {
    $cmd = Get-Command OpenCppCoverage -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not $base) { continue }
        $p = Join-Path $base 'OpenCppCoverage\OpenCppCoverage.exe'
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Normalize([string]$path) { return $path.Replace('/', '\').Trim().ToLowerInvariant() }

# tests/coverage-hold.md の表（| ファイル | コード | 理由 | 決めた日 |）を読む
function Read-Holds {
    $holds = @()
    $file = Join-Path $root 'tests/coverage-hold.md'
    if (-not (Test-Path $file)) { return $holds }
    foreach ($line in (Get-Content $file -Encoding UTF8)) {
        if ($line -notmatch '^\s*\|') { continue }
        $mark = [string][char]1  # \| （表の中の |）を一時的に置き換える
        $cells = @(($line.Replace('\|', $mark) -split '\|') | ForEach-Object { $_.Replace($mark, '|').Trim() })
        # 先頭と末尾の | で空の要素ができる
        if ($cells.Count -lt 5) { continue }
        $path = $cells[1]; $code = $cells[2].Trim('`').Trim(); $reason = $cells[3]
        if ($path -eq 'ファイル' -or $path -match '^-+$' -or -not $code) { continue }
        $holds += [pscustomobject]@{ Path = (Normalize $path); Code = $code; Reason = $reason }
    }
    return $holds
}

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
if ($bad.Count) { Fail $NonAsciiMessage $bad }

$cmake = Find-CMake
if (-not $cmake) { Fail 'cmake が見つかりません。' @() }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
if (-not (Test-Path $ctest)) { $ctest = 'ctest' }
$occ = Find-OpenCppCoverage
if (-not $occ) {
    Fail 'OpenCppCoverage が見つかりません。winget install --id OpenCppCoverage.OpenCppCoverage -e で入れてください（setup.ps1 -InstallMissing でも入ります）。' @()
}
$src = Join-Path $root $SourceDir
if (-not (Test-Path $src)) { Fail "テスト対象のフォルダーがありません: $SourceDir（.claude/scripts/coverage.ps1 の SourceDir を直してください）" @() }

# 1. ビルド
if (-not (Test-Path 'build/CMakeCache.txt')) {
    $out = & $cmake -S . -B build 2>&1
    if ($LASTEXITCODE -ne 0) { Fail 'CMake の構成に失敗しました。' $out 60 }
}
$out = & $cmake --build build --config Debug 2>&1
if ($LASTEXITCODE -ne 0) { Fail 'ビルドに失敗しました。' $out }

# 2. テスト（先に普通に実行して、全件成功を確かめる）
$out = & $ctest --test-dir build -C Debug -LE slow --output-on-failure 2>&1
if ($LASTEXITCODE -ne 0) { Fail 'テストが失敗しています。先にテストを通してください。' $out 120 }

# 3. 測定（ctest が起動するテストの実行ファイルも --cover_children で測る）
$covDir = Join-Path $root 'build/coverage'
if (Test-Path $covDir) { Remove-Item -Recurse -Force $covDir }
New-Item -ItemType Directory -Force -Path $covDir | Out-Null
$xmlPath = Join-Path $covDir 'coverage.xml'
$out = & $occ --sources $src --cover_children `
    --export_type "html:$covDir" --export_type "cobertura:$xmlPath" `
    -- $ctest --test-dir build -C Debug -LE slow 2>&1
if (-not (Test-Path $xmlPath)) { Fail 'OpenCppCoverage の結果（coverage.xml）ができませんでした。' $out 40 }

# 4. 集計（同じソースが複数のテストの実行ファイルに出てくるので、行ごとに合わせる）
# DOCTYPE（外部の DTD）は読まない
[xml]$xml = (Get-Content $xmlPath -Raw -Encoding UTF8) -replace '<!DOCTYPE[^>]*>', ''
$base = ''
$sourceNode = $xml.SelectSingleNode('//sources/source')
if ($sourceNode) { $base = $sourceNode.InnerText.Trim() }
$files = @{}
foreach ($cls in $xml.SelectNodes('//class')) {
    $name = $cls.GetAttribute('filename')
    $full = if ([System.IO.Path]::IsPathRooted($name) -or -not $base) { $name } else { $base.TrimEnd('\', '/') + '\' + $name.TrimStart('\', '/') }
    $key = Normalize $full
    if (-not $files.ContainsKey($key)) { $files[$key] = @{ Full = $full; Lines = @{} } }
    foreach ($ln in $cls.SelectNodes('lines/line')) {
        $n = [int]$ln.GetAttribute('number')
        $hit = [long]$ln.GetAttribute('hits') -gt 0
        $lines = $files[$key].Lines
        $lines[$n] = ($lines.ContainsKey($n) -and $lines[$n]) -or $hit
    }
}

$rootKey = (Normalize $root).TrimEnd('\') + '\'
$rootNoDrive = ($rootKey -replace '^[a-z]:\\', '')
$holds = Read-Holds
$rows = @()
$totalLines = 0; $totalCovered = 0; $totalHeld = 0
foreach ($key in $files.Keys) {
    $f = $files[$key]
    # ルートからの相対パスにする（Cobertura のパスはドライブ名が別になっていることがある）
    $rel = $f.Full
    if ($key.StartsWith($rootKey)) {
        $rel = $f.Full.Substring($rootKey.Length)
    } else {
        $i = $key.IndexOf($rootNoDrive)
        if ($i -ge 0) { $rel = $f.Full.Substring($i + $rootNoDrive.Length) }
    }
    $relKey = Normalize $rel
    $code = @()
    if (Test-Path $f.Full) { $code = @(Get-Content $f.Full -Encoding UTF8) }
    $red = @(); $held = @()
    foreach ($n in ($f.Lines.Keys | Sort-Object)) {
        if ($f.Lines[$n]) { continue }
        $t = if ($n -ge 1 -and $n -le $code.Count) { $code[$n - 1].Trim() } else { '' }
        $h = $holds | Where-Object { $_.Path -eq $relKey -and $_.Code -eq $t } | Select-Object -First 1
        $item = [pscustomobject]@{ Line = $n; Code = $t; Reason = $(if ($h) { $h.Reason } else { '' }) }
        if ($h) { $held += $item } else { $red += $item }
    }
    $count = $f.Lines.Count
    $covered = @($f.Lines.Values | Where-Object { $_ }).Count
    $totalLines += $count; $totalCovered += $covered; $totalHeld += $held.Count
    $rows += [pscustomobject]@{
        Path = $rel.Replace('\', '/'); Lines = $count; Covered = $covered
        Pct = $(if ($count) { 100.0 * $covered / $count } else { 100.0 }); Red = $red; Held = $held
    }
}
$rows = @($rows | Sort-Object Pct, Path)

$report = New-Object System.Collections.Generic.List[string]
if ($totalLines -eq 0) {
    # 0 行を「100%」と出すと誤解のもとになる。テストが対象のコードを呼んでいないと、
    # リンカーが使われないコードを実行ファイルに入れないため、測る行そのものが無くなる
    $report.Add("カバレッジ（$(Get-Date -Format 'yyyy-MM-dd HH:mm')、Debug、slow を除く、対象: $SourceDir）")
    $report.Add('測れる行が 0 行でした（率は出しません）。')
    $report.Add("テストが $SourceDir/ のコードをまだ 1 行も呼んでいないと考えられます。")
    $report.Add('仕様（spec.md の受け入れ条件など）に沿って、対象のコードを呼ぶテストを書いてから、もう一度測ってください。')
    $report.Add('テストが対象のコードを呼んでいるのに 0 行のときは、-SourceDir の指定（.claude/scripts/coverage.ps1 の先頭）を確かめてください。')
    $reportText = $report -join "`r`n"
    [System.IO.File]::WriteAllText((Join-Path $covDir 'uncovered.txt'), $reportText + "`r`n", (New-Object System.Text.UTF8Encoding $true))
    Use-Utf8Output
    Write-Output $reportText
    exit 3
}
$pct = 100.0 * $totalCovered / $totalLines
$uncovered = $totalLines - $totalCovered
$report.Add("カバレッジ（$(Get-Date -Format 'yyyy-MM-dd HH:mm')、Debug、slow を除く、対象: $SourceDir）")
$report.Add(('全体: {0:N1}%（{1:N0} 行中、未カバー {2} 行、うち保留 {3} 行）' -f $pct, $totalLines, $uncovered, $totalHeld))
$report.Add('')
$report.Add('ファイル別（低い順）')
foreach ($r in $rows) {
    $report.Add(('  {0,5:N1}%  {1}  未カバー {2}（保留 {3}）' -f $r.Pct, $r.Path, ($r.Red.Count + $r.Held.Count), $r.Held.Count))
}
$report.Add('')
$report.Add('未カバーの行（保留を除く。/coverage が根拠を調べる対象）')
$any = $false
foreach ($r in $rows) {
    if ($r.Red.Count -eq 0) { continue }
    $any = $true
    $report.Add($r.Path)
    foreach ($x in $r.Red) { $report.Add(('  {0,5}: {1}' -f $x.Line, $x.Code)) }
}
if (-not $any) { $report.Add('  なし') }
$report.Add('')
$report.Add('保留の行（tests/coverage-hold.md）')
$any = $false
foreach ($r in $rows) {
    if ($r.Held.Count -eq 0) { continue }
    $any = $true
    $report.Add($r.Path)
    foreach ($x in $r.Held) { $report.Add(('  {0,5}: {1}　理由: {2}' -f $x.Line, $x.Code, $x.Reason)) }
}
if (-not $any) { $report.Add('  なし') }
$report.Add('')
$report.Add('詳しい表示: build/coverage/index.html')

$reportText = $report -join "`r`n"
[System.IO.File]::WriteAllText((Join-Path $covDir 'uncovered.txt'), $reportText + "`r`n", (New-Object System.Text.UTF8Encoding $true))
Use-Utf8Output
Write-Output $reportText
exit 0
