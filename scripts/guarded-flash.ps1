param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("hat", "crowpanel", "legacy-esp32", "legacy-esp32-led")]
  [string]$Target,

  [Parameter(Mandatory = $true)]
  [ValidateSet("build", "upload")]
  [string]$Action,

  [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$script:EsptoolExe = "C:/Users/user/.platformio/penv/Scripts/esptool.exe"
$script:IdentityConfigPath = Join-Path $PSScriptRoot "flash-targets.json"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
  $PSNativeCommandUseErrorActionPreference = $false
}

function Get-TargetConfig {
  param([string]$Name)

  switch ($Name) {
    "hat" {
      return [pscustomobject]@{
        DisplayName   = "HAT ESP32-C6"
        EnvName       = "hat_c6_i2c_probe"
        WorkingDir    = "."
        ExpectedPort  = "COM11"
        ExpectedChip  = "ESP32-C6"
      }
    }
    "crowpanel" {
      return [pscustomobject]@{
        DisplayName   = "CrowPanel ESP32-S3"
        EnvName       = "crowpanel43"
        WorkingDir    = "crowpanel-43-bringup"
        ExpectedPort  = "COM12"
        ExpectedChip  = "ESP32-S3"
      }
    }
    "legacy-esp32" {
      return [pscustomobject]@{
        DisplayName   = "Legacy ESP32 Dev"
        EnvName       = "esp32dev"
        WorkingDir    = "."
        ExpectedPort  = "COM5"
        ExpectedChip  = "ESP32"
      }
    }
    "legacy-esp32-led" {
      return [pscustomobject]@{
        DisplayName   = "Legacy ESP32 Dev (WS281x)"
        EnvName       = "esp32dev_ws281x"
        WorkingDir    = "."
        ExpectedPort  = "COM5"
        ExpectedChip  = "ESP32"
      }
    }
    default {
      throw "Unknown target: $Name"
    }
  }
}

function Write-PrecheckFailure {
  param([string]$Message, [int]$Code = 2)
  Write-Host ""
  Write-Host "FLASH PRECHECK FAILED" -ForegroundColor Red
  Write-Host $Message -ForegroundColor Yellow
  Write-Host "No flash was performed." -ForegroundColor Yellow
  exit $Code
}

function Invoke-Esptool {
  param(
    [string]$Port,
    [string]$CommandName
  )

  $stdoutFile = [System.IO.Path]::GetTempFileName()
  $stderrFile = [System.IO.Path]::GetTempFileName()
  try {
    $proc = Start-Process -FilePath $script:EsptoolExe `
                -ArgumentList @("--port", $Port, $CommandName) `
                -NoNewWindow -PassThru -Wait `
                -RedirectStandardOutput $stdoutFile `
                -RedirectStandardError $stderrFile
    $stdout = if (Test-Path $stdoutFile) { Get-Content $stdoutFile -Raw } else { "" }
    $stderr = if (Test-Path $stderrFile) { Get-Content $stderrFile -Raw } else { "" }
    return [pscustomobject]@{
      ExitCode = $proc.ExitCode
      Text     = ($stdout + "`n" + $stderr)
    }
  }
  finally {
    Remove-Item $stdoutFile -ErrorAction SilentlyContinue
    Remove-Item $stderrFile -ErrorAction SilentlyContinue
  }
}

function Get-IdentityConfig {
  if (!(Test-Path $script:IdentityConfigPath)) {
    return $null
  }

  return Get-Content $script:IdentityConfigPath -Raw | ConvertFrom-Json
}

function Get-ExpectedMac {
  param([string]$TargetName)

  $config = Get-IdentityConfig
  if ($null -eq $config) {
    return ""
  }

  $entryProp = $config.PSObject.Properties[$TargetName]
  if ($null -eq $entryProp) {
    return ""
  }

  $entry = $entryProp.Value
  if ($null -eq $entry) {
    return ""
  }

  return ([string]$entry.expectedMac).Trim().ToUpperInvariant()
}

function Get-DetectedPorts {
  $ports = New-Object System.Collections.ArrayList

  $pnpPorts = @(Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue)
  foreach ($p in $pnpPorts) {
    if ($null -eq $p.FriendlyName) { continue }
    $m = [regex]::Match([string]$p.FriendlyName, 'COM\d+')
    if ($m.Success) {
      [void]$ports.Add([string]$m.Value.ToUpperInvariant())
    }
  }

  return @($ports | Sort-Object -Unique)
}

function Get-PortMac {
  param([string]$Port)

  if (!(Test-Path $script:EsptoolExe)) {
    Write-PrecheckFailure "esptool executable not found at $($script:EsptoolExe)"
  }

  $probe = Invoke-Esptool -Port $Port -CommandName "read-mac"
  $probeText = $probe.Text
  if ($probe.ExitCode -ne 0) {
    return ""
  }

  $macMatch = [regex]::Match($probeText, 'MAC:\s+([0-9A-Fa-f:]{17,23})')
  if (!$macMatch.Success) {
    return ""
  }

  return $macMatch.Groups[1].Value.Trim().ToUpperInvariant()
}

function Invoke-ChipProbe {
  param(
    [string]$Port,
    [string]$ExpectedChip
  )

  if (!(Test-Path $script:EsptoolExe)) {
    Write-PrecheckFailure "esptool executable not found at $($script:EsptoolExe)"
  }

  $probe = Invoke-Esptool -Port $Port -CommandName "chip-id"
  $probeText = $probe.Text

  if ($probe.ExitCode -ne 0) {
    Write-PrecheckFailure "Chip probe failed on $Port. Output:`n$probeText"
  }

  if ($probeText -notmatch [regex]::Escape($ExpectedChip)) {
    Write-PrecheckFailure "Port $Port detected chip does not match expected '$ExpectedChip'. Output:`n$probeText"
  }

  Write-Host "Precheck: chip probe matched $ExpectedChip on $Port" -ForegroundColor Green
}

function Get-MatchingChipPorts {
  param([string]$ExpectedChip)

  $serialPorts = Get-DetectedPorts

  $portMatches = New-Object System.Collections.ArrayList

  foreach ($port in $serialPorts) {
    $probe = Invoke-Esptool -Port $port -CommandName "chip-id"
    $probeText = $probe.Text
    if ($probe.ExitCode -eq 0 -and $probeText -match [regex]::Escape($ExpectedChip)) {
      [void]$portMatches.Add([string]$port)
    }
  }

  return @($portMatches)
}

$cfg = Get-TargetConfig -Name $Target
$pioExe = "C:/Users/user/.platformio/penv/Scripts/platformio.exe"
$expectedMac = Get-ExpectedMac -TargetName $Target

if (!(Test-Path $pioExe)) {
  Write-PrecheckFailure "PlatformIO executable not found at $pioExe"
}

if ($Action -eq "upload" -and -not $Force) {
  $ports = @(Get-DetectedPorts)
  if ($ports -notcontains $cfg.ExpectedPort) {
    $seen = if (@($ports).Count -gt 0) { $ports -join ", " } else { "none" }
    Write-PrecheckFailure "Expected $($cfg.DisplayName) on $($cfg.ExpectedPort), but detected ports: $seen"
  }

  $matchingPorts = @(Get-MatchingChipPorts -ExpectedChip $cfg.ExpectedChip)
  if (@($matchingPorts).Count -gt 1) {
    Write-PrecheckFailure "Ambiguous target selection: multiple $($cfg.ExpectedChip) devices detected on $($matchingPorts -join ', '). Leave only the intended target connected, then try again."
  }
  if (@($matchingPorts).Count -eq 0) {
    Write-PrecheckFailure "Expected chip family $($cfg.ExpectedChip) was not detected on any connected serial port."
  }
  if ($matchingPorts[0] -ne $cfg.ExpectedPort) {
    Write-PrecheckFailure "Expected $($cfg.DisplayName) on $($cfg.ExpectedPort), but the only detected $($cfg.ExpectedChip) device is on $($matchingPorts[0])."
  }

  $detectedMac = Get-PortMac -Port $cfg.ExpectedPort
  if ([string]::IsNullOrWhiteSpace($detectedMac)) {
    Write-PrecheckFailure "Could not read immutable MAC from $($cfg.ExpectedPort)."
  }
  Write-Host "Precheck: detected MAC $detectedMac on $($cfg.ExpectedPort)" -ForegroundColor Green
  if (![string]::IsNullOrWhiteSpace($expectedMac) -and $detectedMac -ne $expectedMac) {
    Write-PrecheckFailure "MAC mismatch on $($cfg.ExpectedPort). Expected $expectedMac but detected $detectedMac."
  }

  Write-Host "Precheck: expected port $($cfg.ExpectedPort) is present" -ForegroundColor Green
  Invoke-ChipProbe -Port $cfg.ExpectedPort -ExpectedChip $cfg.ExpectedChip
}

$pioCommand = New-Object System.Collections.ArrayList
[void]$pioCommand.Add("run")
if ($cfg.WorkingDir -ne ".") {
  [void]$pioCommand.Add("-d")
  [void]$pioCommand.Add($cfg.WorkingDir)
}
[void]$pioCommand.Add("-e")
[void]$pioCommand.Add($cfg.EnvName)

if ($Action -eq "upload") {
  [void]$pioCommand.Add("-t")
  [void]$pioCommand.Add("upload")
  [void]$pioCommand.Add("--upload-port")
  [void]$pioCommand.Add($cfg.ExpectedPort)
}

Write-Host "Running: platformio $($pioCommand -join ' ')" -ForegroundColor Cyan
& $pioExe $pioCommand.ToArray()
exit $LASTEXITCODE
