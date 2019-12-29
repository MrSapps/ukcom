param([String]$psyq_path)

$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot

$oldPathEnv = $Env:path 

Function psyq_setup($psyq_path)
{
    # Setup PSYQ env vars
    $Env:path = $oldPathEnv + ";" + $psyq_path
    $Env:PSYQ_PATH = $psyq_path
    # Setup PSYQ ini
    $psyq_path_without_bin = $psyq_path
    if ($psyq_path_without_bin.EndsWith("\bin\", "CurrentCultureIgnoreCase"))
    {
        $psyq_path_without_bin = $psyq_path_without_bin.Substring(0, $psyq_path_without_bin.Length - 5)
    }
    elseif ($psyq_path_without_bin.EndsWith("\bin", "CurrentCultureIgnoreCase"))
    {
        $psyq_path_without_bin = $psyq_path_without_bin.Substring(0, $psyq_path_without_bin.Length - 4)
    }

    $env:C_PLUS_INCLUDE_PATH = "$psyq_path_without_bin\include"
    $Env:c_include_path = "$psyq_path_without_bin\include"
    $env:PSX_PATH = $psyq_path

    (Get-Content $psyq_path\psyq.ini.template) | 
    Foreach-Object {$_ -replace '\$PSYQ_PATH',$psyq_path_without_bin}  | 
    Out-File $psyq_path\psyq.ini
}

# Cache created dirs to avoid constantly hitting the disk
$createdDirs = @{}

function compile_c($fileName)
{
    $objName = $objName.replace(".C", ".obj").replace(".c", ".obj").replace("src\", "obj\")

    if (-Not $forceRebuild -And [System.IO.File]::Exists("$objName"))
    {
        $asmWriteTime = (get-item "$fileName").LastWriteTime
        $objWriteTime = (get-item "$objName").LastWriteTime
        $upToDate = $asmWriteTime -le $objWriteTime
    }


    $parentFolder = Split-Path -Path $objName -Parent
    if (-Not ($createdDirs.contains($parentFolder)))
    {
        Write-Host "Make dir $parentFolder" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
        New-Item -ItemType directory -Path $parentFolder -ErrorAction SilentlyContinue | out-null
        $createdDirs.add($parentFolder, $parentFolder)
    }

    ccpsx.exe -O2 -G 0 -g -c -Wall "$fileName" "-o$objName"
    if($LASTEXITCODE -eq 0)
    {
        Write-Host "Compiled $fileName" -ForegroundColor "green"
    } 
    else 
    {
        Write-Error "Compilation failed for: ccpsx.exe -O2 -G 0 -g -c -Wall $fileName -o$objName"
    }
}

#Remove-Item $PSScriptRoot\..\obj -Recurse -ErrorAction Ignore | out-null
New-Item -ItemType directory -Path $PSScriptRoot\obj -ErrorAction SilentlyContinue | out-null

Write-Host "PSYQ setup" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
psyq_setup($psyq_path)

# Compile all .C files
Write-Host "Obtain C source list" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
$cFiles = Get-ChildItem -Recurse src\* -Include *.c | Select-Object -ExpandProperty FullName

Write-Host "Compile C source" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
foreach ($file in $cFiles)
{
    $objName = $file
    compile_c($objName)
}

# Compile all .S files
Write-Host "Obtain ASM source list" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
$sFiles =  Get-ChildItem -Recurse asm\* -Include *.s | Select-Object -ExpandProperty FullName
foreach ($file in $sFiles)
{
    $fullObjName =  $file.replace(".S", ".obj").replace(".s", ".obj").replace("asm\", "obj\")
    $fullSName = $file

    $parentFolder = Split-Path -Path $fullObjName -Parent
    if (-Not ($createdDirs.contains($parentFolder)))
    {
        Write-Host "Make dir $parentFolder" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
        New-Item -ItemType directory -Path $parentFolder -ErrorAction SilentlyContinue | out-null
        $createdDirs.add($parentFolder, $parentFolder)
    }

    asmpsx.exe /j ../asm /l /q $fullSName,$fullObjName
    if($LASTEXITCODE -eq 0)
    {
        Write-Host "Assembled $fullSName"  -ForegroundColor "green"
    }
    else 
    {
        Write-Error "Assembling failed for asmpsx.exe /l /q $fullSName,$fullObjName"
    }
}

# Run the linker
Write-Host "link rom" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
psylink.exe /z /l $psyq_path\..\LIB /q /c /m /p "@$PSScriptRoot\linker_command_file_rom.txt",$PSScriptRoot\obj\ukcom.rom,$PSScriptRoot\obj\ukcom.sym,$PSScriptRoot\obj\ukcom.map
if($LASTEXITCODE -eq 0)
{
    Write-Host "Linked ukcom.rom" -ForegroundColor "yellow"
} 
else 
{
    Write-Error "Linking failed $LASTEXITCODE"
}

Write-Host "link exe" -ForegroundColor "DarkMagenta" -BackgroundColor "Black"
psylink.exe /l $psyq_path\..\LIB /q /c /m "@$PSScriptRoot\linker_command_file_exe.txt",$PSScriptRoot\obj\ukcom_exe.cpe,$PSScriptRoot\obj\ukcom_exe.sym,$PSScriptRoot\obj\ukcom_exe.map
if($LASTEXITCODE -eq 0)
{
    Write-Host "Linked ukcom_exe.exe" -ForegroundColor "yellow"
} 
else 
{
    Write-Error "Linking failed $LASTEXITCODE"
}

cpe2exe.exe /CJ obj\ukcom_exe.cpe | out-null
if($LASTEXITCODE -eq 0)
{
    Write-Host "obj\ukcom_exe.cpe -> ..\obj\ukcom_exe.exe" -ForegroundColor "yellow"
} 
else 
{
    Write-Error "Converting CPE to EXE failed"
}