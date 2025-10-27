# Security Setup Guide

## Database Configuration

This project uses hardcoded database credentials for simplicity. **For production use, consider implementing environment variables.**

### Current Configuration:
- Host: localhost
- Port: 5433
- Database: distributed_task_queue
- User: yugabyte
- Password: yugabyte

### Security Notes:

⚠️ **IMPORTANT:** The current implementation has hardcoded passwords in the source code. This is acceptable for:
- Local development
- Educational projects
- Non-production environments

❌ **DO NOT use this configuration for:**
- Production environments
- Public-facing applications
- Any system handling sensitive data

### For Production Deployment:

1. **Use environment variables** instead of hardcoded credentials
2. **Implement proper secret management** (e.g., Azure Key Vault, AWS Secrets Manager)
3. **Use strong, unique passwords**
4. **Enable SSL/TLS** for database connections
5. **Implement proper access controls**

### Quick Start:

1. **Start YugabyteDB:**
   ```bash
   docker run -d --name yugabytedb -p7000:7000 -p9000:9000 -p5433:5433 yugabytedb/yugabyte:latest
   ```

2. **Create database:**
   ```bash
   docker exec -it yugabytedb ysqlsh -U yugabyte -c "CREATE DATABASE distributed_task_queue;"
   ```

3. **Apply schema:**
   ```bash
   docker exec -it yugabytedb ysqlsh -U yugabyte -d distributed_task_queue -c "$(Get-Content sql/schema.sql)"
   ```

4. **Build and run:**
   ```bash
   # Build the project
   mkdir build && cd build
   cmake ..
   cmake --build . --config Debug
   
   # Run coordinator
   .\coordinator\Debug\coordinator.exe
   
   # Seed tasks
   .\scripts\seed_tasks.ps1 -Count 10
   ```
