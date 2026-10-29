param([string]$Installer, [string]$ExpectedHash, [int]$ObsProcessId,
      [string]$Target, [string]$Marker, [string]$StatusFile)
$ErrorActionPreference = 'Stop'
# Use .NET directly so an inherited PowerShell module path cannot hide commands.
function ObsStillRunning {
    try {
        $process = [System.Diagnostics.Process]::GetProcessById($ObsProcessId)
        try { return !$process.HasExited } finally { $process.Dispose() }
    } catch [System.ArgumentException] { return $false }
}
try {
    $deadline = [DateTime]::Now.AddMinutes(30)
    while ([System.IO.File]::Exists($Marker) -and (ObsStillRunning)) {
        if ([DateTime]::Now -gt $deadline) { throw 'Update expired: OBS was not closed within 30 minutes.' }
        [System.Threading.Thread]::Sleep(1000)
    }
    if (![System.IO.File]::Exists($Marker)) { exit }
    $stream = [System.IO.File]::OpenRead($Installer)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $actualHash = [System.BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
    } finally { $stream.Dispose(); $hasher.Dispose() }
    if ($actualHash -ne $ExpectedHash) { throw 'Installer checksum changed; installation cancelled.' }
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Installer
    $start.Arguments = '/DIR="' + $Target + '"'
    $start.UseShellExecute = $true
    $start.Verb = 'runas'
    $process = [System.Diagnostics.Process]::Start($start)
    try {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw ('Setup was cancelled or failed (exit ' + $process.ExitCode + ').') }
    } finally { $process.Dispose() }
    [System.IO.File]::WriteAllText($StatusFile, 'Update installed. Restart OBS.')
} catch {
    [System.IO.File]::WriteAllText($StatusFile, 'Update failed: ' + $_.Exception.Message)
} finally {
    [System.IO.File]::Delete($Marker)
}
