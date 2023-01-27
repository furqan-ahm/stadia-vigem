[CmdletBinding()]
param (
    [Parameter()][ValidateSet("DEBUG", "RELEASE")][string]$Configuration = "DEBUG",
    [Parameter()][ValidateSet("x86", "x64")][string]$Architecture = "x86",
    [Parameter()][bool]$SkipBuildToolsSetup = $false
)

$script:CommonFlags = @("/Zi", "/W4", "/EHsc", "/DWIN32", "/D_UNICODE", "/DUNICODE")
$script:DebugFlags = @( "/Od" )
$script:ReleaseFlags = @("/GL", "/O2")

function Import-Prerequisites {

    if ($SkipBuildToolsSetup -eq $true) {
        return
    }

    if (Get-Module -ListAvailable -Name VSSetup) {
        Update-Module -Name VSSetup
    } else {
        Install-Module -Name VSSetup -Scope CurrentUser -Force
    }

    Import-Module -Name VSSetup

    if (Get-Module -ListAvailable -Name WintellectPowerShell) {
        Update-Module -Name WintellectPowerShell
    } else {
        Install-Module -Name WintellectPowerShell -Scope CurrentUser -Force
    }

    Import-Module -Name WintellectPowerShell
}

function Invoke-BuildTools {
    param (
        $Architecture
    )
    
    if ($SkipBuildToolsSetup -eq $true) {
        return
    }

    $latestVsInstallationInfo = Get-VSSetupInstance -All | Sort-Object -Property InstallationVersion -Descending | Select-Object -First 1
    
    Invoke-CmdScript "$($latestVsInstallationInfo.InstallationPath)\VC\Auxiliary\Build\vcvarsall.bat" $Architecture
}

function Invoke-Build-libstadia {
    param (
        $Architecture
    )

    $OutputName = "libstadia-$Architecture.lib"
    $Flags = If ($Configuration -eq "DEBUG") {$script:DebugFlags} else {$script:ReleaseFlags}

    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch

    Write-Host "*** ${OutputName}: Build started ***"
    Write-Host

    $StopWatch.Start()

    & "cl.exe" /c $Flags $CommonFlags /Ilibstadia/include /Foobj/libstadia/ libstadia/src/*.c
    & "lib.exe" /out:bin/$OutputName obj/libstadia/*.obj 

    $StopWatch.Stop()

    Write-Host
    Write-Host "*** ${OutputName}: Build finished in $($StopWatch.Elapsed) ***"
}

function Invoke-Build-Stadia-Tester {
    param (
        $Architecture
    )

    $OutputName = "stadia-tester-$Architecture.exe"
    $Flags = If ($Configuration -eq "DEBUG") {$script:DebugFlags} else {$script:ReleaseFlags}

    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch

    Write-Host "*** ${OutputName}: Build started ***"
    Write-Host

    $StopWatch.Start()

    & "cl.exe" $Flags $CommonFlags /Ilibstadia/include /Foobj/stadia-tester/ /Febin/$OutputName stadia-tester/src/*.c User32.lib

    $StopWatch.Stop()

    Write-Host
    Write-Host "*** ${OutputName}: Build finished in $($StopWatch.Elapsed) ***"
}

function Invoke-Build-Service {
    param (
        $Architecture
    )

    $OutputName = "Stadia-ViGEm.Service.$Architecture.exe"
    $ResourceDllName = "Stadia-ViGEm.Service.$Architecture.Resources.dll";
    $Flags = If ($Configuration -eq "DEBUG") {$script:DebugFlags} else {$script:ReleaseFlags}
    $LibraryPath = "bin/libstadia-$Architecture.lib"

    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch

    Write-Host "*** ${OutputName}: Build started ***"
    Write-Host

    $StopWatch.Start()

    & "mc.exe" -U service/res/messages.mc -h service/include/ -r service/res/generated
    & "rc.exe" /Foobj/service/Stadia-ViGEm.Service.Messages.res service/res/generated/messages.rc
    & "link.exe" -dll -noentry -out:bin/$ResourceDllName obj/service/Stadia-ViGEm.Service.Messages.res
    
    & "rc.exe" /foobj/service/Stadia-ViGEm.Service.res service/res/res.rc
    & "cl.exe" $Flags $CommonFlags /Ilibstadia/include /IViGEmClient/include /Iservice/include /Foobj/service/ /Febin/$OutputName ViGEmClient/src/*.cpp obj/service/Stadia-ViGEm.Service.res service/src/*.c $LibraryPath

    $StopWatch.Stop()

    Write-Host
    Write-Host "*** ${OutputName}: Build finished in $($StopWatch.Elapsed) ***"
}

function Invoke-Build-Tray {
    param (
        $Architecture
    )

    $OutputName = "Stadia-ViGEm.Tray.$Architecture.exe"
    $Flags = If ($Configuration -eq "DEBUG") {$script:DebugFlags} else {$script:ReleaseFlags}
    $LibraryPath = "bin/libstadia-$Architecture.lib"

    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch

    Write-Host "*** ${OutputName}: Build started ***"
    Write-Host

    $StopWatch.Start()

    & "rc.exe" /foobj/tray/tray.res tray/res/res.rc
    & "cl.exe" $Flags $CommonFlags /Ilibstadia/include /IViGEmClient/include /Itray/include /Foobj/tray/ /Febin/$OutputName ViGEmClient/src/*.cpp obj/tray/tray.res tray/src/*.c $LibraryPath

    $StopWatch.Stop()

    Write-Host
    Write-Host "*** ${OutputName}: Build finished in $($StopWatch.Elapsed) ***"
}

# Entry

New-Item -Path "bin" -ItemType Directory -Force > $null
New-Item -Path "obj" -ItemType Directory -Force > $null
New-Item -Path "obj/libstadia" -ItemType Directory -Force > $null
New-Item -Path "obj/stadia-tester" -ItemType Directory -Force > $null
New-Item -Path "obj/service" -ItemType Directory -Force > $null
New-Item -Path "obj/tray" -ItemType Directory -Force > $null

Import-Prerequisites

Write-Host "-- Build started. Configuration: $Configuration, Architecture: $Architecture --"

if ($Architecture -eq "x86" -Or $Architecture -eq "ALL") {
    Invoke-BuildTools -Architecture "x86"
    Invoke-Build-libstadia -Architecture "x86"
    Invoke-Build-Stadia-Tester -Architecture "x86"
    Invoke-Build-Service -Architecture "x86"
    #Invoke-Build-Tray -Architecture "x86"
}

if ($Architecture -eq "x64" -Or $Architecture -eq "ALL") {
    Invoke-BuildTools -Architecture "x64"
    Invoke-Build-libstadia -Architecture "x64"
    Invoke-Build-Stadia-Tester -Architecture "x64"
    Invoke-Build-Service -Architecture "x64"
    #Invoke-Build-Tray -Architecture "x64"
}

Write-Host "-- Build completed. --"
