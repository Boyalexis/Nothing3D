param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$originalPath=$env:PATH
Push-Location $projectRoot
try {
    if(!$SkipBuild) {
        & "$PSScriptRoot/build-release.bat"
        if($LASTEXITCODE -ne 0) { throw 'Release build failed' }
    }
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
    $package=Join-Path $projectRoot "out/Nothing3D-0.1.0-win64-$stamp"
    New-Item -ItemType Directory -Path $package | Out-Null
    Copy-Item -LiteralPath "$projectRoot/build/Release/Nothing3D.exe" -Destination $package
    $qtBin=Join-Path $projectRoot 'tools/Qt/6.10.3/msvc2022_64/bin'
    $env:PATH="$qtBin;$originalPath"
    & "$qtBin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime "$package/Nothing3D.exe"
    if($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
    $runtime='D:/DevTools/VSBuildTools/2022/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT'
    Copy-Item -Path "$runtime/*.dll" -Destination $package
    New-Item -ItemType Directory -Path "$package/examples","$package/docs","$package/build" | Out-Null
    Copy-Item -LiteralPath "$projectRoot/examples/getting-started.n3d" -Destination "$package/examples"
    if(Test-Path "$projectRoot/build/performance-1000.n3d") { Copy-Item -LiteralPath "$projectRoot/build/performance-1000.n3d" -Destination "$package/examples" }
    Copy-Item -Path "$projectRoot/docs/*-learning.md" -Destination "$package/docs"
    Copy-Item -LiteralPath "$projectRoot/docs/First-stage-run-package.md" -Destination "$package/开始使用.md"
    Set-Content -LiteralPath "$package/qt.conf" -Value "[Paths]`nPlugins=." -Encoding utf8
    $hashes=Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
        [pscustomobject]@{path=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath "$package/manifest.json" -Encoding utf8
    Compress-Archive -Path "$package/*" -DestinationPath "$package.zip"
    Set-Content -LiteralPath "$projectRoot/build/latest-package.txt" -Value $package -Encoding utf8
    Write-Output "Package: $package"
    Write-Output "Archive: $package.zip"
} finally { $env:PATH=$originalPath; Pop-Location }
