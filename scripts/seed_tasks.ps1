Param(
    [int]$Count = 10,
    [string]$ConnStr = "",
    [string]$Prefix = "echo Task from seed"
)

# If no connection string provided, build from environment variables
if ([string]::IsNullOrEmpty($ConnStr)) {
    $dbHost = if ($env:DB_HOST) { $env:DB_HOST } else { "localhost" }
    $dbPort = if ($env:DB_PORT) { $env:DB_PORT } else { "5433" }
    $dbName = if ($env:DB_NAME) { $env:DB_NAME } else { "distributed_task_queue" }
    $dbUser = if ($env:DB_USER) { $env:DB_USER } else { "yugabyte" }
    $dbPassword = if ($env:DB_PASSWORD) { $env:DB_PASSWORD } else { "yugabyte" }
    
    $ConnStr = "host=$dbHost port=$dbPort dbname=$dbName user=$dbUser password=$dbPassword"
}

function ConvertTo-SqlLiteral([string]$s) {
    return $s -replace "'", "''"
}

# Extract password from connection string (if present) and set PGPASSWORD so psql won't prompt.
# Then remove the password token from the connection string passed to psql for cleanliness.
$pgPassMatch = [regex]::Match($ConnStr, 'password=([^\s]+)')
if ($pgPassMatch.Success) {
    $env:PGPASSWORD = $pgPassMatch.Groups[1].Value
    # remove the password=... token from the connection string we pass to psql
    $ConnStr = ($ConnStr -replace 'password=[^\s]+', '').Trim()
}

# Parse connection string into components so we can pass explicit psql flags (-h -p -U -d)
$connParts = @{}
foreach ($part in ($ConnStr -split '\s+')) {
    if ($part -match '=') {
        $kv = $part -split '=', 2
        $key = $kv[0].ToLower()
        $val = $kv[1]
        $connParts[$key] = $val
    }
}

# Default values if any missing (avoid reserved automatic variable names like $host)
# Use explicit checks because -or is a boolean operator and would return True/False
if ($connParts.ContainsKey('host')) { $pgHost = $connParts['host'] } else { $pgHost = 'localhost' }
if ($connParts.ContainsKey('port')) { $pgPort = $connParts['port'] } else { $pgPort = '5432' }
if ($connParts.ContainsKey('dbname')) { $pgDbName = $connParts['dbname'] }
elseif ($connParts.ContainsKey('db')) { $pgDbName = $connParts['db'] } else { $pgDbName = 'postgres' }
if ($connParts.ContainsKey('user')) { $pgUser = $connParts['user'] } else { $pgUser = 'postgres' }

# Build a single SQL script with all inserts and execute it in one psql call to avoid quoting issues
$sqlLines = New-Object System.Collections.Generic.List[string]
for ($i = 1; $i -le $Count; $i++) {
    $payload = "$Prefix $i"
    $payloadEsc = ConvertTo-SqlLiteral $payload
    $sqlLines.Add("INSERT INTO tasks (payload, status, created_at, updated_at) VALUES ('$payloadEsc', 'pending', now(), now());")
    Write-Output "Prepared task: $payload"
}

# Write SQL to a temporary file without a UTF-8 BOM to avoid psql parsing issues
$tmp = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), ([System.Guid]::NewGuid().ToString() + ".sql"))
# WriteAllLines with UTF8Encoding(false) writes UTF-8 without BOM
[System.IO.File]::WriteAllLines($tmp, $sqlLines, (New-Object System.Text.UTF8Encoding($false)))

Write-Output "Executing $Count inserts via psql using temp file: $tmp"

# Call psql once with -f <file>
$psqlCmd = @('-h', $pgHost, '-p', $pgPort, '-U', $pgUser, '-d', $pgDbName, '-f', $tmp)
try {
    & psql @psqlCmd
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
        Write-Warning "psql returned exit code $exit. Command attempted: psql $($psqlCmd -join ' ')"
    } else {
        Write-Output "Inserted $Count tasks."
    }
}
catch {
    Write-Warning "Failed to execute psql: $_"
}
finally {
    Remove-Item -Path $tmp -ErrorAction SilentlyContinue
}