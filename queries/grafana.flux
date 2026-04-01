Panel 1 — Load % Gauge
from(bucket: "electrical_protection")
  |> range(start: -5m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct")
  |> last()

Panel 2 — Temperature Gauge
from(bucket: "electrical_protection")
  |> range(start: -5m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "temp_c")
  |> last()

Panel 3 — System State (Stat panel)
from(bucket: "electrical_protection")
  |> range(start: -5m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "state")
  |> last()

Panel 4 — Load % Over Time (Line graph)
from(bucket: "electrical_protection")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct")
  |> aggregateWindow(every: 10s, fn: mean, createEmpty: false)

Panel 5 — Temperature Over Time
from(bucket: "electrical_protection")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "temp_c")
  |> aggregateWindow(every: 10s, fn: mean, createEmpty: false)

Panel 6 — Relay Status Over Time (Step graph)
from(bucket: "electrical_protection")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "relay")