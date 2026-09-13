param([string]$Archive)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$Archive) { $Archive=(Get-Content "$projectRoot/build/latest-package.txt" -Raw).Trim()+'.zip' }
$testRoot=Join-Path $projectRoot ('out/package-check-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
Expand-Archive -LiteralPath $Archive -DestinationPath $testRoot
$manifest=Get-Content "$testRoot/manifest.json" -Raw | ConvertFrom-Json
foreach($entry in $manifest) {
    if((Get-FileHash -LiteralPath (Join-Path $testRoot $entry.path) -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Hash mismatch: $($entry.path)" }
}
$names=@('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','VK_LAYER_PATH','QT_QPA_PLATFORM')
$saved=@{}
foreach($name in $names) { $saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
$results=@()
Push-Location $testRoot
try {
    $env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
    foreach($name in $names | Where-Object { $_ -ne 'PATH' }) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    New-Item -ItemType Directory -Path "$testRoot/build" -Force | Out-Null
    foreach($test in @('--smoke-test','--dimension-test','--transform-test','--history-test','--object-actions-test','--project-test')) {
        & "$testRoot/Nothing3D.exe" $test | Out-Null
        $code=$LASTEXITCODE
        $results+=[pscustomobject]@{test=$test;exit=$code}
        if($code -ne 0) { throw "Packaged test failed: $test ($code)" }
    }
    foreach($gpu in @('intel','nvidia')) {
        $arguments=@('--ui-preview')
        if($gpu -eq 'intel') { $arguments+='--integrated' }
        & "$testRoot/Nothing3D.exe" @arguments | Out-Null
        $code=$LASTEXITCODE
        if($code -ne 0 -or !(Test-Path 'build/ui-preview.png')) { throw "Packaged GPU preview failed: $gpu" }
        Move-Item -LiteralPath 'build/ui-preview.png' -Destination "build/package-$gpu.png"
        $results+=[pscustomobject]@{test="GPU preview $gpu";exit=$code}
    }
    $results | ConvertTo-Json | Set-Content -LiteralPath "$projectRoot/build/package-check.json" -Encoding utf8
    Write-Output "PASS: extracted ZIP hashes, six functional checks, two native GPU previews"
    Write-Output "Evidence: $testRoot/build"
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Pop-Location
}
