[CmdletBinding()]
param(
	[string]$ComDebug = "COM7",
	[string]$ComEvt = "COM12",
	[int]$Baud = 115200,
	[int]$DurationSec = 600,
	[string]$OutDir = "",
	[switch]$SendBaseline
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-SerialPort {
	param(
		[string]$PortName,
		[int]$BaudRate
	)

	$port = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, "None", 8, "One"
	$port.NewLine = "`r`n"
	$port.ReadTimeout = 120
	$port.WriteTimeout = 500
	return $port
}

function Drain-Port {
	param(
		[System.IO.Ports.SerialPort]$Port,
		[string]$LogPath,
		[datetime]$Until,
		[switch]$Timestamp
	)

	while ((Get-Date) -lt $Until) {
		try {
			$line = $Port.ReadLine()
			if ($line) {
				if ($Timestamp) {
					Add-Content -Path $LogPath -Value ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $line)
				} else {
					Add-Content -Path $LogPath -Value $line
				}
			}
		} catch [System.TimeoutException] {
			# No line available in this interval.
		}
	}
}

$workspace = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutDir)) {
	$stamp = Get-Date -Format "yyyy-MM-dd/HHmmss"
	$OutDir = Join-Path $workspace ("docs/firmware-buckets/artifacts/bucket-3/{0}" -f $stamp)
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$debugLog = Join-Path $OutDir ("b3-com7-live-{0}.log" -f $runStamp)
$evtLog = Join-Path $OutDir ("b3-com12-live-{0}.log" -f $runStamp)

$debugPort = $null
$evtPort = $null

try {
	$debugPort = New-SerialPort -PortName $ComDebug -BaudRate $Baud
	$evtPort = New-SerialPort -PortName $ComEvt -BaudRate $Baud

	$debugPort.Open()
	$evtPort.Open()

	Add-Content -Path $debugLog -Value ("# Capture start {0} {1}" -f (Get-Date -Format "s"), $ComDebug)
	Add-Content -Path $evtLog -Value ("# Capture start {0} {1}" -f (Get-Date -Format "s"), $ComEvt)

	Drain-Port -Port $debugPort -LogPath $debugLog -Until ((Get-Date).AddSeconds(1)) -Timestamp
	Drain-Port -Port $evtPort -LogPath $evtLog -Until ((Get-Date).AddSeconds(1)) -Timestamp

	if ($SendBaseline) {
		$baseline = @("HELP", "DIAG", "INANOW", "AHTNOW", "CFGSHOW", "CMD:GET STATE", "GET STATE")
		foreach ($cmd in $baseline) {
			Add-Content -Path $debugLog -Value (">>> {0}" -f $cmd)
			$debugPort.WriteLine($cmd)
			$window = (Get-Date).AddSeconds(1.5)
			while ((Get-Date) -lt $window) {
				Drain-Port -Port $debugPort -LogPath $debugLog -Until ((Get-Date).AddMilliseconds(120)) -Timestamp
				Drain-Port -Port $evtPort -LogPath $evtLog -Until ((Get-Date).AddMilliseconds(120)) -Timestamp
			}
		}
	}

	Write-Host "Capture active. Perform physical B3-S2/B3-S3/B3-S4/B3-S5 inductions now."
	Write-Host "Duration: $DurationSec sec"

	$end = (Get-Date).AddSeconds($DurationSec)
	while ((Get-Date) -lt $end) {
		Drain-Port -Port $debugPort -LogPath $debugLog -Until ((Get-Date).AddMilliseconds(250)) -Timestamp
		Drain-Port -Port $evtPort -LogPath $evtLog -Until ((Get-Date).AddMilliseconds(250)) -Timestamp
	}

	Add-Content -Path $debugLog -Value ("# Capture end {0}" -f (Get-Date -Format "s"))
	Add-Content -Path $evtLog -Value ("# Capture end {0}" -f (Get-Date -Format "s"))

	Write-Host "COM_DEBUG_LOG=$debugLog"
	Write-Host "COM_EVT_LOG=$evtLog"
}
finally {
	if ($null -ne $debugPort -and $debugPort.IsOpen) {
		$debugPort.Close()
	}
	if ($null -ne $evtPort -and $evtPort.IsOpen) {
		$evtPort.Close()
	}
}
