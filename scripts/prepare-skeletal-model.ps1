$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$modelPath = Join-Path $projectRoot 'examples/skeletal_animation/assets/RiggedFigure.glb'
$expected = 'D6BE85417D3E256861EE733EEA6916093A7AF7C79C16366181FD8ABCAEB38CF5'
# 已存在时先验证，不覆盖本地修改；首次下载也固定revision并验证摘要。
if (-not (Test-Path -LiteralPath $modelPath)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/edc7c9e67c639d230715049ee31f9a96a6babbbe/Models/RiggedFigure/glTF-Binary/RiggedFigure.glb' -OutFile $modelPath
}
if ((Get-FileHash -LiteralPath $modelPath -Algorithm SHA256).Hash -ne $expected) {
    throw 'RiggedFigure SHA256 mismatch. File retained for inspection; no automatic overwrite.'
}
Write-Output "Verified $modelPath"
