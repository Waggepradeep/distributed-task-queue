CREATE TABLE IF NOT EXISTS tasks (
id SERIAL PRIMARY KEY,
payload TEXT NOT NULL,
status VARCHAR(20) NOT NULL DEFAULT 'pending', -- pending, in_progress, done, failed
created_at TIMESTAMP WITH TIME ZONE DEFAULT now(),
updated_at TIMESTAMP WITH TIME ZONE DEFAULT now(),
worker_id VARCHAR(128)
);


CREATE INDEX IF NOT EXISTS idx_tasks_status_created ON tasks(status, created_at);


CREATE TABLE IF NOT EXISTS workers (
id VARCHAR(128) PRIMARY KEY,
registered_at TIMESTAMP WITH TIME ZONE DEFAULT now(),
last_seen TIMESTAMP WITH TIME ZONE DEFAULT now(),
metadata JSONB
);