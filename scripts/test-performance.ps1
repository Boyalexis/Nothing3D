param([switch]$Validation, [ValidateSet('baseline','optimized')][string]$Label='optimized')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$originalPath=$env:PATH
$originalLayers=$env:VK_LAYER_PATH
$failed=$false
Push-Location $projectRoot
try {
    $env:PATH="$projectRoot\tools\Qt\6.10.3\msvc2022_64\bin;$originalPath"
    $env:VK_LAYER_PATH="$projectRoot\tools\VulkanSDK\1.4.357.0\Bin"
    foreach($gpu in @('intel','nvidia')) {
        $arguments=@('--performance-test')
        if($gpu -eq 'intel') { $arguments+='--integrated' }
        if($Validation) { $arguments+='--performance-validation' }
        & "$projectRoot\build\Release\Nothing3D.exe" @arguments | Out-Null
        $code=$LASTEXITCODE
        if($code -ne 0) { $failed=$true; Write-Output "$gpu failed: exit=$code"; continue }
        $report=Get-Content "build/performance-$gpu.json" -Raw | ConvertFrom-Json
        if(!$report.completed -or $report.validation_errors_including_shutdown -ne 0) { $failed=$true }
        if($Validation -and !$report.validation_enabled) { $failed=$true }
        if(!$Validation -and @($report.samples | Where-Object { !$_.target_met }).Count -gt 0) { $failed=$true }
        $suffix=if($Validation) { 'validation' } else { $Label }
        Copy-Item "build/performance-$gpu.json" "build/performance-$gpu-$suffix.json"
        Write-Output ($report | ConvertTo-Json -Depth 6)
    }
} finally {
    $env:PATH=$originalPath
    $env:VK_LAYER_PATH=$originalLayers
    Pop-Location
}
if($failed) { exit 1 }
