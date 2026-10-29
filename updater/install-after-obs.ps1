param([string]$Installer, [string]$ExpectedHash, [int]$ObsProcessId,
      [string]$Target, [string]$Marker, [string]$StatusFile)
$ErrorActionPreference = 'Stop'
try {
    $deadline = (Get-Date).AddMinutes(30)
    while ((Test-Path -LiteralPath $Marker) -and (Get-Process -Id $ObsProcessId -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $deadline) { throw 'Update expired: OBS was not closed within 30 minutes.' }
        Start-Sleep -Seconds 1
    }
    if (!(Test-Path -LiteralPath $Marker)) { exit }
    if ((Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) {
        throw 'Installer checksum changed; installation cancelled.'
    }
    $process = Start-Process -FilePath $Installer -ArgumentList ('/DIR="' + $Target + '"') -Verb RunAs -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw ('Setup was cancelled or failed (exit ' + $process.ExitCode + ').') }
    Set-Content -LiteralPath $StatusFile -Value 'Update installed. Restart OBS.' -Encoding UTF8
} catch {
    Set-Content -LiteralPath $StatusFile -Value ('Update failed: ' + $_.Exception.Message) -Encoding UTF8
} finally {
    Remove-Item -LiteralPath $Marker -ErrorAction SilentlyContinue
}
