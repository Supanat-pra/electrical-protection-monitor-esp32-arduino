Query 1 — View all telemetry last 30 minutes
    from(bucket: "electrical_protection")
    |> range(start: -30m)
    |> filter(fn: (r) => r._measurement == "telemetry")

Query 2 — Average load per minute
    from(bucket: "electrical_protection")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct")
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
  |> yield(name: "avg_load_per_minute")

Query 3 — Maximum temperature
    from(bucket: "electrical_protection")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "temp_c")
  |> max()
  |> yield(name: "max_temperature")

Query 4 — Current load and temperature (latest value)
    from(bucket: "electrical_protection")
  |> range(start: -5m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct" or
                       r._field == "temp_c")
  |> last()
  |> yield(name: "current_values")

Query 5 — Count TRIPPED/LOCKOUT events
    from(bucket: "electrical_protection")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "alert")
  |> filter(fn: (r) => r._field == "state")
  |> filter(fn: (r) => r._value == "TRIPPED" or
                       r._value == "LOCKOUT")
  |> count()
  |> yield(name: "trip_events")

Query 6 — Relay trip history (when relay switched)
    from(bucket: "electrical_protection")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "relay")
  |> filter(fn: (r) => r._value == 0)
  |> yield(name: "trip_history")

Query 7 — Load and relay together (for correlation graph)
    from(bucket: "electrical_protection")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct" or
                       r._field == "relay")
  |> pivot(
      rowKey: ["_time"],
      columnKey: ["_field"],
      valueColumn: "_value"
    )
  |> yield(name: "load_vs_relay")
