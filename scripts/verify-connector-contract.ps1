param(
    [Parameter(Mandatory=$false)]
    [string]$RegulatorNetlist = "hardware/kicad/dsp-regulator-rev-b/DSP-Regulator-RevB.net",

    [Parameter(Mandatory=$false)]
    [string]$HatNetlist = "hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net",

    [Parameter(Mandatory=$false)]
    [ValidateSet("baseline", "reduced")]
    [string]$Mode = "reduced",

    [Parameter(Mandatory=$false)]
    [switch]$StrictPins

    ,
    [Parameter(Mandatory=$false)]
    [ValidateSet("strict", "reduced-policy")]
    [string]$BaselineFeedbackPolicy = "reduced-policy"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-Text {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing file: $Path"
    }

    return [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path), [System.Text.Encoding]::UTF8)
}

function Escape-Regex {
    param([string]$Value)
    return [Regex]::Escape($Value)
}

function Get-NetNameCandidates {
    param([string]$NetName)

    $trimmed = $NetName.Trim()
    $candidates = New-Object System.Collections.Generic.List[string]
    $candidates.Add($trimmed)

    $leadingSpace = " $trimmed"
    if ($leadingSpace -ne $trimmed) {
        $candidates.Add($leadingSpace)
    }

    return $candidates
}

function NetHasNode {
    param(
        [string]$Content,
        [string]$NetName,
        [string]$Ref,
        [string]$Pin
    )

    $netEsc = Escape-Regex $NetName
    $refEsc = Escape-Regex $Ref
    $pinEsc = Escape-Regex $Pin

    $pattern = '(?s)\(net\s+\(code\s+"[^"]+"\)\s+\(name\s+"{0}"\).*?\(node\s+\(ref\s+"{1}"\)\s+\(pin\s+"{2}"\)' -f $netEsc, $refEsc, $pinEsc
    return [Regex]::IsMatch($Content, $pattern)
}

function NetExists {
    param(
        [string]$Content,
        [string]$NetName
    )

    $netEsc = Escape-Regex $NetName
    $pattern = '\(net\s+\(code\s+"[^"]+"\)\s+\(name\s+"{0}"\)' -f $netEsc
    return [Regex]::IsMatch($Content, $pattern)
}

function NetHasConnectorNode {
    param(
        [string]$Content,
        [string]$NetName,
        [string]$ConnectorRefPrefix = "J"
    )

    $netEsc = Escape-Regex $NetName
    $refEsc = Escape-Regex $ConnectorRefPrefix
    $pattern = '(?s)\(net\s+\(code\s+"[^"]+"\)\s+\(name\s+"{0}"\).*?\(node\s+\(ref\s+"{1}[^"]*"\)\s+\(pin\s+"[^"]+"\)' -f $netEsc, $refEsc
    return [Regex]::IsMatch($Content, $pattern)
}

function NetHasNodeAnyName {
    param(
        [string]$Content,
        [string]$NetName,
        [string]$Ref,
        [string]$Pin
    )

    foreach ($candidate in (Get-NetNameCandidates -NetName $NetName)) {
        if (NetHasNode -Content $Content -NetName $candidate -Ref $Ref -Pin $Pin) {
            return $true
        }
    }

    return $false
}

function NetHasConnectorNodeAnyName {
    param(
        [string]$Content,
        [string]$NetName,
        [string]$ConnectorRefPrefix = "J"
    )

    foreach ($candidate in (Get-NetNameCandidates -NetName $NetName)) {
        if (NetHasConnectorNode -Content $Content -NetName $candidate -ConnectorRefPrefix $ConnectorRefPrefix) {
            return $true
        }
    }

    return $false
}

$reg = Read-Text -Path $RegulatorNetlist
$hat = Read-Text -Path $HatNetlist

$requiredMappings = @(
    @{ Net = "VSENSE_5V+"; RegRef = "J1"; RegPin = "3"; HatRef = "J4"; HatPin = "3" },
    @{ Net = "VSENSE_5V-"; RegRef = "J1"; RegPin = "2"; HatRef = "J4"; HatPin = "2" },
    @{ Net = "VSENSE_3V3+"; RegRef = "J1"; RegPin = "15"; HatRef = "J4"; HatPin = "15" },
    @{ Net = "VSENSE_3V3-"; RegRef = "J1"; RegPin = "14"; HatRef = "J4"; HatPin = "14" },
    @{ Net = "VSENSE_ADJ+"; RegRef = "J2"; RegPin = "12"; HatRef = "J6"; HatPin = "12" },
    @{ Net = "VSENSE_ADJ-"; RegRef = "J2"; RegPin = "11"; HatRef = "J6"; HatPin = "11" },
    @{ Net = "ISET_MPU_5V"; RegRef = "J1"; RegPin = "5"; HatRef = "J4"; HatPin = "5" },
    @{ Net = "ISET_MPU_3V3"; RegRef = "J1"; RegPin = "10"; HatRef = "J4"; HatPin = "10" },
    @{ Net = "ISET_MPU_Channel_3"; RegRef = "J2"; RegPin = "5"; HatRef = "J6"; HatPin = "5" },
    @{ Net = "+5V_reg"; RegRef = "J1"; RegPin = "6"; HatRef = "J4"; HatPin = "6" },
    @{ Net = "+3.3V_Reg"; RegRef = "J1"; RegPin = "11"; HatRef = "J4"; HatPin = "11" },
    @{ Net = "+V Adj Channel"; RegRef = "J2"; RegPin = "13"; HatRef = "J6"; HatPin = "13" },
    @{ Net = "+12V"; RegRef = "J2"; RegPin = "8"; HatRef = "J6"; HatPin = "10" },
    @{ Net = "FAULT_CRITICAL_SUM"; RegRef = "J2"; RegPin = "3"; HatRef = "J6"; HatPin = "3" }
)

$feedbackMappings = @(
    @{ Net = "Feedback 5V"; RegRef = "J1"; RegPin = "4"; HatRef = "J4"; HatPin = "4"; BaselineHatOptional = $false },
    @{ Net = "Feedback 3.3"; RegRef = "J1"; RegPin = "9"; HatRef = "J4"; HatPin = "9"; BaselineHatOptional = $true },
    @{ Net = "Feedback Adj Channel"; RegRef = "J2"; RegPin = "4"; HatRef = "J6"; HatPin = "4"; BaselineHatOptional = $false }
)

$checks = New-Object System.Collections.Generic.List[Object]

foreach ($m in $requiredMappings) {
    if ($StrictPins) {
        $regOk = NetHasNodeAnyName -Content $reg -NetName $m.Net -Ref $m.RegRef -Pin $m.RegPin
        $hatOk = NetHasNodeAnyName -Content $hat -NetName $m.Net -Ref $m.HatRef -Pin $m.HatPin
    } else {
        # Non-strict mode tolerates connector/pin renumbering and only requires net presence on a connector in each netlist.
        $regOk = NetHasConnectorNodeAnyName -Content $reg -NetName $m.Net
        $hatOk = NetHasConnectorNodeAnyName -Content $hat -NetName $m.Net
    }
    $ok = $regOk -and $hatOk

    $checks.Add([pscustomobject]@{
        Check = "Required mapping $($m.Net)"
        Result = $(if ($ok) { "PASS" } else { "FAIL" })
        Detail = $(if ($StrictPins) { "Reg $($m.RegRef)-$($m.RegPin), HAT $($m.HatRef)-$($m.HatPin)" } else { "Connector net present in both netlists" })
    })
}

foreach ($m in $feedbackMappings) {
    $regHas = NetHasNodeAnyName -Content $reg -NetName $m.Net -Ref $m.RegRef -Pin $m.RegPin
    $hatHas = NetHasNodeAnyName -Content $hat -NetName $m.Net -Ref $m.HatRef -Pin $m.HatPin
    $present = $regHas -or $hatHas

    if ($Mode -eq "baseline") {
        if ($BaselineFeedbackPolicy -eq "strict") {
            if ($m.BaselineHatOptional) {
                $ok = $regHas
                $expectation = "must exist on regulator connector; HAT side optional in baseline"
            } else {
                $ok = $regHas -and $hatHas
                $expectation = "must exist on both connectors"
            }
        } else {
            # Reduced-policy baseline treats feedback crossing presence as informational.
            $ok = $true
            $expectation = $(if ($present) { "informational under reduced-policy baseline (crossing present)" } else { "informational under reduced-policy baseline (no crossing)" })
        }
    } else {
        $ok = -not $present
        $expectation = "must not cross connectors"
    }

    $checks.Add([pscustomobject]@{
        Check = "Feedback mapping $($m.Net)"
        Result = $(if ($ok) { "PASS" } else { "FAIL" })
        Detail = $expectation
    })
}

$bootNames = @("+5V_Boot", "+5V_Bootstrap")
$bootFound = @()
foreach ($name in $bootNames) {
    if ((NetExists -Content $reg -NetName $name) -or (NetExists -Content $hat -NetName $name)) {
        $bootFound += $name
    }
}

$singleBootName = ($bootFound.Count -eq 1)
$checks.Add([pscustomobject]@{
    Check = "Bootstrap naming normalization"
    Result = $(if ($singleBootName) { "PASS" } else { "FAIL" })
    Detail = "Found: $($bootFound -join ', ')"
})

$checks | Format-Table -AutoSize

$failCount = @($checks | Where-Object { $_.Result -eq "FAIL" }).Count
if ($failCount -gt 0) {
    Write-Error "Connector contract verification failed with $failCount failing checks."
}

Write-Host "Connector contract verification passed in mode '$Mode'." -ForegroundColor Green
