# Collects a read-only flash-content report and an independently verified backup.
# Requires Car firmware .20 or later. Does not initialize/erase flash or reset counts.
[CmdletBinding()]
param(
    [string]$Device = 'http://192.168.3.31',
    [string]$OutputDirectory = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('MSB-Counter-Backups\CarJournalInspection-' + (Get-Date -Format 'yyyyMMdd-HHmmss')))
)
$ErrorActionPreference = 'Stop'
$Device = $Device.TrimEnd('/')
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$reportPath = Join-Path $OutputDirectory 'CarJournalInspection.json'
$backupPath = Join-Path $OutputDirectory 'CarJournalRegion.bin'
$deadline = (Get-Date).AddMinutes(3)
do {
    $report = Invoke-RestMethod -Uri "$Device/diagnostics/journal" -TimeoutSec 30
    $report | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -Path $reportPath
    if ($report.journalHealth -ne 'FOREIGN_DATA_PRESERVED') {
        Write-Host "Journal health: $($report.journalHealth). No inspection backup requested."
        Write-Host "Report: $reportPath"
        return
    }
    if ($report.backupStatus -eq 'FAILED') { throw "Preservation failed: $($report.backupError). Internal flash is unchanged. Report: $reportPath" }
    if ($report.backupStatus -eq 'VERIFIED') { break }
    Write-Progress -Activity 'Preserving Car Counter journal-region evidence' -Status "$($report.backupStatus): $($report.backupProgress) / $($report.inspectedBytes) bytes"
    if ((Get-Date) -gt $deadline) { throw "Backup is still $($report.backupStatus). Internal flash is unchanged. Report: $reportPath" }
    Start-Sleep -Seconds 2
} while ($true)
Write-Progress -Activity 'Preserving Car Counter journal-region evidence' -Completed
Invoke-WebRequest -Uri "$Device/diagnostics/journal-backup" -OutFile $backupPath -UseBasicParsing -TimeoutSec 60
$bytes = [IO.File]::ReadAllBytes($backupPath)
if ($bytes.Length -ne $report.inspectedBytes) { throw 'Downloaded backup has the wrong length. No initialization is authorized.' }
if (-not ('MsbCounterIntegrity' -as [type])) {
    Add-Type -TypeDefinition @'
public static class MsbCounterIntegrity {
    public static uint Fnv(byte[] data) {
        unchecked { uint hash = 2166136261U; foreach (byte value in data) hash = (hash ^ value) * 16777619U; return hash; }
    }
}
'@
}
$actual = '{0:X8}' -f [MsbCounterIntegrity]::Fnv($bytes)
if ($actual -ne $report.backupFnv32) { throw 'Downloaded backup checksum differs from the verified source. No initialization is authorized.' }
$report | Add-Member -NotePropertyName localBackup -NotePropertyValue $backupPath -Force
$report | Add-Member -NotePropertyName localSHA256 -NotePropertyValue (Get-FileHash -Path $backupPath -Algorithm SHA256).Hash -Force
$report | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -Path $reportPath
Write-Host "Verified backup: $backupPath"
Write-Host "Diagnostic report: $reportPath"
Write-Host 'Share CarJournalInspection.json for analysis. Keep the binary backup locally.'
Write-Host 'No internal flash was erased or initialized; no counts were reset.'
