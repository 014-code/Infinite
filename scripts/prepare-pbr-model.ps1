# 固定上游版本和SHA256，保留完整切线/材质，不改写GLB。仅显式执行时访问网络。
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$projectRoot = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $projectRoot 'examples/gltf_pbr/assets'
$source = Join-Path $projectRoot 'build-model-downloads/Avocado.glb'
$expected = 'CCC9C3CE56423720B09399C2351537207CD5A65F859F9E6E2F30922762F3ABD4'
if (!(Test-Path -LiteralPath $source)) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $source) -Force | Out-Null
    Invoke-WebRequest 'https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/edc7c9e67c639d230715049ee31f9a96a6babbbe/Models/Avocado/glTF-Binary/Avocado.glb' -OutFile $source
}
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $expected) { throw 'Avocado source checksum mismatch' }
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $source -Destination (Join-Path $destination 'Avocado.glb')
Write-Host 'Original Avocado GLB verified and prepared.'
