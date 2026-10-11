param(
    [string]$LCSCExportPath = "$HOME\Downloads\export_project_20260829_181958.xls",
    [string]$AuditPath = "docs/REVC_PROCUREMENT_AUDIT_2026-08-29.csv",
    [string]$OutputPath = "docs/REVC_SYMBOL_FIELD_COMPLETION_2026-08-29.csv"
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $LCSCExportPath)) { throw "LCSC export not found: $LCSCExportPath" }
if (-not (Test-Path $AuditPath)) { throw "Procurement audit not found: $AuditPath" }

$excel = New-Object -ComObject Excel.Application
$excel.Visible = $false
$workbook = $excel.Workbooks.Open((Resolve-Path $LCSCExportPath), $null, $true)
$sheet = $workbook.Worksheets.Item(1)
$usedRange = $sheet.UsedRange
$lcscByPartNumber = @{}

try {
    # LCSC writes its matching-results headers on row 5. Columns 11-26 are the matched-product fields.
    for ($row = 6; $row -le $usedRange.Rows.Count; $row++) {
        $lcscPartNumber = ([string]$sheet.Cells.Item($row, 16).Text).Trim()
        if ($lcscPartNumber -notmatch '^C\d+$') { continue }
        $lcscByPartNumber[$lcscPartNumber] = [pscustomobject]@{
            'Matched Manufacturer Part Number' = ([string]$sheet.Cells.Item($row, 11).Text).Trim()
            'Matched Manufacturer' = ([string]$sheet.Cells.Item($row, 12).Text).Trim()
            'Matched Package' = ([string]$sheet.Cells.Item($row, 17).Text).Trim()
            Availability = ([string]$sheet.Cells.Item($row, 22).Text).Trim()
            'Matched Description' = ([string]$sheet.Cells.Item($row, 24).Text).Trim()
            'Product Link' = ([string]$sheet.Cells.Item($row, 26).Text).Trim()
        }
    }
}
finally {
    $workbook.Close($false)
    $excel.Quit()
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($usedRange)
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($sheet)
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($workbook)
    [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel)
}

$completionRows = foreach ($auditRow in (Import-Csv $AuditPath | Where-Object { $_.'Alignment Status' -eq 'REVIEW_IDENTITY_FIELDS' })) {
    $match = $lcscByPartNumber[$auditRow.'LCSC Part #']
    [pscustomobject]@{
        Board = $auditRow.Board
        Reference = $auditRow.Reference
        Value = $auditRow.Value
        Footprint = $auditRow.Footprint
        'LCSC Part #' = $auditRow.'LCSC Part #'
        'Current MPN' = $auditRow.'Manufacturer Part Number'
        'Current Manufacturer' = $auditRow.Manufacturer
        'Suggested MPN' = if ($match) { $match.'Matched Manufacturer Part Number' } else { '' }
        'Suggested Manufacturer' = if ($match) { $match.'Matched Manufacturer' } else { '' }
        'LCSC Package' = if ($match) { $match.'Matched Package' } else { '' }
        Availability = if ($match) { $match.Availability } else { 'NOT IN PROJECT EXPORT' }
        'LCSC Description' = if ($match) { $match.'Matched Description' } else { '' }
        'Product Link' = if ($match) { $match.'Product Link' } else { '' }
        'Review Status' = if (-not $match) { 'HOLD_NOT_IN_LCSC_EXPORT' } elseif ($match.Availability -ne 'In Stock') { 'REVIEW_BACKORDER' } else { 'READY_FOR_FIELD_REVIEW' }
    }
}

$directory = Split-Path -Parent $OutputPath
if ($directory) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
$completionRows | Sort-Object Board, Reference | Export-Csv -NoTypeInformation -Encoding utf8 -Path $OutputPath
$summary = $completionRows | Group-Object 'Review Status' | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Count)" }
Write-Output "Wrote $($completionRows.Count) symbol-field completion rows to $OutputPath ($($summary -join ', '))."