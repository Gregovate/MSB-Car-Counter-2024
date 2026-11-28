#requires -Version 5.1
<#
.SYNOPSIS
Downloads and repairs the November 27, 2026 Car Counter CSV exports.
.DESCRIPTION
Run before more official traffic, with the file manager idle. The device uses a
single shared current directory. Preview is read-only apart from selecting it.
-Apply stages all changes and verifies each download before replacing any file.
Both the PC and SD retain backups. Re-run after a disconnect to finish the swap.
#>
[CmdletBinding()]
param(
    [string]$BaseUrl = 'http://192.168.3.31',
    [string]$BackupRoot = (Join-Path (Get-Location) 'car-counter-repair-backups'),
    [switch]$Apply
)
$ErrorActionPreference = 'Stop'
$BaseUrl = $BaseUrl.TrimEnd('/')
$curl = (Get-Command curl.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (!$curl) { $curl = (Get-Command curl -CommandType Application | Select-Object -First 1).Source }
$work = Join-Path ([IO.Path]::GetFullPath($BackupRoot)) ([guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $work -Force
Write-Host "Local backups and repaired CSVs: $work"
$ascii = [Text.Encoding]::ASCII
$names = 'EnterLog.csv','HourlyData.csv','ShowSummary.csv','ShowSummaryBuckets.csv'
$oldEntryHash = 'ca9fc53c224d8229386e60ffaf65765c6ff60b88f98cedf367d071b4014f269b'
$newEntryHash = '3078809bf952cc7e2c836acd53e02d48b1e35aae6526ab2115359fa0c6ecaf5b'
$missingTimes = @'
17:01:20 17:01:55 17:05:03 17:05:23 17:06:10 17:09:44 17:11:09 17:11:35
17:12:30 17:12:51 17:14:20 17:16:00 17:16:57 17:17:42 17:19:05 17:20:30
17:25:48 17:30:39 17:32:58 17:34:17 17:34:36 17:35:39 17:38:05 17:45:15
17:46:28 17:50:34 17:51:00 17:52:47 17:56:05 17:56:17 17:58:02 17:58:40
17:59:22 17:59:45 17:59:51 18:01:14 18:02:18 18:02:25 18:05:17 18:05:40
18:06:22 18:08:16 18:11:36 18:11:54 18:13:49 18:17:17 18:21:20 18:22:31
18:27:26 18:29:04 18:31:01 18:32:07 18:36:41 18:37:01 18:42:15 18:42:57
18:48:53 18:50:26 18:51:33 18:53:22 19:04:49 19:07:46 19:10:59 19:12:23
19:15:12 19:19:13 19:21:24 19:22:19 19:24:15 19:28:04 19:30:40 19:34:17
19:38:09 19:38:19 19:44:48 19:46:48 19:47:35 19:48:55 19:49:22 19:49:37
19:54:58 19:58:27 20:04:24 20:06:52 20:07:16 20:13:35 20:15:40 20:19:05
20:24:19 20:28:11 20:34:14 20:44:11 20:50:21 20:53:29
'@ -split '\s+'

function Hash-File($Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Request-Device($Route, $Output, [string[]]$Options = @(), [switch]$AllowMissing) {
    $status = & $curl --silent --show-error --connect-timeout 8 --max-time 60 --output $Output --write-out '%{http_code}' @Options "$BaseUrl/$Route"
    if ($LASTEXITCODE -ne 0) { throw "Connection failed on $Route. Backups: $work" }
    if ($status -eq '404' -and $AllowMissing) { return $false }
    if ($status -ne '200') { throw "HTTP $status on $Route. See $Output" }
    return $true
}
function Select-Season {
    $reply = Join-Path $work 'response.txt'
    $null = Request-Device 'changeDirectory?dir=%2FCC%2F2026' $reply
    if ([IO.File]::ReadAllText($reply).Trim() -cne 'Changed directory to /CC/2026') { throw 'Unexpected device directory.' }
}
function Get-DeviceFile($Name, $Output) {
    Select-Season
    Request-Device "download?filename=$Name" $Output -AllowMissing
}
function Move-DeviceFile($From, $To) {
    Select-Season
    $null = Request-Device "rename?old=$From&new=$To" (Join-Path $work 'response.txt')
}
function Make-Entry($InputPath, $OutputPath) {
    if ((Hash-File $InputPath) -eq $newEntryHash) { Copy-Item $InputPath $OutputPath; return }
    if ((Hash-File $InputPath) -ne $oldEntryHash) { throw 'EnterLog differs from the audited original. Refusing to replace it.' }
    $lines = [IO.File]::ReadAllLines($InputPath)
    $events = @{}
    # Index 1 is the motorcycle-associated record. Keep measured fields for cars.
    foreach ($line in $lines[2..($lines.Length-1)]) {
        $fields = $line -split ',\s*'
        $events.Add($fields[0], $fields)
    }
    foreach ($time in $missingTimes) { $events.Add("2026-11-27 $time", $null) }
    $result = [Collections.Generic.List[string]]::new()
    $result.Add(($lines[0] -replace ',\s*',','))
    $number = 0
    $lastOriginal = -1
    foreach ($stamp in ($events.Keys | Sort-Object)) {
        $number++
        $f = $events[$stamp]
        if ($null -eq $f) {
            $result.Add("$stamp,,$number,,")
            $lastOriginal = -1
        } else {
            $interval = ''
            if ($lastOriginal -eq ([int]$f[2] - 1)) { $interval = $f[4] }
            $result.Add((($stamp,$f[1],$number,$f[3],$interval) -join ','))
            $lastOriginal = [int]$f[2]
        }
    }
    [IO.File]::WriteAllText($OutputPath, ($result -join "`r`n") + "`r`n", $ascii)
    if ((Hash-File $OutputPath) -ne $newEntryHash) { throw 'Rebuilt log failed the audit fingerprint.' }
}
function Make-Report($Name, $InputPath, $OutputPath) {
    $date = '2026-11-27'
    switch ($Name) {
        'ShowSummary.csv' {
            $header = 'Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,After9PM,DailyTotal,ShowTotal,DailyAvgTemp,ExtensionMin'
            # After9PM in this cumulative export includes all cars through closing.
            $old = "$date,1,0,301,552,766,930,930,930,930,42.4,0"
            $new = "$date,1,0,335,611,847,1023,1023,1023,1023,42.4,0"
        }
        'ShowSummaryBuckets.csv' {
            $header = 'Date,DaysRunning,6PM,7PM,8PM,9PM,After9PM,DailyTotal,ShowTotal,DailyAvgTemp,ExtensionMin'
            $old = "$date,1,301,251,214,164,0,930,930,42.4,0"
            $new = "$date,1,335,276,236,176,0,1023,1023,42.4,0"
        }
        'HourlyData.csv' {
            $header = 'Date,' + ((0..23 | ForEach-Object { 'Hr {0:00}' -f $_ }) -join ',')
            $hours = @(0) * 17 + @(301,251,214,164) + @(0) * 3
            $old = "$date," + ($hours -join ',')
            $hours = @(0) * 17 + @(335,276,236,176) + @(0) * 3
            $new = "$date," + ($hours -join ',')
        }
    }
    $body = [IO.File]::ReadAllText($InputPath)
    if (($body -split '\r?\n',2)[0] -cne $header) { throw "Unexpected header in $Name." }
    $pattern = "(?m)^$date,[^\r\n]*"
    $rows = [regex]::Matches($body,$pattern)
    if ($rows.Count -ne 1 -or $rows[0].Value -cnotin @($old,$new)) { throw "Unexpected November 27 values in $Name. Originals saved in $work." }
    [IO.File]::WriteAllText($OutputPath,[regex]::Replace($body,$pattern,$new),$ascii)
}

# Download and validate all four originals before uploading any replacement.
$plans = foreach ($name in $names) {
    $source = Join-Path $work $name
    $fixed = Join-Path $work "fixed-$name"
    $backup = "Nov27-original-$name"
    $stage = "Nov27-stage-$name"
    $present = Get-DeviceFile $name $source
    $saved = Join-Path $work $backup
    $hasBackup = Get-DeviceFile $backup $saved
    if (!$present) {
        if (!$hasBackup) { throw "Missing $name and its repair backup." }
        Copy-Item $saved $source -Force
    }
    if ($name -eq 'EnterLog.csv') { Make-Entry $source $fixed } else { Make-Report $name $source $fixed }
    $oldHash = Hash-File $source
    $newHash = Hash-File $fixed
    if ($present -and $oldHash -eq $newHash) { Write-Host "$name already correct"; continue }
    if ($present -and $hasBackup) { throw "Backup name already exists alongside an uncorrected $name. Inspect $work." }
    Write-Host "$name ready"
    [pscustomobject]@{Name=$name; Stage=$stage; Backup=$backup; Fixed=$fixed; OldHash=$oldHash; NewHash=$newHash; Present=$present}
}
if (!$Apply) { Write-Host 'Preview complete. Run with -Apply to write the verified files.'; return }
$check = Join-Path $work 'readback.tmp'
foreach ($plan in $plans) {
    Select-Season
    # A relative upload filename avoids native argument quoting differences
    # between Windows PowerShell 5.1 and PowerShell 7 when the repo has spaces.
    Push-Location -LiteralPath $work
    try {
        $part = 'file=@' + [IO.Path]::GetFileName($plan.Fixed) + ';filename=' + $plan.Stage
        $null = Request-Device 'upload' (Join-Path $work 'response.txt') @('--form',$part)
    } finally { Pop-Location }
    if (!(Get-DeviceFile $plan.Stage $check) -or (Hash-File $check) -ne $plan.NewHash) { throw "Staging verification failed for $($plan.Name)." }
}
foreach ($plan in $plans) {
    $exists = Get-DeviceFile $plan.Name $check
    if ($exists -ne $plan.Present -or ($exists -and (Hash-File $check) -ne $plan.OldHash)) { throw "Device file changed during repair: $($plan.Name)" }
    if ($exists) { Move-DeviceFile $plan.Name $plan.Backup }
    try {
        Move-DeviceFile $plan.Stage $plan.Name
        if (!(Get-DeviceFile $plan.Name $check) -or (Hash-File $check) -ne $plan.NewHash) { throw 'Readback mismatch.' }
    } catch {
        if (!(Get-DeviceFile $plan.Name $check)) { Move-DeviceFile $plan.Backup $plan.Name }
        throw
    }
    Write-Host "$($plan.Name) installed and verified"
}
Write-Host 'Repair complete. November 27 entrance total: 1023.'
