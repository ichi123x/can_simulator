# VS Code のビルドタスクから呼ぶ。インストールされている Visual Studio の MSBuild を vswhere で探して
# ソリューションをビルドする（PC ごとに Visual Studio の版・場所が違っても動くように）
param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64'
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe が見つかりません: $vswhere"
    exit 1
}

$msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
    Select-Object -First 1
if (-not $msbuild) {
    Write-Error 'MSBuild.exe が見つかりません（Visual Studio の C++ のワークロードを入れてください）'
    exit 1
}

$sln = Join-Path $PSScriptRoot '..\MFCApplication1\MFCApplication1.sln'
& $msbuild $sln "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m /nologo /v:minimal
exit $LASTEXITCODE
