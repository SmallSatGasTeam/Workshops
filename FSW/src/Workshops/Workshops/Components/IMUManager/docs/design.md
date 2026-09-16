# Component::IMUManager Design

## 1. Introduction

This is `IMUManager`'s first design document — until now the component was
documented only at the implementation level (`docs/sdd.md`). It covers the
component's full current design, not just the change this document
introduces: the existing BNO055 interface and self-configuration state
machine (already implemented and unchanged by this design), and the new
work — making `IMUManager` the project's second F' Data Products producer
(`Svc.DpManager`/`Svc.DpWriter`/`Svc.DpCatalog`), following `EPSManager`
(`Components/EPSManager/docs/design.md`). It explains *why* each piece is
shaped this way; function bodies and exact byte offsets belong in
`docs/sdd.md`, not here.

**Implementation status: design only.** Nothing in §4 is built yet. This
document is the settled output of a `design-discussion` session and is the
brief for implementation, not a record of it — unlike `EPSManager`'s design
doc, which was updated after the fact to report a passing build and test
suite, this one predates any code change.

The cached-poll telemetry path (`dataRequest` → I2C read → 22 telemetry
channels, all pre-existing) and the new recording path are independent, the
same relationship `EPSManager`'s design settled for itself: one serves
whatever polls `IMUManager`'s telemetry today, the other serves ground
history. Neither depends on the other.

This design also introduces one new concept outside `IMUManager` itself: a
latched `hasGroundContact` bool on `FlightLogic`, needed to satisfy
`FSW-CDH-530-3`'s two-phase sample cadence (§4.3). `FlightLogic`'s own
design doc (`Components/FlightLogic/docs/design.md` §4.9) now covers this
mechanism from its side; §4.3/§4.4 here describe `IMUManager`'s dependent
half of the same interface and are otherwise unchanged.

The accumulate/flush mechanism itself (§4.6, §5) is the same general C&DH
pattern `EPSManager` established and `fprime-patterns.md` §10 documents
generically — this document does not re-derive it, only what's specific to
`IMUManager`'s instance: its record shape, its cadence, and the one new
piece of architecture the cadence requires.

## 2. Role

`IMUManager` owns talking to the BNO055 over I2C: self-configuring it into
NDOF (9-DOF fusion) mode, reading all 22 sensor fields once per
`dataRequest` tick, and publishing them to telemetry. This design adds a
second responsibility: sampling those same already-read values on its own
cadence, packing them into a Data Products container, and handing the
container to `Svc.DpManager` once it fills. `IMUManager` does **not** know:

- How or when the ground actually downlinks the recorded file — that is
  `Svc.DpCatalog`'s job, triggered by explicit ground command
  (`BUILD_CATALOG`, `START_XMIT_CATALOG`), unaffected by this design.
- *Why* ground contact matters or how it's detected — `FlightLogic` decides
  that and hands `IMUManager` only the resulting bool (§4.3, §4.4).
  `IMUManager` stores and acts on the value it's given; it has no opinion on
  what "first ground communication" means.
- How other producers (`EPSManager`, `TemperatureSensorManager`,
  `FlightLogic`, `TransceiverConfigurationManager`,
  `SBandTransceiverConfigurationManager`) record their own telemetry — each
  implements this pattern independently against its own container/record
  types (`EPSManager`'s design.md §4.1; not re-argued here).
- The full six-producer priority scheme — only its own settled value (30,
  §4.7).
- How large the shared `Svc.BufferManager` pool is or should be — it lives
  within the existing 10,000-byte/10-buffer pool `EPSManager` already uses,
  not a value this component chooses (§4.6).

## 3. Related Requirements

| Requirement | Text | Design status |
|---|---|---|
| `FSW-CDH-500-3` | IMUManager shall read all IMU fields (accelerometer, magnetometer, gyroscope, Euler, quaternion, linear acceleration, gravity, temperature) over I2C on each dataRequest tick and publish them to their telemetry channels. | Satisfied by existing architecture, unaffected by this design — the pre-existing cached-poll telemetry path (`sdd.md` §2, §6). |
| `FSW-CDH-530-3` | IMUManager shall sample and record attitude, determination, and magnetic-field data once every 1 second plus or minus 5 ms from ejection until first ground communication, then once every 1 minute plus or minus 5 seconds thereafter. | `open` in `requirements.csv` — moves to satisfied by this design: §4.3/§4.4 for the two-phase cadence, §4.5 for choosing to record all 22 fields (a superset of the "attitude, determination, and magnetic-field" wording, so the requirement's literal minimum is covered without narrowing the record). Recommend reparenting from `FSW-CDH-500-3` to `FSW-CDH-013-2` once implemented, mirroring `FSW-EPS-180-3`'s precedent (`EPSManager`'s design.md §3) — flagged here, not executed; a `requirements.csv` edit is `retiring-requirements`'s job. |
| `FSW-CDH-013-2` | Each component responsible for capturing telemetry or sensor data shall accumulate its readings into data product containers up to 10KB and queue each full container to DpManager for ground downlink. | Implemented by this design — `IMUManager` is its second implementation, after `EPSManager` (§4.6). |
| `FSW-CDH-111-3` | FlightLogic shall track time since the last valid ground command and declare a command-loss fault after COMMAND_LOSS_TIMEOUT_S. | Not `IMUManager`'s requirement and not modified by this design — listed here because §4.3's `hasGroundContact` latch reuses the exact same choke point (`FlightLogic::notifyCommandHeard()`) that resets this timer's countdown. The two responsibilities share a call site but not any state; this design changes neither the timer's behavior nor this requirement's status. |

## 4. Design

### 4.1. IMUManager records itself; no shared recorder component

Same decision `EPSManager`'s design settled and does not need re-arguing
here (`EPSManager`'s design.md §4.1): a dedicated cross-subsystem recorder
component would just be polling a poll, since `IMUManager` already refreshes
its own cached values every tick. `IMUManager` owns writing its own data
products directly, the same way it already owns its own I2C register map.

### 4.2. Component kind: no conversion needed

`EPSManager` had to convert from `active` to `passive` before it could take
on this pattern. `IMUManager` does not: it is already `passive`, already has
a `sync input port dataRequest: Svc.Sched` running directly on whatever
rate-group thread schedules it, and already has no ping ports to remove
(confirmed against the current `.fpp` — `fprime-patterns.md`'s Health
Checking Pattern section already cites `IMUManager` as one of the two
existing precedents, alongside `TemperatureSensorManager`, for a passive
data-acquisition manager with no `Svc.Health` participation). The
accumulation mechanism in §4.6 is verified safe regardless of caller thread
(`fprime-patterns.md` §10), so nothing about `IMUManager` already being
passive changes how it's used.

### 4.3. New concept: `FlightLogic`'s `hasGroundContact` latch

`FSW-CDH-530-3` needs a way to know whether ground has *ever* contacted the
satellite — nothing in the codebase tracks this today. The nearest existing
mechanism is `FlightLogic::notifyCommandHeard()`: every one of
`FlightLogic`'s five command handlers (`takePic`, `confirmDownlink`,
`resetFlags`, `FORCE_SAFE`, `RELEASE_SAFE`) already calls it unconditionally
on entry, today solely to forward a reset signal to `FaultManager`'s
command-loss timer (`FSW-CDH-111-3`).

This design adds one bool member to `FlightLogic`, `hasGroundContact`, set
`true` inside `notifyCommandHeard()` the first time it runs. No new port
into `FlightLogic` and no new trigger logic — every existing call site
already routes through this one function.

**It is not cleared by `RESET_FLAGS`.** `resetFlags_cmdHandler` already
leaves `FaultManager`'s reboot-storm history untouched, with the existing
comment "survives this reset" — the same reasoning applies here: whether
ground has ever contacted the satellite is a mission-history fact, not
simulator/test state `RESET_FLAGS` exists to rewind. It would also be
self-contradictory: `RESET_FLAGS` is itself a ground command, so the
handler that just proved contact is happening cannot also erase the record
of it in the same call.

### 4.4. New port: `FlightLogic.setGroundContactEstablished -> IMUManager.setGroundContactEstablished`

`FlightLogic` forwards `hasGroundContact`'s current value to `IMUManager`
every tick (level-driven — resent every cycle regardless of whether it
changed, the same convention `setHeater`/`setChargeInhibit`/`setSensorRail`
already use — not edge-triggered, so there is no separate "first time only"
signal path to get wrong). This reuses an existing port type,
`FL.setSwitch (enabled: bool)` (`Ports/Ports.fpp`) — the same type
`setHeater`/`setChargeInhibit`/`setSensorRail` already use — so no new port
type is defined:

```fpp
# FlightLogic.fpp
output port setGroundContactEstablished: FL.setSwitch

# IMUManager.fpp
guarded input port setGroundContactEstablished: FL.setSwitch
```

`guarded` because `FlightLogic` and whatever rate-group thread schedules
`IMUManager`'s `dataRequest` are different threads — the same cross-thread
protection `EPSManager`'s actuation ports use (`EPSManager`'s design.md
§4.2). This is the first instance of "`FlightLogic` gates a producer's DP
cadence" in the codebase; it is not assumed to generalize to other
producers by name until one of them actually needs it (§6).

### 4.5. Two-phase sampling cadence

`IMUManager` already ticks at whatever rate `dataRequest` is wired to
(1 Hz in `FSWDeployment`'s `rateGroup1`, 0.25 Hz in `Simulate`'s
`rateGroup3` — a pre-existing mismatch between the two deployments,
unrelated to this design and not fixed by it). Following `EPSManager`'s
tick-counter pattern (`EPSManager`'s design.md §4.3), a bounded member
counter gates one Data Products sample per interval — except here the
active interval is chosen per tick from two named constants instead of one:

```cpp
const U32 interval = this->hasGroundContact
    ? IMU_DP_SAMPLE_INTERVAL_SLOW_S   // = 60
    : IMU_DP_SAMPLE_INTERVAL_FAST_S;  // = 1
if (++this->m_dpTickCount >= interval) {
    this->m_dpTickCount = 0;
    this->sampleDataProducts();
}
```

`this->hasGroundContact` here is `IMUManager`'s own cached copy of the value
last received over §4.4's port — a plain member set by the guarded handler,
read by the tick handler, no different in kind from any other cross-thread
cached value already in this codebase. This directly satisfies
`FSW-CDH-530-3`'s two cadence phases without a second scheduling port or
rate-group divisor: 1 Hz (matching phase one) until the first tick where
`hasGroundContact` reads `true`, 1-per-60 (matching phase two, and matching
`EPSManager`'s own cadence) from then on. The switch is one-directional in
practice — `hasGroundContact` only ever latches `true` (§4.3) — so this
counter can only ever speed up once and then stay slow; there is no path
back to the fast phase.

### 4.6. Fixed-size container, check-before-append

Identical mechanism to `EPSManager`'s design (`EPSManager`'s design.md
§4.4) — check `getPacketSize()` plus one record's size against the byte cap
*before* serializing, flush (`dpSend`) and reopen (`dpGet`) only when it
would overflow — not re-derived here. Two points specific to `IMUManager`:

- Every `dpGet_ImuTelemetryContainer` call must request
  `IMU_DP_CONTAINER_MAX_BYTES - Fw::DpContainer::MIN_PACKET_SIZE`, not the
  raw cap — the same padding fix `EPSManager` needed, caught there only by
  a unit test exercising the success path, not by inspection
  (`fprime-errors.md`). This design states it up front rather than
  rediscovering it during implementation.
- The byte cap is not `IMUManager`'s to choose. `Svc.BufferManager` serves
  the whole deployment from one bin size (`dpBufferStoreSize = 10000`,
  `dpBufferStoreCount = 10`, confirmed in
  `DataProductsConfig.fpp` — a single bin, not one per producer or per
  container type). `IMU_DP_CONTAINER_MAX_BYTES` is therefore fixed at 10000,
  the same as `EPSManager`'s, because both draw from the same pool.

### 4.7. Record shape: all 22 fields, one struct, per-sample timestamp

```fpp
struct ImuTelemetrySample {
    timeSeconds: U32,
    accX: F32, accY: F32, accZ: F32,
    magX: F32, magY: F32, magZ: F32,
    gyrX: F32, gyrY: F32, gyrZ: F32,
    eulX: F32, eulY: F32, eulZ: F32,
    quaX: F32, quaY: F32, quaZ: F32,
    liaX: F32, liaY: F32, liaZ: F32,
    grvX: F32, grvY: F32, grvZ: F32,
    temp: I8
}

product record ImuTelemetrySampleRecord: ImuTelemetrySample id 0
product container ImuTelemetryContainer id 0 default priority 30
```

`timeSeconds: U32`, filled from `this->getTime().getSeconds()` at the point
of serialization, not `Fw.Time` — `Fw.Time` is an opaque `type` and
`fpp-to-dict` rejects it as a `product record` field
(`error: type of record is not displayable`, `fprime-errors.md`); this
design states the correct field type from the start rather than repeating
`EPSManager`'s original mistake.

All 22 telemetry fields are carried, not the narrower "attitude,
determination, and magnetic-field" subset `FSW-CDH-530-3`'s wording names —
matching `FSW-CDH-500-3`'s "all IMU fields" and `EPSManager`'s precedent of
recording everything already cached rather than a curated subset. One
struct, not several record types, keeps per-sample code to one
`serializeRecord_ImuTelemetrySampleRecord` call, the same as `EPSManager`.

**Capacity, estimated, not yet confirmed by a build:** the struct serializes
to roughly 4 (`timeSeconds`) + 84 (21 `F32` fields) + 1 (`temp`) = 89 bytes,
plus `EpsTelemetrySampleRecord`'s confirmed 4-byte record-ID overhead
(`EPSManager`'s design.md §4.4) suggests ~93 bytes/record — call it ~90-95
bytes pending the actual `SIZE_OF_ImuTelemetrySampleRecord_RECORD` value
once built. At the shared 10,000-byte cap that puts roughly 100-110 records
per container: at the slow (60 s) cadence, one file roughly every 1.5-2
hours, the same order of magnitude as `EPSManager`'s ~5 files/day; at the
fast (1 s) cadence — active only until first ground contact — one file
roughly every 2 minutes, by design, since dense data right after ejection
is the point of the fast phase.

`default priority 30` is `IMUManager`'s assigned value in the settled
six-producer ordering (issue #33; `EPSManager`'s design.md §4.5 has the
full scheme). `IMUManager` only needs its own value, not the other four.

### 4.8. Port wiring and producer index

```fpp
product get port productGetOut
product send port productSendOut
```
```fpp
# FSWDeployment/Top/topology.fpp and Simulate/Top/topology.fpp
imuManager.productGetOut -> DataProducts.dpMgr.productGetIn[1]
imuManager.productSendOut -> DataProducts.dpMgr.productSendIn[1]
```

`IMUManager` is the second producer wired (`EPSManager` took index 0), so
unlike `EPSManager`, this design owns the two pieces `EPSManager`'s design
doc left open for whoever came next (`EPSManager`'s design.md §7 Q1):

- **`DpManagerNumPorts` bump.** Defaults to 5 (`fprime/default/config/AcConstants.fpp`).
  Settled at exactly **6**, once, project-wide — matching the fixed
  six-producer roster with no extra headroom, since `CameraManager` is
  explicitly not a future DP producer (deferred to plain-file storage) and
  nothing else is expected to need a slot (settled during
  `TemperatureSensorManager`'s design, `TemperatureSensorManager`'s design.md
  §4.6). Bumped via a project `AcConstants.fpp` config override, the same
  pattern `SBandComCcsdsConfig/AcConstants.fpp` already established for
  `ComQueueBufferPorts` (`fprime-errors.md`) — copy every current constant's
  value forward and change only this one, since overrides apply whole-build,
  not per-module.
- **Forwarding connections.** The imported `DataProducts.Subtopology` only
  wires index 0's `bufferGetOut`/`productSendOut` internally ("explicit
  port indexes for demo," per its own comment). Index 1 needs the same two
  connections added directly in each deployment's own `topology.fpp` (no
  vendored-file edit):
  ```fpp
  DataProducts.dpMgr.bufferGetOut[1] -> DataProducts.dpBufferManager.bufferGetCallee
  DataProducts.dpMgr.productSendOut[1] -> DataProducts.dpWriter.bufferSendIn
  ```

`dpGet`/`dpSend` are safe regardless of caller thread (verified against
`DpManager`'s actual implementation, `fprime-patterns.md` §10), so
`IMUManager` already being passive (§4.2) has no bearing on this wiring.

### 4.9. Reboot loses the in-progress container

Same inherited limitation as `EPSManager`'s design (`EPSManager`'s design.md
§4.7, §7 Q2), not re-argued or re-decided here: a container lives only in
RAM until `dpSend()` hands it off, so a reboot mid-accumulation loses
whatever samples it held. Accepted as-is, same as `EPSManager`.

## 5. Algorithmic State Machine

*(Placeholder — diagram not yet supplied.)*

This component's actual state machine has two axes that both need to appear
in one diagram (not two separate diagrams, per this component's single
actual state machine):

1. **Initialization/configuration** (already implemented, currently only
   described as prose in `sdd.md` §7, never drawn): `Uninitialized` →
   `config()` attempts (power-mode check, operation-mode check, system
   status/error check) → success moves to `Initialized`; failure increments
   `configFails`, and after `MAX_CONFIG_FAILS` triggers a device reset and
   increments `resets`; after `MAX_RESETS`, the component stops attempting
   configuration permanently (no further transitions out of that terminal
   condition).
2. **Data Products accumulation** (new, this design): once `Initialized`,
   each sample-due tick runs the same `Accumulating`/`Closed` shape
   `EPSManager`'s diagram already shows (`EPSManager`'s design.md §5) —
   check current packet size plus one record against the 10,000-byte cap
   before appending; flush and reopen if it would overflow; retry `dpGet`
   on the next sample-due tick if opening ever fails.

The diagram needs to show these as nested, not parallel: the DP sub-machine
only runs at all while in `Initialized`, and within it, whether a given
tick is "sample-due" is gated by the counter-vs-interval check in §4.5 — a
guard condition on the sample-due transition, not a separate state, since
`hasGroundContact` (received asynchronously via the guarded
`setGroundContactEstablished` port, §4.4) only changes *which threshold* the
same counter compares against, never adding a distinct mode of operation.

## 6. What this design deliberately does not do

- Does not introduce a shared/generic recorder component (§4.1).
- Does not convert `IMUManager`'s component kind — it was already `passive`
  before this design and needed no change (§4.2), unlike `EPSManager`.
- Does not generalize `hasGroundContact`/`setGroundContactEstablished` to
  any other producer. Only `IMUManager` is wired to it; if
  `TemperatureSensorManager` or another future producer needs the same
  phase-gating later, that is its own design decision, not an automatic
  consequence of this one (§4.3, §4.4).
- Does not slow down or otherwise change `IMUManager`'s existing telemetry
  publish rate — that stays tied to whatever `dataRequest` ticks at,
  independent of the Data Products sample cadence (§4.5).
- Does not fix the existing `FSWDeployment`-vs-`Simulate` rate-group
  frequency mismatch for `dataRequest` (1 Hz vs. 0.25 Hz) — pre-existing,
  out of scope here.
- Does not resize or otherwise reconfigure the shared `Svc.BufferManager`
  pool — lives within `EPSManager`'s existing 10,000-byte/10-buffer pool
  as-is (§4.6).
- Does not wire port-forwarding or bump `DpManagerNumPorts` beyond what
  index 0 (`EPSManager`) and index 1 (`IMUManager`) need — the remaining
  four producers still need their own capacity and wiring when their turn
  comes (§4.8).
- Does not persist an in-progress container across a reboot (§4.9).
- Does not automate ground downlink triggering — unchanged from
  `EPSManager`'s design; `BUILD_CATALOG`/`START_XMIT_CATALOG` remain
  explicit ground-commanded actions via the existing
  `downlink_data_products.seq`.
- Does not edit `requirements.csv` — the `FSW-CDH-530-3` reparenting
  recommendation (§3) is flagged for `retiring-requirements`, not applied
  by this document.

## 7. Open design questions

1. **In-progress container loss on reboot.** Inherited from `EPSManager`'s
   design (§4.9); whether this is worth mitigating for either producer is
   still open, not decided or re-decided here.
2. **`Svc.BufferManager` pool headroom.** Only 10 buffers exist for the
   whole deployment (`dpBufferStoreCount = 10`). Two producers (`EPSManager`,
   `IMUManager`) each holding at most one open container at a time is no
   concern; whether headroom is still sufficient once all six producers are
   online (up to 6 concurrently open containers, plus whatever `DpWriter`
   holds in flight) is worth checking when the later producers are
   designed — not a gap in this design, just a forward-looking note.
3. **`FSW-CDH-530-3` reparenting.** Recommended (§3) to move from parenting
   `FSW-CDH-500-3` to parenting `FSW-CDH-013-2`, mirroring
   `FSW-EPS-180-3`'s precedent — not executed here; a `requirements.csv`
   wording/parent change is `retiring-requirements`'s job.

## 8. References

- Issue #33: the settled six-producer priority scheme (`FlightLogic` 10 →
  `EPSManager` 20 → `IMUManager` 30 →
  `SBandTransceiverConfigurationManager` 40 →
  `TransceiverConfigurationManager` 50 → `TemperatureSensorManager` 60) and
  the `CmdSequencer`-based downlink-trigger decision.
- `docs/requirements/requirements.csv`: `FSW-CDH-500-3`, `FSW-CDH-530-3`,
  `FSW-CDH-013-2`, `FSW-CDH-111-3`.
- `.agents/context/fprime-patterns.md` §10 — the general Data Products
  Accumulating Producer pattern this design implements; not re-derived
  here.
- `.agents/context/fprime-errors.md` — the `dpGet` buffer-padding gotcha,
  the `Fw.Time`-not-displayable gotcha, and the `DpManagerNumPorts`/
  port-wiring gap this design's §4.8 resolves for index 1.
- `Components/EPSManager/docs/design.md` — the sibling design this one
  follows directly: §4.1 (no shared recorder), §4.4 (check-before-append,
  buffer-padding fix), §4.5 (priority scheme, timestamp field type), §7 Q1
  (the port-wiring/capacity gap this design's §4.8 resolves for the second
  producer).
- `Components/EPSManager/docs/sdd.md` — `guarded`/cross-thread port
  precedent cited in §4.4.
- `Ports/Ports.fpp` — `FL.setSwitch`, the existing port type reused in §4.4.
- `Components/FlightLogic/FlightLogic.cpp`/`.hpp` — `notifyCommandHeard()`
  and the five command handlers that call it (§4.3).
- `Components/FlightLogic/docs/design.md` §4.9 — `FlightLogic`'s own side
  of the `hasGroundContact`/`setGroundContactEstablished` interface.
- `Components/IMUManager/docs/sdd.md` — current implemented behavior for
  everything not changed by this design (BNO055 register map, NDOF config
  sequence, telemetry channel list); will need updating alongside
  implementation, not by this document.
- `fprime/docs/user-manual/framework/data-products.md` — producer port/API
  reference (`dpGet`, `dpSend`, `serializeRecord_*`, `SIZE_OF_*_RECORD`).
- `fprime/Ref/DpDemo/` — reference producer component implementation.
