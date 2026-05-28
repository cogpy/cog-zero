# Real-Time Monitoring Dashboard Guide

This guide covers the WebSocket-enabled monitoring dashboard for CogZero agents.

## Overview

The MonitoringServer provides a real-time web dashboard for monitoring agent status, metrics, and state. Features include:

- Live metrics streaming via WebSocket
- Interactive atom visualization
- Goal and task status tracking
- Agent controls (pause/resume)

## Quick Start

### Enable Dashboard

```cpp
#include "cog0/Agent.h"
#include "cog0/MonitoringServer.h"

cog0::Agent agent;
cog0::MonitoringServer server(
    agent.atomStore().shared_from_this(),
    agent.cognitiveLoop().shared_from_this(),
    8080  // port
);

// Enable WebSocket support
server.enableWebSocket(true);

// Start server
server.start();

// Access dashboard at http://localhost:8080/dashboard
```

## HTTP Endpoints

### GET /health

Health check endpoint for load balancers and monitoring systems.

**Response:**
```json
{
  "status": "ok",
  "uptime_s": 3600.5
}
```

### GET /metrics

Detailed performance metrics.

**Response:**
```json
{
  "atomCount": 1250,
  "cycleCount": 5420,
  "rulesFiredTotal": 8930,
  "avgCycleMs": 12.5,
  "uptimeSeconds": 3600.5,
  "pendingTasks": 3
}
```

### GET /atoms

List all atoms in the AtomStore.

**Response:**
```json
[
  {
    "name": "concept-1",
    "type": "ConceptNode",
    "strength": 0.95,
    "confidence": 0.88
  }
]
```

### GET /attention

Atoms sorted by attention (STI).

**Response:**
```json
[
  {
    "name": "high-priority-concept",
    "sti": 150,
    "lti": 50
  }
]
```

### GET /dashboard

Embedded HTML/JS monitoring dashboard.

## WebSocket Endpoints

### /ws/metrics

Real-time metrics stream (JSON, every 500ms).

**Client Connection:**
```javascript
const ws = new WebSocket('ws://localhost:8080/ws/metrics');
ws.onmessage = (event) => {
    const metrics = JSON.parse(event.data);
    console.log('Atoms:', metrics.atomCount);
    console.log('Cycles:', metrics.cycleCount);
};
```

**Message Format:**
```json
{
  "type": "metrics",
  "timestamp": 1705315800,
  "data": {
    "atomCount": 1250,
    "cycleCount": 5421,
    "avgCycleMs": 12.3,
    "pendingTasks": 2
  }
}
```

### /ws/atoms

Atom change notifications.

**Message Format:**
```json
{
  "type": "atom_change",
  "action": "added|modified|removed",
  "atom": {
    "name": "new-concept",
    "type": "ConceptNode",
    "strength": 0.5,
    "confidence": 0.5
  }
}
```

### /ws/attention

Attention value updates.

**Message Format:**
```json
{
  "type": "attention_update",
  "atom": "concept-1",
  "sti": 120,
  "lti": 45
}
```

## Dashboard Features

### Metrics Panel

- **Atom Count**: Total atoms in AtomStore
- **Cycle Count**: Cognitive loop iterations
- **Rules Fired**: Total inference rule activations
- **Avg Cycle Time**: Moving average of cycle duration
- **Uptime**: Server running time

### Goals & Tasks Panel

Live display of:
- Active goals with priority and achievement status
- Pending/running tasks with status

### Atom Visualization

Mini hypergraph showing:
- High-attention atoms (STI > threshold)
- Atom connections/links
- Type coloring (ConceptNode, PredicateNode, etc.)

### Agent Controls

- **Pause/Resume**: Toggle cognitive loop
- **Inject Percept**: Send text percept to agent
- **Clear Atoms**: Reset AtomStore (with confirmation)

## Configuration

### Port Configuration

```cpp
// Default port 8080
MonitoringServer server(store, loop, 8080);

// Custom port
MonitoringServer server(store, loop, 9090);
```

### Custom Metrics

```cpp
// Add application-specific metrics
server.setExtraMetricsHook([]() {
    return R"("customCounter": 42, "customGauge": 3.14)";
});
```

### Disable WebSocket

```cpp
// HTTP-only mode (polling)
server.enableWebSocket(false);
```

## Security

### Production Recommendations

1. **Reverse Proxy**: Place behind nginx/Apache with HTTPS
2. **Authentication**: Add basic auth or API keys
3. **Rate Limiting**: Prevent DoS attacks
4. **IP Filtering**: Restrict to internal network

### TLS Configuration

```cpp
// Enable TLS (requires OpenSSL/mbedTLS)
server.enableTLS("/path/to/cert.pem", "/path/to/key.pem");
```

## Prometheus Integration

The `/metrics` endpoint can be scraped by Prometheus. Example config:

```yaml
scrape_configs:
  - job_name: 'cog0'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
```

## Troubleshooting

### Dashboard Not Loading

1. Check server is running: `curl http://localhost:8080/health`
2. Verify port not in use: `lsof -i :8080`
3. Check firewall rules

### WebSocket Connection Failed

1. Ensure WebSocket enabled: `server.enableWebSocket(true)`
2. Check browser console for errors
3. Verify no proxy blocking WebSocket upgrade

### Metrics Not Updating

1. Confirm CognitiveLoop is running
2. Check WebSocket connection status
3. Verify no JavaScript errors in console

## Example: Monitoring Script

```python
#!/usr/bin/env python3
import websocket
import json

def on_message(ws, message):
    data = json.loads(message)
    print(f"Atoms: {data['data']['atomCount']}, "
          f"Cycles: {data['data']['cycleCount']}")

def on_error(ws, error):
    print(f"Error: {error}")

def on_open(ws):
    print("Connected to cog0 dashboard")

ws = websocket.WebSocketApp("ws://localhost:8080/ws/metrics",
                            on_message=on_message,
                            on_error=on_error,
                            on_open=on_open)
ws.run_forever()
```

## Related Documentation

- [API_REFERENCE.md](API_REFERENCE.md) - Complete API docs
- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) - Production deployment
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues
