param(
    [string]$Toolchain = 'E:/msys64/ucrt64',
    [switch]$SkipBuild
)

# 一键本机验收。每次使用新的构建子目录，保留日志与失败副本，不删除用户文件。
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$exampleManifest = Join-Path $projectRoot 'cmake/EngineExamples.cmake'
$buildRoot = Join-Path $projectRoot 'build-ucrt64-release'
$cmake = Join-Path $Toolchain 'bin/cmake.exe'
$ctest = Join-Path $Toolchain 'bin/ctest.exe'
$runRoot = Join-Path $buildRoot ('acceptance/交付 测试-' + [guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path $runRoot 'package'
$workingRoot = Join-Path $runRoot '工作目录 与exe不同'
$oldPath = $env:PATH
$results = [System.Collections.Generic.List[object]]::new()
New-Item -ItemType Directory -Path $workingRoot -Force | Out-Null

function Read-ExampleManifest {
    if (!(Test-Path -LiteralPath $exampleManifest -PathType Leaf)) {
        throw "Example manifest was not found: $exampleManifest"
    }

    # 清单采用“一行一个CMake列表项”，脚本只接受小写字母、数字和下划线，
    # 这样注释或格式错误不会被误当成示例目录继续执行。
    $entries = @(Get-Content -LiteralPath $exampleManifest | ForEach-Object {
        $entry = $_.Trim()
        if ($entry -match '^[a-z0-9_]+$') {
            $entry
        }
    })
    if ($entries.Count -eq 0) {
        throw "Example manifest is empty: $exampleManifest"
    }
    $duplicates = @($entries | Group-Object | Where-Object Count -gt 1)
    if ($duplicates.Count -gt 0) {
        throw "Example manifest contains duplicate entries: $($duplicates.Name -join ', ')"
    }
    return $entries
}

function Invoke-NativeChecked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Native command failed ($LASTEXITCODE): $Program $Arguments"
    }
}

function Invoke-PackagedExample {
    param([string]$Executable, [string]$Case, [string]$ExpectedError = '', [string]$Argument = '--smoke-test')
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.Arguments = $Argument
    $start.WorkingDirectory = $workingRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $start.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    # 只改变子进程的环境，不修改系统PATH；运行库必须由安装目录提供。
    $start.EnvironmentVariables['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (!$process.Start()) { throw "Unable to start $Case" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(20000)) {
            # 只终止本函数启动的测试进程，不枚举或关闭用户已有应用。
            $process.Kill()
            $process.WaitForExit()
            throw "Packaged example timed out: $Case"
        }
        $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        $output | Set-Content -LiteralPath (Join-Path $runRoot "$Case.txt") -Encoding UTF8
        if ($ExpectedError) {
            if ($process.ExitCode -ne 1 -or !$output.Contains($ExpectedError) -or !$output.Contains('[ERROR]')) {
                throw "Failure case did not report expected error: $Case (exit $($process.ExitCode))`n$output"
            }
        } elseif ($process.ExitCode -ne 0 -or $output.Contains('[ERROR]') -or !$output.Contains('example stopped')) {
            throw "Packaged example failed: $Case (exit $($process.ExitCode))`n$output"
        }
        $results.Add([ordered]@{ case = $Case; exitCode = $process.ExitCode; expectedError = $ExpectedError; passed = $true })
        Write-Host "PASS $Case (exit $($process.ExitCode))"
    } finally {
        $process.Dispose()
    }
}

Push-Location $projectRoot
try {
    $env:PATH = (Join-Path $Toolchain 'bin') + ';' + $oldPath
    if (!$SkipBuild) {
        Invoke-NativeChecked $cmake @('--preset', 'msys2-ucrt64-release',
            "-DCMAKE_CXX_COMPILER=$Toolchain/bin/g++.exe", "-DCMAKE_MAKE_PROGRAM=$Toolchain/bin/ninja.exe", "-DCMAKE_PREFIX_PATH=$Toolchain")
        Invoke-NativeChecked $cmake @('--build', '--preset', 'msys2-ucrt64-release')
    }
    Invoke-NativeChecked $ctest @('--preset', 'msys2-ucrt64-release')
    Invoke-NativeChecked $cmake @('--install', $buildRoot, '--prefix', $packageRoot)

    $examples = Read-ExampleManifest
    foreach ($name in $examples) {
        $exe = Join-Path $packageRoot "examples/$name/$name.exe"
        Invoke-PackagedExample $exe $name
        $log = Join-Path $packageRoot "examples/$name/logs/$name.log"
        if (!(Test-Path -LiteralPath $log) -or !(Get-Content -LiteralPath $log -Raw).Contains('example stopped')) {
            throw "Example did not write its executable-relative log: $name"
        }
    }

    # 故障只注入新建副本，不触碰源码资源和正常安装包。
    # 故障用例只复制正常纹理示例目录的副本，不修改安装目录或源码资源。
    # 先建立副本目录，再复制示例目录内容；Copy-Item会把assets、shaders和logs
    # 放到这个目标目录下，保持exe旁边的原有相对布局。
    # 使用明确的组合路径，确保故障副本始终包含与正式包一致的示例目录层级。
    New-Item -ItemType Directory -Path ([System.IO.Path]::Combine($runRoot, 'missing-shader-fixture', 'textured_quad')) -Force | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $packageRoot 'examples/textured_quad') | `
        Copy-Item -Destination ([System.IO.Path]::Combine($runRoot, 'missing-shader-fixture', 'textured_quad')) -Recurse -Force -ErrorAction Stop
    $shaderPath = (Resolve-Path -LiteralPath ([System.IO.Path]::Combine($runRoot, 'missing-shader-fixture', 'textured_quad', 'shaders', 'texture.vert'))).Path
    if (!$shaderPath.StartsWith([System.IO.Path]::GetFullPath([System.IO.Path]::Combine($runRoot, 'missing-shader-fixture', 'textured_quad')) + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe fixture path' }
    # 先保存副本，再只删除本次脚本创建的故障目录中的Shader，保证源码和正常安装包不变。
    Copy-Item -LiteralPath $shaderPath -Destination ($shaderPath + '.saved') -Force
    Remove-Item -LiteralPath $shaderPath -Force
    if (Test-Path -LiteralPath $shaderPath) { throw 'Failed to remove isolated missing-shader fixture' }
    Invoke-PackagedExample ([System.IO.Path]::Combine($runRoot, 'missing-shader-fixture', 'textured_quad', 'textured_quad.exe')) 'missing-shader' 'texture.vert'

    New-Item -ItemType Directory -Path ([System.IO.Path]::Combine($runRoot, 'corrupt-image-fixture', 'textured_quad')) -Force | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $packageRoot 'examples/textured_quad') | `
        Copy-Item -Destination ([System.IO.Path]::Combine($runRoot, 'corrupt-image-fixture', 'textured_quad')) -Recurse -Force -ErrorAction Stop
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tests/fixtures/images/corrupt.ppm') `
        -Destination ([System.IO.Path]::Combine($runRoot, 'corrupt-image-fixture', 'textured_quad', 'assets', 'checker.ppm')) -Force
    Invoke-PackagedExample ([System.IO.Path]::Combine($runRoot, 'corrupt-image-fixture', 'textured_quad', 'textured_quad.exe')) 'corrupt-image' "Unexpected end of P3 image"
    Invoke-PackagedExample (Join-Path $packageRoot 'examples/application_template/application_template.exe') `
        'invalid-argument' 'Usage: example' '--not-an-option'

    # 复制已经验收的安装目录，避免再次安装引入差异；随后逐文件校验SHA256。
    # 示例产生的日志也会随副本保留，故障副本位于packageRoot之外，不进入交付包。
    $deliveryRoot = Join-Path $runRoot 'Infinite-Windows-Release'
    Copy-Item -LiteralPath $packageRoot -Destination $deliveryRoot -Recurse -Force
    if (!(Test-Path -LiteralPath $deliveryRoot -PathType Container)) {
        throw "Failed to create final delivery directory: $deliveryRoot"
    }
    $hashes = @(Get-ChildItem -LiteralPath $deliveryRoot -File -Recurse | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($deliveryRoot.Length + 1)
        $verifiedPath = Join-Path $packageRoot $relative
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        if ($hash -ne (Get-FileHash -LiteralPath $verifiedPath -Algorithm SHA256).Hash) {
            throw "Delivery differs from verified package: $relative"
        }
        [ordered]@{ path = $relative; sha256 = $hash }
    })
    $hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runRoot 'sha256.json') -Encoding UTF8
    $receipt = [ordered]@{
        status = 'passed'; configuration = 'Release'; verifiedDirectory = $packageRoot
        workingDirectory = $workingRoot; sanitizedChildPath = "$env:SystemRoot\System32;$env:SystemRoot"
        compiler = (& (Join-Path $Toolchain 'bin/g++.exe') --version | Select-Object -First 1)
        checks = $results.ToArray(); fileCount = $hashes.Count
        limitations = 'Same Windows machine and driver; not a clean-machine or manual-UI certification.'
    }
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $runRoot 'acceptance.json') -Encoding UTF8
    $archive = Join-Path $runRoot 'Infinite-Windows-Release.zip'
    Compress-Archive -LiteralPath (Join-Path $runRoot 'Infinite-Windows-Release') -DestinationPath $archive
    Write-Host "Verified archive: $archive"
    Write-Host "Receipt: $(Join-Path $runRoot 'acceptance.json')"
} finally {
    $env:PATH = $oldPath
    Pop-Location
}
