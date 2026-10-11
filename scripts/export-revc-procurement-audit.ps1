param(
    [string]$AuditOutputPath = "docs/REVC_PROCUREMENT_AUDIT_2026-08-29.csv",
    [string]$CartOutputPath = "docs/REVC_LCSC_CART_IMPORT_2026-08-28.csv"
)

$ErrorActionPreference = 'Stop'
$fields = 'Reference,Value,Footprint,LCSC,Part,Manufacturer,Datasheet,QUANTITY,DNP'
$labels = 'Reference,Value,Footprint,LCSC Part #,Manufacturer Part Number,Manufacturer,Datasheet,Qty,DNP'
$sources = @(
    @{ Board = 'Regulator'; Schematic = 'hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch'; Export = 'hardware/kicad/dsp-regulator-rev-c/procurement-source-bom.csv' },
    @{ Board = 'HAT'; Schematic = 'hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch'; Export = 'hardware/kicad/dsp-regulator-hat-rev-c/procurement-source-bom.csv' }
)

if (-not (Get-Command kicad-cli -ErrorAction SilentlyContinue)) {
    throw 'kicad-cli was not found on PATH. Install KiCad 10 and add its bin directory to PATH.'
}

$auditRows = foreach ($source in $sources) {
    $null = & kicad-cli sch export bom $source.Schematic --output $source.Export --fields $fields --labels $labels --ref-delimiter ';'
    if ($LASTEXITCODE -ne 0) { throw "KiCad BOM export failed: $($source.Schematic)" }

    foreach ($bomRow in (Import-Csv $source.Export)) {
        $lcscAssigned = $bomRow.'LCSC Part #' -match '^C\d+$'
        $dnp = $bomRow.DNP -eq 'DNP'
        $alignment = if ($dnp) { 'DNP' } elseif (-not $lcscAssigned) { 'HOLD_NO_LCSC' } elseif (-not $bomRow.'Manufacturer Part Number' -or -not $bomRow.Manufacturer) { 'REVIEW_IDENTITY_FIELDS' } else { 'ALIGNED' }

        [pscustomobject]@{
            Board = $source.Board
            Reference = $bomRow.Reference
            Value = $bomRow.Value
            Footprint = $bomRow.Footprint
            DNP = $bomRow.DNP
            'LCSC Part #' = $bomRow.'LCSC Part #'
            'Manufacturer Part Number' = $bomRow.'Manufacturer Part Number'
            Manufacturer = $bomRow.Manufacturer
            Datasheet = $bomRow.Datasheet
            'Alignment Status' = $alignment
            'Two-board Qty' = if ($dnp) { 0 } else { 2 * [int]$bomRow.Qty }
            Source = $source.Schematic
        }
    }
}

$auditDirectory = Split-Path -Parent $AuditOutputPath
if ($auditDirectory) { New-Item -ItemType Directory -Force -Path $auditDirectory | Out-Null }
$auditRows | Sort-Object Board, Reference | Export-Csv -NoTypeInformation -Encoding utf8 -Path $AuditOutputPath

$cartRows = $auditRows |
    Where-Object { $_.'Alignment Status' -in 'ALIGNED', 'REVIEW_IDENTITY_FIELDS' } |
    Group-Object 'LCSC Part #' |
    ForEach-Object {
        $members = $_.Group
        [pscustomobject]@{
            'LCSC Part #' = $_.Name
            Quantity = ($members | Measure-Object 'Two-board Qty' -Sum).Sum
            References = (($members | ForEach-Object { "$($_.Board) $($_.Reference)" }) -join '; ')
            'Manufacturer Part Number' = (($members.'Manufacturer Part Number' | Where-Object { $_ } | Select-Object -Unique) -join '; ')
            Manufacturer = (($members.Manufacturer | Where-Object { $_ } | Select-Object -Unique) -join '; ')
            'Alignment Status' = if ($members.'Alignment Status' -contains 'REVIEW_IDENTITY_FIELDS') { 'REVIEW_IDENTITY_FIELDS' } else { 'ALIGNED' }
        }
    } |
    Sort-Object 'LCSC Part #'

$cartDirectory = Split-Path -Parent $CartOutputPath
if ($cartDirectory) { New-Item -ItemType Directory -Force -Path $cartDirectory | Out-Null }
$cartRows | Export-Csv -NoTypeInformation -Encoding utf8 -Path $CartOutputPath

$summary = $auditRows | Group-Object 'Alignment Status' | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Count)" }
Write-Output "Wrote $($auditRows.Count) reference rows to $AuditOutputPath and $($cartRows.Count) consolidated LCSC lines to $CartOutputPath ($($summary -join ', '))."
