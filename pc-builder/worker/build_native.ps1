param([Parameter(Mandatory)][string]$Root,[int]$Parallel=4)
$ErrorActionPreference='Stop'
$env:PSModulePath="$env:SystemRoot\System32\WindowsPowerShell\v1.0\Modules"
$workspace=Join-Path $Root 'Install\BuildWorkspace'
$toolkit=Join-Path $Root 'Install\Toolkit'
. (Join-Path $workspace 'NativeBuildFlags.ps1')
$bin=Join-Path $toolkit 'llvm-mingw\bin'
$env:PATH=Get-MkwToolchainPath (Join-Path $toolkit 'llvm-mingw')
$args=Get-MkwNativeConfigureArguments -SourceDirectory (Join-Path $workspace 'runtime') -BuildDirectory (Join-Path $workspace 'native-build') -Ninja (Join-Path $toolkit 'Ninja\ninja.exe') -CCompiler (Join-Path $bin 'x86_64-w64-mingw32-clang.exe') -CxxCompiler (Join-Path $bin 'x86_64-w64-mingw32-clang++.exe') -ResourceCompiler (Join-Path $bin 'x86_64-w64-mingw32-windres.exe') -DependenciesDirectory (Join-Path $workspace 'Dependencies') -NativePrebuiltDirectory (Join-Path $workspace 'Dependencies\native_prebuilt') -AdditionalArguments @('-DMKW_USE_PREBUILT_RUNTIME=OFF',"-DMKW_TRANSLATED_COMPILE_JOBS=$Parallel",'-DMKW_X86_CPU_PROFILE=v3')
& (Join-Path $toolkit 'CMake\bin\cmake.exe') @args
if($LASTEXITCODE -ne 0){throw 'Native configuration failed'}
& (Join-Path $toolkit 'Ninja\ninja.exe') -C (Join-Path $workspace 'native-build') RetroRewind.exe -j $Parallel
if($LASTEXITCODE -ne 0){throw 'Native compilation failed'}
