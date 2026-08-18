# CogZero Production Deployment Guide

This guide covers production hardening features introduced in **Phase 14**:
agent migration, real-time monitoring (WebSocket dashboard), optional TLS,
AgentService (gRPC/JSON), and Raft log persistence.

## 1. Recommended topology

```
                 ┌──────────────────────┐
   operators ───▶│ MonitoringServer     │  :8080  HTTP/HTTPS + /dashboard
                 │  + WebSocket /ws/*   │
                 └──────────┬───────────┘
                            │
                 ┌──────────▼───────────┐
   clients  ───▶│ GrpcAgentServer      │  :50051 JSON AgentService
                 │  (or real gRPC)      │
                 └──────────┬───────────┘
                            │
                 ┌──────────▼───────────┐
                 │ Agent + AtomStore    │
                 │ CognitiveLoop        │
                 │ RaftNode (optional)  │
                 └──────────────────────┘
```

For multi-node clusters, use `ClusterManager` migration APIs plus Raft leader
coordination. See [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md).

## 2. Build

```bash
cmake -S standalone -B build-standalone \
  -DBUILD_TESTING=ON \
  -DUSE_TLS=ON \
  -DUSE_GRPC=OFF
cmake --build build-standalone -j
ctest --test-dir build-standalone -R 'phase14|phase12' --output-on-failure
```

| Option | Default | Notes |
|--------|---------|-------|
| `USE_TLS` | `ON` | Enables OpenSSL when found (`COG0_HAVE_OPENSSL`) |
| `USE_GRPC` | `OFF` | Probe for real gRPC; JSON fallback always available |
| `USE_READLINE` | `ON` | Interactive CLI tab-completion |

## 3. MonitoringServer

### Endpoints

| Path | Type | Description |
|------|------|-------------|
| `/health` | GET JSON | Liveness + TLS flags |
| `/metrics` | GET JSON | Atom/cycle counters |
| `/atoms` | GET JSON | Atom listing |
| `/attention` | GET JSON | STI/LTI ranking |
| `/dashboard` | GET HTML | Embedded real-time UI |
| `/ws/metrics` | WebSocket | Push metrics (~500ms) |

Details: [DASHBOARD_GUIDE.md](DASHBOARD_GUIDE.md).

### TLS

Generate a certificate (example self-signed for lab use):

```bash
openssl req -x509 -newkey rsa:2048 \
  -keyout mon.key -out mon.crt -days 365 -nodes \
  -subj '/CN=cog0.example.com'
```

```cpp
cog0::MonitoringServer mon(store, loop, 8443);
if (!mon.enableTLS("mon.crt", "mon.key")) {
    // handle error — mon.tlsLastError()
}
mon.enableWebSocket(true);
mon.start();
```

`/health` reports:

```json
{
  "status": "ok",
  "uptime_s": 12.34,
  "tls_enabled": true,
  "tls_ready": true,
  "tls_available": true
}
```

**Production checklist**

- [ ] Use certificates from a trusted CA (or internal PKI)
- [ ] Restrict bind address via firewall / reverse proxy
- [ ] Prefer reverse proxy (nginx/Caddy) termination if central cert management is required
- [ ] Do not expose `/dashboard` on the public internet without authn/authz at the edge

## 4. AgentService (gRPC / JSON)

See [GRPC_GUIDE.md](GRPC_GUIDE.md).

```cpp
cog0::Agent agent;
cog0::GrpcAgentServer api(agent, 50051);
api.start();
```

Expose only on trusted networks or behind mTLS-capable proxies.

## 5. Agent migration

Serialize agent state and hand off between cluster nodes:

```cpp
std::string blob = agent.serialize();
// transport blob to target node
auto restored = cog0::Agent::deserialize(blob);
```

Cluster coordination:

```cpp
cluster.initiateMigration(sourceNode, targetNode, agentId);
cluster.receiveMigration(serializedState, sourceNode);
```

Full procedures: [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md).

## 6. Raft persistence

```cpp
auto store = cog0::createLogStore("memory");  // default
// Future: createLogStore("rocksdb", "/var/lib/cog0/raft");
```

The pluggable `RaftLogStore` interface supports durable backends; the standalone
default is in-memory. Persist Raft `term` / `votedFor` via `persistState` /
`loadState` on restart.

## 7. Security hardening checklist

- [ ] Enable TLS on MonitoringServer (`enableTLS`)
- [ ] Firewall AgentService and monitoring ports
- [ ] Run as non-root with minimal filesystem permissions
- [ ] Disable debug/verbose logging in production
- [ ] Validate migration payloads before `deserialize`
- [ ] Keep OpenSSL packages patched
- [ ] Prefer read-only mounts for certs; separate key permissions (`0600`)

## 8. Operations

### Health scrape

```bash
curl -fsS http://127.0.0.1:8080/health
# or with TLS:
curl -fsS --cacert mon.crt https://127.0.0.1:8443/health
```

### Dashboard

Open `http://<host>:8080/dashboard` (or `https://` when TLS is enabled).

### Suggested systemd unit (sketch)

```ini
[Unit]
Description=CogZero standalone agent
After=network.target

[Service]
Type=simple
User=cog0
ExecStart=/usr/local/bin/cog0 --batch
Restart=on-failure
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true

[Install]
WantedBy=multi-user.target
```

## 9. Troubleshooting

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| `enableTLS` returns false | OpenSSL missing or bad PEM paths | Install `libssl-dev`, verify cert/key |
| `/health` shows `tls_enabled:false` | TLS not configured before `start()` | Call `enableTLS` while stopped |
| gRPC client cannot connect | Wrong port / server not started | Check `server.port()` and firewall |
| Migration deserialize fails | Truncated/corrupt JSON | Verify transport integrity |
| WebSocket disconnects | Proxy idle timeout | Raise proxy read timeout; keepalives |

## 10. Related docs

- [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)
- [DASHBOARD_GUIDE.md](DASHBOARD_GUIDE.md)
- [GRPC_GUIDE.md](GRPC_GUIDE.md)
- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)
