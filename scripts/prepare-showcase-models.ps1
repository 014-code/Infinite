# 从固定版本下载CC0模型，并生成当前基础颜色预览支持的GLB副本。
# 这是显式运行的离线资源准备工具；构建和示例运行都不访问网络。
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ProgressPreference = 'SilentlyContinue'
$projectRoot = Split-Path -Parent $PSScriptRoot
$revision = 'edc7c9e67c639d230715049ee31f9a96a6babbbe'
$baseUrl = "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/$revision"
$cache = Join-Path $projectRoot 'build-model-downloads'
$assetRoot = Join-Path $projectRoot 'examples/material_showcase/assets/models'
New-Item -ItemType Directory -Path $cache -Force | Out-Null

foreach ($name in @('Avocado', 'BarramundiFish', 'Lantern')) {
    $url = "$baseUrl/Models/$name/glTF-Binary/$name.glb"
    $original = Join-Path $cache "$name.glb"
    $directory = Join-Path $assetRoot $name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Invoke-WebRequest $url -OutFile $original
    Invoke-WebRequest "$baseUrl/Models/$name/README.md" -OutFile (Join-Path $directory 'UPSTREAM.md')

    # 仅重写GLB的JSON块，二进制块逐字节保留。未使用的切线accessor也保留，
    # 因而原来的索引、顶点、图像偏移都不变，不发生重建模型或重新压缩贴图。
    $bytes = [IO.File]::ReadAllBytes($original)
    if ($bytes.Length -lt 20 -or [BitConverter]::ToUInt32($bytes, 0) -ne 0x46546C67 -or
        [BitConverter]::ToUInt32($bytes, 4) -ne 2 -or
        [BitConverter]::ToUInt32($bytes, 8) -ne $bytes.Length -or
        [BitConverter]::ToUInt32($bytes, 16) -ne 0x4E4F534A) { throw "Invalid GLB: $name" }
    $jsonLength = [BitConverter]::ToUInt32($bytes, 12)
    if ($jsonLength -gt $bytes.Length - 20) { throw "Invalid JSON length: $name" }
    $document = [Text.Encoding]::UTF8.GetString($bytes, 20, $jsonLength) | ConvertFrom-Json
    $removed = 0
    foreach ($mesh in $document.meshes) {
        foreach ($primitive in $mesh.primitives) {
            if ($primitive.attributes.PSObject.Properties['TANGENT']) {
                $primitive.attributes.PSObject.Properties.Remove('TANGENT')
                $removed++
            }
        }
    }
    $json = [Text.Encoding]::UTF8.GetBytes(($document | ConvertTo-Json -Depth 100 -Compress))
    $padding = (4 - $json.Length % 4) % 4
    $tailOffset = 20 + $jsonLength
    $totalLength = 20 + $json.Length + $padding + $bytes.Length - $tailOffset
    $destination = Join-Path $directory 'preview.glb'
    $stream = [IO.File]::Create($destination)
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([uint32]0x46546C67)
        $writer.Write([uint32]2)
        $writer.Write([uint32]$totalLength)
        $writer.Write([uint32]($json.Length + $padding))
        $writer.Write([uint32]0x4E4F534A)
        $writer.Write($json)
        for ($i = 0; $i -lt $padding; $i++) { $writer.Write([byte]32) }
        $writer.Write($bytes, [int]$tailOffset, [int]($bytes.Length - $tailOffset))
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
    # 原版留在构建缓存，分发包包含预览版、原作者说明与可重复下载的固定提交链接。
    $receipt = [ordered]@{
        model = $name
        repository = 'KhronosGroup/glTF-Sample-Assets'
        revision = $revision
        sourceUrl = $url
        license = 'CC0-1.0'
        licenseUrl = 'https://creativecommons.org/publicdomain/zero/1.0/'
        originalSha256 = (Get-FileHash $original -Algorithm SHA256).Hash
        previewSha256 = (Get-FileHash $destination -Algorithm SHA256).Hash
        modification = "Removed $removed TANGENT attribute references; binary chunk unchanged. Preview uses base color and diffuse lighting only."
    }
    $receipt | ConvertTo-Json | Set-Content (Join-Path $directory 'SOURCE.json') -Encoding utf8
    Write-Host "$name prepared: $($bytes.Length) bytes, $removed tangent references removed"
}
