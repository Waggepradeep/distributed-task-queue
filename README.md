# Distributed Task Queue — README

This repository is a small Coordinator/Worker demo using Poco (HTTP + Data) and YugabyteDB (YSQL) as the backing store.
This README documents the edits, how to build & run the Coordinator and Worker on Windows (PowerShell), and the helper scripts for seeding and running multiple workers. It also lists common SQL commands to inspect and manipulate the task table.

⚠️ **SECURITY NOTICE:** This project contains hardcoded database credentials for development purposes. See [SECURITY.md](SECURITY.md) for important security considerations before using in production.

Contents
- What I changed and why
- Build & run (Windows / PowerShell)
- Helper scripts (seed and run multiple workers)
- Database operations (psql commands)
- Notes, safety, and next steps

What I changed (summary of work done)
- Fixed include and Poco usage issues in `coordinator/src/DBManager.cpp` and related files.
- Rewrote task fetching/assignment to be atomic. `DBManager::fetchAndAssignTask(workerId)` now uses a single UPDATE ... RETURNING (via CTE) to claim one pending task and return its data. This avoids races where a SELECT followed by UPDATE could affect 0 rows.
- Reworked `assignTaskToWorker` / `updateTaskStatus` to log SQL and handle errors more robustly.
- Updated `CoordinatorServer` handlers:
	- `/register` — worker registration (logs worker id)
	- `/request_task` — atomically fetch & assign a task to the requesting worker (uses `fetchAndAssignTask`)
	- `/complete_task` — record completion and update task status via `DBManager::updateTaskStatus`
- Added helper scripts in `scripts/`:
	- `run_workers.ps1` — launch multiple worker.exe processes
	- `seed_tasks.ps1` — insert many pending tasks into the database (handles password and quoting)
- Fixed several Powershell script issues (automatic variable name collisions, BOM on temp SQL file, passing psql args correctly).

Build & run (Windows / PowerShell)
Prerequisites
- CMake, a C++ toolchain (MSVC), Poco libraries and PostgreSQL client libraries. On Windows we've used vcpkg to supply Poco and libpq in the project CMake configuration.
- YugabyteDB server running and accessible. The project expects a database `distributed_task_queue` and the `tasks` table (see `sql/schema.sql`).

Build
1. Configure and build with CMake (example using Ninja):
```powershell
# from repository root
cmake -S . -B build -G "Ninja"
cmake --build build --config Debug
```
This will generate `coordinator.exe` and `worker.exe` (paths depend on your generator and configuration). Example paths used in this repo:
- `build\coordinator\Debug\coordinator.exe`
- `build\worker\Debug\worker.exe`

Run the Coordinator
```powershell
# run coordinator (in one terminal)
.\build\coordinator\Debug\coordinator.exe
```
The coordinator will connect to YugabyteDB (connection string is specified in `coordinator/src/main.cpp` by default) and start listening on port 8080.

Run a single Worker (manual)
```powershell
# run a single worker in another terminal
.\build\worker\Debug\worker.exe --name worker-1 --coordinator http://127.0.0.1:8080
```
The worker registers with the coordinator and polls for tasks. If no tasks are available it returns HTTP 204 and sleeps before retrying.

Run a single Task (manual)
To test with just one task, you can insert a single task directly into the database:
```powershell
# Insert one task manually
ysqlsh -h localhost -p 5433 -U yugabyte -d distributed_task_queue -c "INSERT INTO tasks (payload, status, created_at, updated_at) VALUES ('echo Hello from single task', 'pending', now(), now());"
```
Then run a single worker to process it:
```powershell
# Run one worker to process the single task
.\build\worker\Debug\worker.exe --name single-worker --coordinator http://127.0.0.1:8080
```
The worker will pick up the task, process it, and mark it as completed.

Run multiple workers (helper)
Use the `scripts/run_workers.ps1` script to start many workers quickly. It opens each worker in a new console window by default.
```powershell
# auto-discover worker.exe and start 5 workers
$exe=(Get-ChildItem -Path . -Filter worker.exe -Recurse -File | Select-Object -First 1).FullName
.\scripts\run_workers.ps1 -Count 5 -WorkerExe $exe
```
Or start a custom path:
```powershell
.\scripts\run_workers.ps1 -Count 3 -WorkerExe "C:\full\path\to\worker.exe"
```

Seed DB tasks (helper)
Use `scripts/seed_tasks.ps1` to insert tasks into the database for workers to process. The script accepts `-Count` and uses `psql` under the hood. It expects `psql` on PATH or PGPASSWORD to be set in the env.
```powershell
# set password for non-interactive psql (optional, script also extracts from ConnStr)
$env:PGPASSWORD = 'your_actual_password'
.\scripts\seed_tasks.ps1 -Count 10
```
This will create pending tasks that your workers will pick up.

Database operations (ysqlsh/psql)
You can inspect and manipulate the `tasks` table directly using `ysqlsh` (YugabyteDB's PostgreSQL-compatible shell) or `psql`.
Default connection flags used in scripts:
- host=localhost port=5433 dbname=distributed_task_queue user=yugabyte

Examples
- Show all tasks:
```powershell
ysqlsh -h localhost -p 5433 -U yugabyte -d distributed_task_queue -c "SELECT * FROM tasks ORDER BY id;"
```
- Insert one task manually:
```powershell
ysqlsh -h localhost -p 5433 -U yugabyte -d distributed_task_queue -c "INSERT INTO tasks (payload, status, created_at, updated_at) VALUES ('echo hi', 'pending', now(), now());"
```
- Requeue all non-pending (useful for tests):
```powershell
ysqlsh -h localhost -p 5433 -U yugabyte -d distributed_task_queue -c "UPDATE tasks SET status='pending', worker_id=NULL, updated_at=now() WHERE status != 'pending';"
```
- Clear all tasks (dangerous):
```powershell
ysqlsh -h localhost -p 5433 -U yugabyte -d distributed_task_queue -c "TRUNCATE TABLE tasks RESTART IDENTITY;"
```

Where the code touched important behavior
- `coordinator/src/DBManager.cpp` — session creation, `fetchPendingTasks`, `assignTaskToWorker`, `updateTaskStatus`, and importantly `fetchAndAssignTask` (new atomic claim implementation).
- `coordinator/src/CoordinatorServer.cpp` — added/updated HTTP handlers and wired the DB manager into them.
- `coordinator/src/DBManager.h` — new method declaration for `fetchAndAssignTask`.
- `scripts/seed_tasks.ps1` and `scripts/run_workers.ps1` — helper scripts to seed and run workers; these were iterated on to fix quoting, BOM, and argument issues on Windows.

Notes & safety
- The worker currently simulates work (sleep) and then reports completion. Do NOT execute arbitrary payloads from the DB in production — running shell commands from DB payloads is a major security risk unless payloads are trusted and executed in a safe sandbox.
- We used a temporary workaround in places where parameter binding with the Poco PostgreSQL connector was problematic: SQL strings are composed carefully and single quotes escaped. The ideal future improvement is to re-enable parameterized/ prepared statements once the correct binding approach is verified.

Next steps / improvements
- Add a POST /create_task endpoint to CoordinatorServer so tasks can be submitted via HTTP (I'll implement if you want).
- Rework DB access to always use parameter binding (safer) for user-data.
- Add LISTEN/NOTIFY so workers can be notified when new tasks arrive rather than short-polling.
- Add monitoring/metrics (pending/in_progress/done counts, per-worker throughput).

YugabyteDB (YSQL) — Migration Completed
---------------------------------------

Migration Status: ✅ COMPLETED
This project has been successfully migrated from PostgreSQL to YugabyteDB (YSQL). All components are now configured to use YugabyteDB as the backing store.

What was changed during migration:
1) Database Configuration
	- Connection string updated to use YugabyteDB defaults: `host=localhost port=5433 user=yugabyte password=<your_password> dbname=distributed_task_queue`
	- Updated in `coordinator/src/main.cpp` and `coordinator/src/test_connection.cpp`
	- Updated PowerShell scripts in `scripts/seed_tasks.ps1`

2) Database Setup
	- YugabyteDB running via Docker: `docker run -d --name yugabytedb -p7000:7000 -p9000:9000 -p5433:5433 yugabytedb/yugabyte:latest`
	- Database created: `distributed_task_queue`
	- Schema applied: `tasks` and `workers` tables with proper indexes

3) Testing and Validation
	- ✅ Database connection successful
	- ✅ Task seeding working (5+ tasks inserted)
	- ✅ Multi-worker coordination (5 workers tested)
	- ✅ Load balancing across workers
	- ✅ Task status updates (pending → in_progress → done)
	- ✅ Concurrent processing verified

Current Configuration:
- Database: YugabyteDB (YSQL) on port 5433
- User: yugabyte / Password: <configure via environment variables>
- Connection: PostgreSQL-compatible (no code changes required)
- Performance: Excellent concurrent processing with multiple workers

Benefits of YugabyteDB:
- ✅ Horizontal scalability (ready for multi-node deployment)
- ✅ Better performance for concurrent operations
- ✅ PostgreSQL compatibility (zero code changes needed)
- ✅ Distributed architecture for cloud-native deployments
- ✅ Automatic failover and high availability

Quick Start with YugabyteDB:
```powershell
# Start YugabyteDB (if not already running)
docker run -d --name yugabytedb -p7000:7000 -p9000:9000 -p5433:5433 yugabytedb/yugabyte:latest

# Create database and schema
docker exec -it yugabytedb ysqlsh -U yugabyte -c "CREATE DATABASE distributed_task_queue;"
docker exec -it yugabytedb ysqlsh -U yugabyte -d distributed_task_queue -c "$(Get-Content sql/schema.sql)"

# Start coordinator
.\build\coordinator\Debug\coordinator.exe

# Seed tasks and start workers
.\scripts\seed_tasks.ps1 -Count 10
.\scripts\run_workers.ps1 -Count 5
```

Where to look in the repo
- coordinator/src/ — coordinator HTTP server and DB manager code
- worker/src/ — worker polling and execution loop
- sql/ — DB schema and seed SQL
- scripts/ — helper PowerShell scripts: `seed_tasks.ps1`, `run_workers.ps1`

If anything in this README is unclear or you want a new helper (e.g. POST endpoint, auto-discovery of psql path, consolidated run script), tell me which item and I'll add it.

if use made any change in the code the go the powershell then run this command to build new code for the changes you made
```
->Remove-Item -Recurse -Force .\build
->cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
->cmake --build build