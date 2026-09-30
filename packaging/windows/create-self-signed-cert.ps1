param(
    [string]$CertName = "WorkQ CI",
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

$resolvedDir = (Resolve-Path -LiteralPath $OutputDir).Path
$password = "WorkQ-" + [Guid]::NewGuid().ToString("N") + "!"
$pfxPath = Join-Path $resolvedDir "workq-ci-cert.pfx"
$passwordPath = Join-Path $resolvedDir "workq-ci-cert-password.txt"

$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject ("CN=" + $CertName) `
    -CertStoreLocation Cert:\CurrentUser\My `
    -KeyExportPolicy Exportable `
    -KeySpec Signature `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -NotAfter (Get-Date).AddYears(3)

$securePassword = ConvertTo-SecureString -String $password -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $securePassword | Out-Null

Set-Content -LiteralPath $passwordPath -Value $password -Encoding ascii

if ($env:GITHUB_ENV) {
    "WORKQ_CERT_PASSWORD=$password" | Add-Content -LiteralPath $env:GITHUB_ENV -Encoding ascii
    "WORKQ_CERT_PATH=$pfxPath" | Add-Content -LiteralPath $env:GITHUB_ENV -Encoding ascii
}

Write-Output "Created self-signed code signing certificate: $pfxPath"
