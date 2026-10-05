param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("hat", "crowpanel", "legacy-esp32", "legacy-esp32-led")]
  [string]$Target,

  [Parameter(Mandatory = $true)]
  [ValidateSet("build", "upload")]
  [string]$Action,

  [switch]$Force,

  # Skip the STM32 quiet lease (manual fallback: power off / isolate the Blue Pill first).
  [switch]$NoQuiet,

  # Override the STM32 console port that receives the QUIET lease.
  [string]$QuietPort = ""
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
        QuietPort     = ""
      }
    }
    "crowpanel" {
      return [pscustomobject]@{
        DisplayName   = "CrowPanel ESP32-S3"
        EnvName       = "crowpanel43"
        WorkingDir    = "crowpanel-43-bringup"
        ExpectedPort  = "COM12"
        ExpectedChip  = "ESP32-S3"
        # STM32 USART1 console (HAT CH340, DTR not connected); must answer 'ACK QUIET ON'.
        QuietPort     = if ($QuietPort) { $QuietPort } else { "COM7" }
      }
    }
    "legacy-esp32" {
      return [pscustomobject]@{
        DisplayName   = "Legacy ESP32 Dev"
        EnvName       = "esp32dev"
        WorkingDir    = "."
        ExpectedPort  = "COM5"
        ExpectedChip  = "ESP32"
        QuietPort     = ""
      }
    }
    "legacy-esp32-led" {
      return [pscustomobject]@{
        DisplayName   = "Legacy ESP32 Dev (WS281x)"
        EnvName       = "esp32dev_ws281x"
        WorkingDir    = "."
        ExpectedPort  = "COM5"
        ExpectedChip  = "ESP32"
        QuietPort     = ""
      }
    }
    default {
      throw "Unknown target: $Name"
    }
  }
}

$script:Quiet = $null  # active quiet session: Port, PowerShell, Handle, State, LeaseSeconds
$script:QuietLeaseSeconds = 15
$script:QuietRenewMs = 3000

function Write-PrecheckFailure {
  param([string]$Message, [int]$Code = 2)
  Stop-QuietSession
  Write-Host ""
  Write-Host "FLASH PRECHECK FAILED" -ForegroundColor Red
  Write-Host $Message -ForegroundColor Yellow
  Write-Host "No flash was performed." -ForegroundColor Yellow
  exit $Code
}

function Invoke-QuietExchange {
  param($Port, [string]$Command, [string]$Pattern, [int]$TimeoutMs = 2500)

  $Port.DiscardInBuffer()
  $Port.Write("$Command`n")
  $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
  $buffer = ""
  while ([DateTime]::UtcNow -lt $deadline) {
    $buffer += $Port.ReadExisting()
    $m = [regex]::Match($buffer, $Pattern)
    if ($m.Success) { return $m }
    Start-Sleep -Milliseconds 50
  }
  return $null
}

# Keepalive runs on its own runspace so it keeps renewing while esptool/PlatformIO block the main thread.
$script:QuietKeepalive = {
  param($sp, $state, $lease, $renewMs)
  $clock = [System.Diagnostics.Stopwatch]::StartNew()
  $lastOkMs = 0
  while (-not $state.Stop) {
    if (($clock.ElapsedMilliseconds - $lastOkMs) -gt ($lease * 1000)) {
      $state.Failed = "quiet lease may have lapsed (no acknowledged renewal for more than $lease s)"
    }
    if (-not $state.Failed) {
      $up = -1
      try {
        $sp.DiscardInBuffer()
        $sp.Write("QUIET $lease`n")
        $deadline = [DateTime]::UtcNow.AddMilliseconds(1500)
        $buf = ""
        while ($up -lt 0 -and [DateTime]::UtcNow -lt $deadline) {
          $buf += $sp.ReadExisting()
          $m = [regex]::Match($buf, 'ACK QUIET ON rem=(\d+) up=(\d+)')
          if ($m.Success) { $up = [int64]$m.Groups[2].Value } else { Start-Sleep -Milliseconds 50 }
        }
      } catch { $up = -1 }

      if ($up -ge 0) {
        if ($up -lt $state.LastUp) {
          $state.Failed = "STM32 reset during the upload (uptime went backwards); UDI telemetry may have resumed"
        } else {
          $state.LastUp = $up
          $state.Misses = 0
          $state.Renewals++
          $lastOkMs = $clock.ElapsedMilliseconds
        }
      } else {
        $state.Misses++
        if ($state.Misses -ge 2) { $state.Failed = "lost the STM32 quiet acknowledgement on the console port (2 missed renewals)" }
      }
    }

    if ($state.Failed) {
      # The link can no longer be proven quiet: stop any uploader this script started.
      Get-CimInstance Win32_Process -Filter "ParentProcessId=$($state.Parent)" -ErrorAction SilentlyContinue |
        ForEach-Object { & taskkill /T /F /PID $_.ProcessId | Out-Null }
      break
    }
    for ($i = 0; $i -lt ($renewMs / 100) -and -not $state.Stop; $i++) { Start-Sleep -Milliseconds 100 }
  }
}

function Start-QuietSession {
  param([string]$PortName, [string]$ExpectedPort, [string[]]$DetectedPorts)

  if ($PortName -eq $ExpectedPort) {
    Write-PrecheckFailure "Quiet port $PortName must not be the programming port $ExpectedPort."
  }
  if ($DetectedPorts -notcontains $PortName) {
    $seen = if (@($DetectedPorts).Count -gt 0) { $DetectedPorts -join ", " } else { "none" }
    Write-PrecheckFailure ("STM32 console port $PortName is not present (detected: $seen), so the Blue Pill cannot be silenced.`n" +
      "Check the Blue Pill is powered and its USART1 CH340 is connected. If the Blue Pill is intentionally powered off or isolated, re-run with -NoQuiet.")
  }

  $sp = New-Object System.IO.Ports.SerialPort $PortName, 115200, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
  $sp.DtrEnable = $false
  $sp.RtsEnable = $false
  $sp.ReadTimeout = 200
  $sp.WriteTimeout = 500
  try {
    $sp.Open()
  } catch {
    $sp.Dispose()
    Write-PrecheckFailure "Could not open STM32 console port ${PortName}: $($_.Exception.Message)`nClose any serial monitor using it, or re-run with -NoQuiet after powering off the Blue Pill."
  }

  $ack = $null
  for ($attempt = 1; $attempt -le 3 -and $null -eq $ack; $attempt++) {
    $ack = Invoke-QuietExchange -Port $sp -Command "QUIET $($script:QuietLeaseSeconds)" -Pattern 'ACK QUIET ON rem=(\d+) up=(\d+)'
  }
  if ($null -eq $ack) {
    $sp.Close(); $sp.Dispose()
    Write-PrecheckFailure ("No 'ACK QUIET ON' from $PortName after 3 attempts; the Blue Pill is not confirmed quiet.`n" +
      "Likely causes: this port is not the STM32 console, or the STM32 firmware predates the QUIET command (flash it first with ST-Link). " +
      "If the Blue Pill is powered off or isolated, re-run with -NoQuiet.")
  }

  $state = [hashtable]::Synchronized(@{
    Stop = $false; Failed = ""; LastUp = [int64]$ack.Groups[2].Value; Misses = 0; Renewals = 0; Parent = $PID
  })
  $ps = [powershell]::Create()
  [void]$ps.AddScript($script:QuietKeepalive).AddArgument($sp).AddArgument($state).AddArgument($script:QuietLeaseSeconds).AddArgument($script:QuietRenewMs)
  $script:Quiet = [pscustomobject]@{ Port = $sp; PortName = $PortName; PowerShell = $ps; Handle = $ps.BeginInvoke(); State = $state }
  Write-Host "Quiet: STM32 acknowledged on $PortName (lease $($script:QuietLeaseSeconds) s, renewed every $($script:QuietRenewMs / 1000) s)" -ForegroundColor Green
}

function Stop-QuietSession {
  $q = $script:Quiet
  if ($null -eq $q) { return }
  $script:Quiet = $null

  $q.State.Stop = $true
  [void]$q.Handle.AsyncWaitHandle.WaitOne(10000)
  $q.PowerShell.Dispose()

  # Never resume telemetry while an uploader started by this script is still alive.
  Get-CimInstance Win32_Process -Filter "ParentProcessId=$PID" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^(platformio|esptool|python)' } |
    ForEach-Object { & taskkill /T /F /PID $_.ProcessId | Out-Null }

  $off = $null
  try {
    $off = Invoke-QuietExchange -Port $q.Port -Command "QUIET OFF" -Pattern 'ACK QUIET OFF up=(\d+)'
  } catch { $off = $null }
  if ($null -ne $off) {
    Write-Host "Quiet: STM32 UDI telemetry resumed (acknowledged)." -ForegroundColor Green
  } else {
    Write-Host "Quiet: no resume acknowledgement; the STM32 lease expires on its own within $($script:QuietLeaseSeconds) s. Power-cycle the Blue Pill if telemetry does not return." -ForegroundColor Yellow
  }
  if ($q.State.Failed) {
    Write-Host "QUIET FAILURE: $($q.State.Failed). The upload was stopped; the CrowPanel may be left in download mode or with a partial image. Re-run the upload." -ForegroundColor Red
  }
  try { $q.Port.Close(); $q.Port.Dispose() } catch {}
  if ($q.State.Failed) { $script:QuietFailed = $true }
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

  # The preceding chip-id probe resets the board; the first read-mac can fail while it re-enumerates.
  $macPattern = 'MAC:\s+([0-9A-Fa-f:]{17,23})'
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    Start-Sleep -Seconds 2
    $probe = Invoke-Esptool -Port $Port -CommandName "read-mac"
    if ($probe.ExitCode -ne 0) { continue }
    $macMatch = [regex]::Match($probe.Text, $macPattern)
    if ($macMatch.Success) {
      return $macMatch.Groups[1].Value.Trim().ToUpperInvariant()
    }
  }

  return ""
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
  param([string]$ExpectedChip, [string]$OnlyPort = "")

  # OnlyPort: the MAC lock on the expected port identifies the target, so unrelated ports are not probed.
  $serialPorts = if ($OnlyPort) { @($OnlyPort) } else { Get-DetectedPorts }

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
$script:QuietFailed = $false
$useQuiet = ($Action -eq "upload") -and (-not [string]::IsNullOrWhiteSpace($cfg.QuietPort)) -and (-not $NoQuiet)

if (!(Test-Path $pioExe)) {
  Write-PrecheckFailure "PlatformIO executable not found at $pioExe"
}

$exitCode = 1
try {
  if ($Action -eq "upload") {
    $ports = @(Get-DetectedPorts)
    if (-not $Force -and $ports -notcontains $cfg.ExpectedPort) {
      $seen = if (@($ports).Count -gt 0) { $ports -join ", " } else { "none" }
      Write-PrecheckFailure "Expected $($cfg.DisplayName) on $($cfg.ExpectedPort), but detected ports: $seen"
    }

    if ($useQuiet) {
      # Quiet must be acknowledged before any esptool detection, probe, or reset.
      Start-QuietSession -PortName $cfg.QuietPort -ExpectedPort $cfg.ExpectedPort -DetectedPorts $ports
    } elseif (-not [string]::IsNullOrWhiteSpace($cfg.QuietPort)) {
      Write-Host "WARNING: -NoQuiet set. The Blue Pill must be powered off or its UART isolated, or the upload may fail." -ForegroundColor Yellow
    }
  }

  if ($Action -eq "upload" -and -not $Force) {
    $onlyPort = if ($useQuiet -and ![string]::IsNullOrWhiteSpace($expectedMac)) { $cfg.ExpectedPort } else { "" }
    $matchingPorts = @(Get-MatchingChipPorts -ExpectedChip $cfg.ExpectedChip -OnlyPort $onlyPort)
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
  $exitCode = $LASTEXITCODE
}
finally {
  # Uploader has exited (or been killed) here; only then is telemetry resumed.
  Stop-QuietSession
}

if ($script:QuietFailed -and $exitCode -eq 0) { $exitCode = 3 }
exit $exitCode
