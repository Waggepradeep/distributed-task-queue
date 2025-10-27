Param(
    [int]$Count = 3,
    [string]$Coordinator = "http://127.0.0.1:8080",
    [string]$BaseName = "worker",
    [string]$WorkerExe = ".\\build\\worker\\Debug\\worker.exe"
)

# Resolve worker executable
$exe = Resolve-Path -LiteralPath $WorkerExe -ErrorAction SilentlyContinue
if (-not $exe) {
    Write-Error "Worker executable not found at '$WorkerExe'. Build the project or adjust -WorkerExe path."
    exit 1
}
$exePath = $exe.Path

for ($i = 1; $i -le $Count; $i++) {
    $name = "${BaseName}-$i"
    # Use an explicit argument list array (avoid overwriting automatic $args)
    $argList = @("--coordinator", $Coordinator, "--name", $name)
    Write-Output "Starting $name -> $exePath $($argList -join ' ')"
    # Start each worker in a new console window
    Start-Process -FilePath $exePath -ArgumentList $argList
    Start-Sleep -Milliseconds 200
}

Write-Output "Started $Count workers."