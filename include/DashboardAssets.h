/*
 * standalone/include/cog0/DashboardAssets.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Embedded HTML/JS dashboard assets for the MonitoringServer.
 *
 * The dashboard provides real-time visualization of:
 *   - Agent metrics (atom count, cycle count, uptime)
 *   - Active goals and tasks
 *   - System health status
 *
 * Served at GET /dashboard, connects via WebSocket to /ws/metrics.
 */

#pragma once

#include <cstddef>

namespace cog0 {

// Embedded dashboard HTML (single-page application with inline CSS/JS)
extern const char* DASHBOARD_HTML;
extern const size_t DASHBOARD_HTML_LEN;

// Dashboard favicon (16x16 PNG, base64-embedded in HTML)
extern const char* DASHBOARD_FAVICON;

} // namespace cog0
