param([ValidateRange(1, 10)][int]$Rounds = 3)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$originalPath = $env:PATH
$originalLayers = $env:VK_LAYER_PATH
$failed = $false
Push-Location $projectRoot
try {
    $env:PATH = "$projectRoot\tools\Qt\6.10.3\msvc2022_64\bin;$originalPath"
    $env:VK_LAYER_PATH = "$projectRoot\tools\VulkanSDK\1.4.357.0\Bin"
    for ($round = 1; $round -le $Rounds; $round++) {
        foreach ($gpu in @('intel', 'nvidia')) {
            $arguments = @('--gpu-test')
            if ($gpu -eq 'intel') { $arguments += '--integrated' }
            # A real exposed window is required for swapchain tests.
            & "$projectRoot\build\Debug\Nothing3D.exe" @arguments | Out-Null
            $code = $LASTEXITCODE
            $report = Get-Content build/q3-gpu-test.txt -Raw
            Copy-Item build/q3-gpu-test.txt "build/q3-$gpu-$round.txt"
            Copy-Item build/q3-vulkan.log "build/q3-$gpu-$round.log"
            if ($code -ne 0 -or $report -notmatch 'Overall: PASS') { $failed = $true }
            Write-Output "Round $round / $gpu / exit=$code"
            Write-Output $report
        }
    }
} finally {
    $env:PATH = $originalPath
    $env:VK_LAYER_PATH = $originalLayers
    Pop-Location
}
if ($failed) { exit 1 }
exit 0
