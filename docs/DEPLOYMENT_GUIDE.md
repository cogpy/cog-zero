# Agent-Zero Deployment Guide

Guide for deploying Agent-Zero in production environments.

## Table of Contents

1. [Deployment Options](#deployment-options)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Monitoring](#monitoring)
6. [Scaling](#scaling)
7. [Security](#security)
8. [Maintenance](#maintenance)

## Deployment Options

### Option 1: Standalone Application

Deploy as a standalone executable for single-agent scenarios.

```bash
# Build release version
cd agents/cpp
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Install
sudo make install
sudo ldconfig

# Run
agentzero-standalone --config=/etc/agentzero/config.yaml
```

### Option 2: CogServer Module

Deploy as a CogServer module for distributed systems.

```bash
# Build with CogServer integration
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_COGSERVER_MODULE=ON \
      ..
make install

# Start CogServer with Agent-Zero
cogserver -c /etc/opencog/cogserver.conf
```

### Option 3: Docker Container

Use Docker for containerized deployment.

```dockerfile
# Dockerfile
FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Install OpenCog
COPY . /opt/opencog
WORKDIR /opt/opencog/build
RUN cmake .. && \
    make cogutil atomspace cogserver && \
    make install && \
    ldconfig

# Build Agent-Zero
WORKDIR /opt/opencog/agents/cpp/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && \
    make install

# Expose ports
EXPOSE 17001 18001 5000

# Run
CMD ["agentzero-standalone"]
```

Build and run:
```bash
docker build -t agentzero:latest .
docker run -d -p 17001:17001 -p 18001:18001 agentzero:latest
```

### Option 4: Kubernetes Deployment

For large-scale deployments:

```yaml
# kubernetes/deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: agentzero
spec:
  replicas: 3
  selector:
    matchLabels:
      app: agentzero
  template:
    metadata:
      labels:
        app: agentzero
    spec:
      containers:
      - name: agentzero
        image: agentzero:latest
        ports:
        - containerPort: 17001
        - containerPort: 18001
        resources:
          requests:
            memory: "4Gi"
            cpu: "2"
          limits:
            memory: "8Gi"
            cpu: "4"
        env:
        - name: AGENTZERO_CONFIG
          value: "/etc/agentzero/config.yaml"
        volumeMounts:
        - name: config
          mountPath: /etc/agentzero
      volumes:
      - name: config
        configMap:
          name: agentzero-config
```

## System Requirements

### Production Environment

**Minimum**:
- CPU: 4 cores
- RAM: 8 GB
- Disk: 20 GB
- OS: Ubuntu 20.04+

**Recommended**:
- CPU: 8+ cores
- RAM: 16+ GB
- Disk: 50+ GB SSD
- OS: Ubuntu 22.04 LTS

### Scaling Guidelines

| Agents | CPUs | RAM | Storage |
|--------|------|-----|---------|
| 1-5 | 4 | 8 GB | 20 GB |
| 5-20 | 8 | 16 GB | 50 GB |
| 20-50 | 16 | 32 GB | 100 GB |
| 50+ | 32+ | 64+ GB | 200+ GB |

## Installation

### Production Installation

```bash
# Install as system package
cd agents/cpp/build-release
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
sudo make install
sudo ldconfig

# Verify installation
agentzero --version
```

### System Integration

```bash
# Create system user
sudo useradd -r -s /bin/false agentzero

# Create directories
sudo mkdir -p /etc/agentzero
sudo mkdir -p /var/lib/agentzero
sudo mkdir -p /var/log/agentzero

# Set permissions
sudo chown -R agentzero:agentzero /var/lib/agentzero
sudo chown -R agentzero:agentzero /var/log/agentzero
```

### Systemd Service

Create `/etc/systemd/system/agentzero.service`:

```ini
[Unit]
Description=Agent-Zero Cognitive Agent
After=network.target

[Service]
Type=simple
User=agentzero
Group=agentzero
WorkingDirectory=/var/lib/agentzero
ExecStart=/usr/bin/agentzero-standalone --config=/etc/agentzero/config.yaml
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Resource limits
MemoryLimit=8G
CPUQuota=400%

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable agentzero
sudo systemctl start agentzero
sudo systemctl status agentzero
```

## Configuration

### Configuration File

`/etc/agentzero/config.yaml`:

```yaml
# Agent-Zero Production Configuration

agent:
  name: "ProductionAgent"
  log_level: "INFO"
  
atomspace:
  initial_size: 10000
  max_size: 1000000
  cleanup_interval: 1000  # steps
  
cognitive_loop:
  max_iterations: -1  # unlimited
  step_timeout_ms: 1000
  phases:
    - perception
    - attention
    - reasoning
    - planning
    - action
    - learning
    - reflection

perception:
  sensors:
    - type: "file"
      path: "/var/lib/agentzero/input"
      polling_interval_ms: 100

attention:
  ecan_enabled: true
  min_sti: 10
  max_sti: 100
  decay_rate: 0.1

reasoning:
  pln_enabled: true
  max_inference_steps: 100
  rule_base: "AgentZeroRules"

planning:
  max_planning_time_ms: 1000
  replan_threshold: 0.3

learning:
  enabled: true
  experience_buffer_size: 10000
  learning_rate: 0.01
  
  moses:
    max_evals: 10000
    pop_size: 100

storage:
  type: "rocksdb"
  path: "/var/lib/agentzero/db"
  persist_interval: 100  # steps

monitoring:
  enabled: true
  metrics_port: 9090
  health_check_interval_ms: 5000

logging:
  file: "/var/log/agentzero/agent.log"
  max_size_mb: 100
  max_backups: 10
```

### Environment-Specific Configs

Development (`config.dev.yaml`):
```yaml
agent:
  log_level: "DEBUG"
  
atomspace:
  max_size: 100000  # Smaller for dev
  
cognitive_loop:
  max_iterations: 1000  # Limited for testing
```

Production (`config.prod.yaml`):
```yaml
agent:
  log_level: "INFO"
  
atomspace:
  max_size: 10000000  # Much larger
  
cognitive_loop:
  max_iterations: -1  # Unlimited
```

## Monitoring

### Health Checks

Implement HTTP health endpoint:

```cpp
class HealthCheckServer {
public:
    void start(int port) {
        // Simple HTTP server on port
        // Returns 200 OK if agent is healthy
    }
    
    bool checkHealth() {
        return agent_->isInitialized() &&
               !agent_->isStuck() &&
               getMemoryUsage() < memoryLimit_;
    }
};
```

### Metrics

Expose Prometheus metrics:

```cpp
#include <prometheus/counter.h>
#include <prometheus/gauge.h>

class MetricsCollector {
    prometheus::Counter& cognitiveSteps_;
    prometheus::Gauge& atomspaceSize_;
    prometheus::Histogram& stepDuration_;
    
public:
    void recordCognitiveStep(double durationMs) {
        cognitiveSteps_.Increment();
        stepDuration_.Observe(durationMs);
    }
    
    void updateAtomSpaceSize(size_t size) {
        atomspaceSize_.Set(size);
    }
};
```

### Log Aggregation

Use structured logging:

```cpp
#include <spdlog/spdlog.h>

void logStructured(const std::string& event, 
                  const nlohmann::json& data) {
    nlohmann::json log {
        {"timestamp", getCurrentTimestamp()},
        {"level", "INFO"},
        {"event", event},
        {"data", data}
    };
    spdlog::info(log.dump());
}

// Usage
logStructured("cognitive_step_completed", {
    {"step", stepCount},
    {"duration_ms", duration},
    {"atoms_created", atomsCreated}
});
```

### Monitoring Dashboard

Example Grafana queries:

```promql
# Cognitive steps per second
rate(agentzero_cognitive_steps_total[1m])

# Average step duration
rate(agentzero_step_duration_seconds_sum[5m]) / 
rate(agentzero_step_duration_seconds_count[5m])

# AtomSpace size
agentzero_atomspace_size

# Memory usage
process_resident_memory_bytes
```

## Scaling

### Horizontal Scaling

Multiple agent instances:

```yaml
# Load balancer configuration
upstream agentzero {
    least_conn;
    server agent1:17001;
    server agent2:17001;
    server agent3:17001;
}
```

### Vertical Scaling

Resource optimization:

```cpp
// Tune for available resources
void optimizeForResources(int cpuCores, size_t memoryBytes) {
    // Adjust parallelism
    setThreadPoolSize(cpuCores);
    
    // Adjust memory limits
    setAtomSpaceMaxSize(memoryBytes * 0.7);  // 70% for AtomSpace
    setExperienceBufferSize(memoryBytes * 0.2);  // 20% for experiences
}
```

### Distributed Deployment

Multi-agent coordination:

```cpp
class DistributedAgentSystem {
    std::vector<AgentConnection> agents_;
    MessageBroker broker_;
    
public:
    void distributeTask(const Task& task) {
        // Select agent based on load
        auto agent = selectLeastLoadedAgent();
        agent->assignTask(task);
    }
    
    void synchronizeKnowledge() {
        // Periodic knowledge sharing
        for (auto& agent : agents_) {
            auto knowledge = agent->exportKnowledge();
            broker_.broadcast(knowledge);
        }
    }
};
```

## Security

### Network Security

```bash
# Firewall rules
sudo ufw allow 17001/tcp  # CogServer
sudo ufw allow 18001/tcp  # Web UI
sudo ufw deny 9090/tcp    # Metrics (internal only)
```

### Authentication

```cpp
class AuthenticationManager {
public:
    bool authenticate(const Credentials& creds) {
        // Implement authentication
        return verifyCredentials(creds);
    }
    
    void requireAuth(bool enabled) {
        authEnabled_ = enabled;
    }
};
```

### Data Protection

```cpp
// Encrypt sensitive data at rest
void persistSensitiveData(const Data& data) {
    auto encrypted = encrypt(data, encryptionKey_);
    storage_->store(encrypted);
}

// Secure communication
void configureTLS() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    SSL_CTX_use_certificate_file(ctx, certPath_, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, keyPath_, SSL_FILETYPE_PEM);
}
```

## Maintenance

### Backup Strategy

```bash
#!/bin/bash
# backup.sh

BACKUP_DIR="/backup/agentzero/$(date +%Y%m%d)"
mkdir -p "$BACKUP_DIR"

# Backup AtomSpace
systemctl stop agentzero
tar -czf "$BACKUP_DIR/atomspace.tar.gz" /var/lib/agentzero/db
systemctl start agentzero

# Backup configuration
cp -r /etc/agentzero "$BACKUP_DIR/config"

# Backup logs
cp /var/log/agentzero/agent.log "$BACKUP_DIR/"

# Retention (keep 30 days)
find /backup/agentzero -mtime +30 -delete
```

### Updates

```bash
# Rolling update procedure
#!/bin/bash

# 1. Build new version
cd /tmp/agentzero-update
git pull
cd agents/cpp/build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 2. Stop service
sudo systemctl stop agentzero

# 3. Backup current version
sudo cp /usr/bin/agentzero-standalone /usr/bin/agentzero-standalone.bak

# 4. Install new version
sudo make install
sudo ldconfig

# 5. Test
/usr/bin/agentzero-standalone --version

# 6. Start service
sudo systemctl start agentzero

# 7. Verify
sleep 5
sudo systemctl status agentzero
```

### Performance Tuning

```bash
# System tuning for production
# /etc/sysctl.conf

# Increase max open files
fs.file-max = 1000000

# TCP tuning
net.core.somaxconn = 1024
net.ipv4.tcp_max_syn_backlog = 2048

# Shared memory
kernel.shmmax = 17179869184
kernel.shmall = 4194304

# Apply
sudo sysctl -p
```

## Best Practices

1. ✅ **Use Release Builds** in production
2. ✅ **Enable Monitoring** from day one
3. ✅ **Implement Health Checks** for load balancers
4. ✅ **Regular Backups** of AtomSpace data
5. ✅ **Resource Limits** to prevent runaway growth
6. ✅ **Structured Logging** for debugging
7. ✅ **Graceful Shutdown** to preserve state
8. ✅ **Version Control** configurations
9. ✅ **Test Updates** in staging first
10. ✅ **Document Changes** in change log

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
