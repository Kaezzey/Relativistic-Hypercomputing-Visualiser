#pragma once

#include "models/BootTelemetry.h"

namespace rhv::ui
{
void DrawBootStatusBlock(const models::BootTelemetry& telemetry, bool includeScopeNote = false);
}  // namespace rhv::ui
