param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("Build", "Upload")]
  [string]$Action
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$usedDrives = @(Get-PSDrive -PSProvider FileSystem | ForEach-Object { $_.Name })
$driveLetter = @("P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z") |
  Where-Object { $_ -notin $usedDrives } |
  Select-Object -First 1

if (-not $driveLetter) {
  throw "No unused drive letter from P: through Z: is available for the short-path build."
}

if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  throw "PlatformIO CLI 'pio' is not available in this VS Code terminal. Activate the PlatformIO environment or install PlatformIO Core."
}

$drive = "${driveLetter}:"
& subst $drive $repoRoot
if ($LASTEXITCODE -ne 0) {
  throw "Could not map $drive to the repository path."
}

$exitCode = 0
try {
  $projectPath = "${drive}\stm32-bluepill-bringup"
  $pioArgs = @("run", "-d", $projectPath, "-e", "bluepill_f103c8_bench", "-j", "1")
  if ($Action -eq "Upload") {
    $pioArgs += @("-t", "upload")
  }

  Write-Host "Issue #100: $Action full-bench F103 profile"
  Write-Host "Temporary short path: $projectPath"
  & pio @pioArgs
  $exitCode = $LASTEXITCODE
}
finally {
  & subst $drive /d
  if ($LASTEXITCODE -ne 0) {
    Write-Warning "Could not remove temporary $drive mapping."
  }
}

exit $exitCode
