# Software OpenGL for the Windows CI VM only; never included in the release.
$ErrorActionPreference = 'Stop'
$archive = Join-Path $env:RUNNER_TEMP 'mesa.7z'
$destination = Join-Path $env:RUNNER_TEMP 'mesa'
$url = 'https://github.com/pal1000/mesa-dist-win/releases/download/26.2.6/mesa3d-26.2.6-release-mingw.7z'
Invoke-WebRequest $url -OutFile $archive
$expected = 'da0f71d9181b672c1918f3c342f11118c687f6059a46b16502845160733cc388'
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'Mesa archive checksum mismatch'
}
& 7z x $archive "-o$destination" -y
if ($LASTEXITCODE -ne 0) { throw 'Mesa extraction failed' }
"PCBEE_MESA_DIR=$destination\x64" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
