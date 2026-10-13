# Motor Driver HIL Test Requirements

**Status:** Draft for engineering review — requirements and acceptance limits have not yet been approved.  
**Project:** STM32 Nucleo-G431KB / Zephyr / DRV8871 brushed DC motor driver.  
**Prepared:** 2026-10-12.  
**Execution status:** Not run. Functionality under development remains work in progress; this document defines the desired verification baseline, not a claim of implementation or test completion.

## 1. Objective and scope

The motor driver shall execute authorized motion commands predictably, maintain bounded speed and electrical output within its declared operating envelope, stop within defined limits, and respond deterministically to communication, feedback, controller, and power faults without unintended restart.

These are proposed engineering requirements for this project, not a claim of compliance with a particular safety or product standard. Applicable certification, EMC, environmental, mechanical guarding, and system-level safety requirements depend on the final product and require separate qualification.

Two complementary test methods are required:

- **H — Controller HIL:** The real MCU, production firmware, CAN transceiver, timers, and I/O operate against a real-time motor/load model. The simulator measures PWM and returns quadrature A/B feedback. Timing and fault-injection tests run through physical interfaces. An open-loop encoder signal generator alone is sufficient for input characterization but does not verify closed-loop dynamics.
- **P — Powered motor/load bench:** The actual driver board, motor, encoder, supply, and controlled mechanical load operate together. Independent instruments measure current, bus voltage, shaft speed, and temperature. This verifies physical performance that controller HIL cannot establish.

A power-HIL replacement for P tests is acceptable only where its power interface, energy absorption, and model fidelity are validated for the specific measurement. Semiconductor protection tests do not by themselves establish safe system behavior.

## 2. Project baseline

Freeze these settings with the tested firmware revision. They are current design/configuration values, not approved product ratings or demonstrated performance.

| Item | Current project baseline |
| --- | --- |
| Controller | STM32G431KB, Nucleo-G431KB, Zephyr C application |
| Power stage | DRV8871, controlled by TIM4 PWM on PB6/PB7 |
| PWM target | 90 kHz in `src/PWM/pwm.c`; measure actual frequency and usable duty range |
| Encoder | TIM3 A/B on PA6/PA7; 2,448 counts per output revolution in the device-tree overlay |
| CAN FD | 500 kbit/s arbitration; 2 Mbit/s data; BRS transmission intended |
| Addressing | Central CAN identifier `0x64`; local payload target `0x65`; broadcast payload target `0xFF` |
| Command payload | 12-byte CAN FD frame, 11 meaningful bytes; action, angular-speed data, and target counter ticks |
| Control-loop setting | `PD_REFR_US = 100` microseconds; this sleep setting is not evidence of a measured 100-microsecond loop period |
| Watchdog setting | `PID_WD_TIMEOUT_mS = 1`; this is not an approved command-loss deadline |
| State model | IDLE, COUNTER_RESET, MOVE, GET_TICKS, ERROR |
| Indicators | PA4 timer heartbeat and WS2812 state indication |

The wire-level command specification shall be frozen before protocol tests: command bytes, accepted DLC/flags/IDs, units, numeric range, direction/sign convention, tick reference and wrap rules, response format, and rejection behavior. Tests must not infer intended behavior solely from current firmware.

## 3. Acceptance parameters to approve

**TBD means not yet testable for acceptance.** Characterization may determine candidate values, but a test cannot be marked Pass until its limits and expected behavior are fixed. Do not substitute a component's peak or absolute maximum rating for the assembled product rating.

| Parameter set | Required definition | Status |
| --- | --- | --- |
| E — Electrical envelope | Minimum/nominal/maximum supply voltage; continuous current; peak current plus duration/duty cycle; current-limit tolerance; permitted regenerative bus rise; absolute bench-abort limits | TBD |
| M — Mechanical envelope | Speed range in rad/s; torque/load/inertia range; gearbox limits; direction convention; permissible motion while disabled; maximum stop time and travel for each load case | TBD |
| C — Control quality | Steady-state absolute speed error near zero and relative error above a defined reference speed; averaging window; settling band/time; overshoot; reversal/ramp limits; disturbance-recovery time | TBD |
| T — Timing | Boot-ready deadline; command-to-output latency; STOP-to-safe-output deadline; scheduled-start tolerance and minimum lead time; loop period/jitter; maximum permitted deadline misses | TBD |
| F — Fault response | Command-loss timeout; each fault's detection and safe-output deadline; encoder/stall thresholds; watchdog timeout; communication recovery deadline | TBD |
| S — Safe-state policy | Coast, brake, or controlled stop then disable for each fault; allowed electrical outputs during stopping; restart authorization; queue cancellation; behavior on reset/power loss | TBD |
| B — Bus and queue envelope | Supported command rate, burst length, bus utilization, response deadline, scheduling horizon, overflow policy, repeated/late command behavior | TBD |
| Q — Qualification matrix | Supply and ambient-temperature corners; load profiles; board/motor sample count; repetitions; soak duration; allowable drift; logging configuration | TBD |
| U — Measurement budget | Instrument accuracy, capture resolution, synchronization error, plant-model error, and decision guard bands for each measured limit | TBD |

Proposed restart policy: after a protective stop, power restoration, or controller reset, motion remains inhibited until the fault is cleared and a fresh valid authorization/command is received. Automatic resumption of a stale command shall not be allowed. Review this policy with the application's mechanical hazards before approval. Coast and brake are not interchangeable; zero PWM duty is not, by itself, a definition of a safe electrical state.

## 4. Test fixture and evidence requirements

- Use a time-synchronized CAN FD interface, encoder simulator, and digital capture for PWM and event timing. Timestamp command reception on the bus and physical output changes using a common timebase; host application timestamps alone are insufficient for hard timing limits.
- For H tests, measure the effective two-input drive command, including inversion, direction, brake/coast behavior, and saturation. Validate the real-time model against representative motor/load measurements. Include resistance, inductance, inertia, friction, load torque, gearing, and encoder quantization as applicable. Record model version, step size, I/O delay, jitter, and any simulator overruns. Runs with an invalid timing budget are inconclusive.
- For P tests, use a current-limited supply, voltage/current capture, independent shaft-speed measurement, thermal measurement, and a guarded load fixture with an independent energy-removal control. Capture stopping transients and regenerated energy; ensure the supply or a separate absorber can handle them.
- Measure MCU signals and driver inputs where practical, and verify driver/motor output separately. Firmware logs or RGB indication alone cannot prove a safe motor output.
- Provide controlled fault insertion for CAN interruption, encoder faults, supply ramps/dips, and controller stalls. Use rated fixtures and predetermined current/temperature/energy cutoffs for stall or power-stage fault tests; do not use uncontrolled live shorts. Use low-energy or simulated faults first.
- The DRV8871 exposes no dedicated fault-report output in its listed pinout. Firmware detection requirements for current/temperature/power-stage faults therefore need a declared observation method or additional sensing. Record unavailable detection as work in progress, rather than passing a test based only on the IC's protection behavior. Its protection and automatic recovery are described in the [TI datasheet, sections 5 and 7](https://www.ti.com/lit/gpn/drv8871).
- Fault hooks may be used for otherwise inaccessible failures. Record them and repeat the relevant nominal/timing tests using the production build without debugger-dependent behavior.

## 5. Verification requirements

Each row is a proposed requirement and its acceptance test. H/P indicates the required method; “H + P” needs both simulated coverage and representative physical validation. Referenced parameter sets in section 3 must be approved before execution for acceptance.

### Startup, actuation, and normal control

| ID | Requirement and stimulus | Pass criteria / evidence | Method |
| --- | --- | --- | --- |
| HIL-001 | The driver shall initialize without unintended motion. Apply power, MCU reset, and repeated power cycles with the host absent, present, and transmitting. Exercise MCU/power-stage supply sequencing. | Measured inputs and motor output remain within S/M limits until initialization is complete and fresh authorization is accepted; ready state reached within T. Scope capture includes the reset interval before firmware runs. | H + P |
| HIL-002 | The driver shall decode positive, zero, and reverse commands consistently with the approved direction/sign convention. Exercise minimum, mid-range, and maximum supported speeds in both directions. | Requested direction, measured shaft direction, and feedback sign agree; zero command implements the defined S behavior; speed stays within M/C limits. | H + P |
| HIL-003 | The driver shall produce valid PWM and drive-mode signals over the supported output range. Sweep duty requests, including minimum nonzero and near-maximum values, and verify coast/brake/direction modes. | Frequency, duty, pulse widths, and driver output match the approved electrical specification; no unintended direction pulse. Check the DRV8871 input timing and truth table, not just MCU duty percentages. [TI datasheet, sections 6.3 and 7.3.1](https://www.ti.com/lit/gpn/drv8871). | H + P |
| HIL-004 | The controller shall track speed commands under representative load and supply conditions. Apply steady commands and speed steps. | Steady error, settling time, overshoot, current, and output stay within C/E/M. Independent shaft-speed measurement corroborates P results. | H + P |
| HIL-005 | The controller shall recover from load disturbances and output saturation. Apply/remove load, hold an unreachable setpoint within the fixture limits, then command a reachable speed or STOP. | Recovery meets C; output/current remain bounded by E; saturation does not prevent the required stop response or cause a post-release speed excursion beyond C/M. | H + P |
| HIL-006 | Direction reversal shall follow the approved ramp/braking policy. Reverse at the defined speed/load corners. | Reversal timing, current, regenerated voltage, and speed excursion remain within C/E/M; no uncontrolled reversal or restart. | H + P |
| HIL-007 | STOP shall cancel pending motion and bring the motor to the specified stop condition. Issue STOP while idle, before a scheduled start, while moving, during a query, and under command load. | Safe electrical output reached within T; shaft speed falls below the M stop threshold within the allowed stop time/travel; queued or delayed commands do not restart motion without fresh authorization. | H + P |

### CAN protocol, state transitions, and timing

| ID | Requirement and stimulus | Pass criteria / evidence | Method |
| --- | --- | --- | --- |
| HIL-008 | Only supported, correctly addressed commands shall affect operation. Send valid local/broadcast commands and frames with wrong source ID, wrong target, unsupported frame format, or incorrect length. | Valid commands produce the specified behavior; rejected traffic changes neither setpoint nor scheduled motion. Rejection handling follows B and never causes unintended actuation. | H |
| HIL-009 | Malformed payloads shall be handled deterministically. Exercise unknown actions, truncated payloads, boundary/out-of-range speeds, invalid float encodings including NaN/infinity, and reserved fields. | Rejection or bounded handling matches the approved protocol; no uncontrolled output, memory corruption, reset, or lockup; malformed commands do not refresh motion authorization. | H |
| HIL-010 | The driver shall handle supported command bursts and resource exhaustion predictably. Increase traffic through B limits and beyond them; interleave MOVE, STOP, queries, and broadcast traffic. | Within B: no lost accepted commands and response/STOP deadlines met. Beyond B: declared overflow policy activates, output stays bounded, and STOP or command-loss protection remains effective. | H |
| HIL-011 | Accepted command data shall remain associated with the correct operation. Rapidly alternate distinct speed, direction, and target-tick payloads while earlier operations remain pending. | Observed action ordering and values match the approved queue/replacement policy; no payload mixing, obsolete execution after STOP, or duplicate actuation. | H |
| HIL-012 | Scheduled motion shall use the approved timebase. Test future, too-near, past, duplicate, rescheduled, and wraparound target ticks; reset the counter with an alarm pending. | Valid starts satisfy T scheduling tolerance with no early motion beyond that tolerance. Rejected or canceled schedules never actuate. Counter reset invalidates/rebases pending work exactly as specified. | H |
| HIL-013 | Telemetry shall return valid, timely information without disrupting motion. Request counter/encoder/status responses during idle and motion, including TX congestion. | Response contents, byte order, freshness, and deadline match B/protocol; no unintended stop, setpoint change, or missed control deadline. Unimplemented response types remain work in progress. | H + P |
| HIL-014 | State/event behavior shall match an approved transition matrix. Exercise every supported state/event pair, including ignored/rejected events and events arriving during entry/exit actions. | State, physical output, scheduled work, and response match the specification. WIP paths in the design remain pending, not accepted behavior. A telemetry query must preserve commanded motion unless the product specification explicitly requires otherwise. | H |
| HIL-015 | Real-time deadlines shall hold under the maximum supported combined load. Combine motor control, CAN RX/TX, timer alarms, and production logging. | Measured loop period/jitter, command latency, and deadline-miss count satisfy T throughout Q duration. Report maximum and distribution, not only averages. | H + P |

### Feedback, faults, and restart

| ID | Requirement and stimulus | Pass criteria / evidence | Method |
| --- | --- | --- | --- |
| HIL-016 | Quadrature acquisition shall preserve count and direction across the operating range. Inject known A/B sequences, reversals, counter rollover, and maximum-rate edges. | Decoded count equals injected count within the declared capture uncertainty; angular scaling agrees with 2,448 counts/revolution or the approved revision. Velocity estimation meets C over the defined sampling window. | H |
| HIL-017 | Feedback loss or implausibility shall trigger the declared response. Inject frozen A/B, disconnected channels, stuck levels, excessive edge rate, reversed feedback, and intermittent pulses. | Each detectable fault is identified within F and produces S within its deadline; behavior at true standstill is distinguished from loss of feedback. No sustained acceleration toward an invalid feedback target. | H, then representative P |
| HIL-018 | Loss of fresh valid motion commands shall cause a protective stop. Stop host transmission, disconnect the bus, and continue only unrelated/malformed traffic. | Timeout measured from the last qualifying command satisfies F; output and shaft stop satisfy S/T/M. Other traffic cannot keep stale motion authorized. | H + P |
| HIL-019 | CAN bus-off, missing acknowledgments, and TX/RX failures shall have bounded behavior. Inject controlled bus faults and restore communication. | Fault response meets F/S; control is not blocked by TX waits. Recovery meets F and requires the approved fresh-authorization policy; stale queued motion is not replayed. | H |
| HIL-020 | Controller execution failure shall not leave indefinite energized motion. Stall the control task, block event processing, and exercise an MCU hang/reset through a controlled test method. | An independent watchdog/output-inhibit mechanism achieves the fault-specific S state within F even when normal task execution cannot continue. Restart follows S. A watchdog dependent on the failed task path alone is insufficient evidence. | H, then low-energy P |
| HIL-021 | Stall/overload and power-stage protection shall keep the assembly within its electrical and thermal limits. Apply a controlled mechanical stall or overload; use rated fault fixtures for any short-circuit case. | Current and temperature stay within approved E limits for the defined exposure, protection acts as specified, and post-fault behavior follows S. Distinguish current regulation from overcurrent shutdown. Record hardware-only protection separately from firmware fault detection. | P; H for firmware response logic |
| HIL-022 | Supply disturbances shall not cause unintended motion. Sweep the approved range, inject defined dips/brownouts, interrupt supply, and restore power during motion and scheduled motion. | Within E: required performance maintained. Outside the operating range but within the approved fault-test envelope: outputs follow S, no unsafe transient/restart, and fresh authorization is required after recovery. | P |
| HIL-023 | The assembly shall operate throughout its declared thermal envelope and respond predictably to overtemperature. Test Q ambient/load corners and controlled thermal fault stimuli. | Continuous operation meets E/C without thermal-limit violation; protective action and recovery meet F/S. No uncontrolled cycling of energized motion during driver automatic recovery. Sensor-dependent diagnostics require actual sensing evidence. | P; H for available sensor logic |
| HIL-024 | Fault indications and restart authorization shall be coherent with physical output. Clear faults, reset counters, restore CAN, and send stale/new commands in varied order. | Cleared faults alone do not restart motion; only the approved recovery sequence does. Status/RGB/CAN indication matches state within B/F. When CAN is unavailable, local protection still operates and fault reporting follows the defined reconnection policy. | H + P |

### Endurance and regression

| ID | Requirement and stimulus | Pass criteria / evidence | Method |
| --- | --- | --- | --- |
| HIL-025 | The driver shall tolerate repeated start/stop/reverse/query cycles and sustained operation over Q. Include fault/recovery cycles and supported maximum traffic. | No unexpected resets, deadlocks, resource exhaustion, data corruption, or output outside E/M; timing and control remain within T/C. Capture reset causes, thermal trend, and resource metrics where available. | H + P |
| HIL-026 | All required behavior shall remain reproducible across the approved release matrix. Repeat the relevant suite after firmware, board, motor, configuration, or toolchain changes. | Every applicable requirement has passing evidence for Q coverage; changes to the test model or acceptance limits are reviewed separately from DUT changes. | H + P |

## 6. Measurement and execution rules

1. Approve the electrical/mechanical envelope, S policy, protocol, and required limits before powered testing. Verify fixture shutdown and measurement polarity first.
2. Run H tests for startup, PWM interpretation, parsing, scheduling, STOP, and fault handling before progressing to unloaded low-energy P tests. Then expand to the approved load, supply, and thermal corners.
3. For scheduled starts, measure the first permitted output transition against the target's declared timebase. For STOP, record separately the time to safe electrical output and the time/travel to mechanical standstill. A stopped control thread does not establish a stopped motor.
4. For each speed point, compute steady-state error over the approved window after settling. Use an absolute error limit near zero to avoid undefined or misleading percentages. Determine overshoot and settling from the same documented step/reference definition.
5. Apply measurement uncertainty in the pass decision. For an upper limit L, a conservative rule is measured maximum plus uncertainty <= L; otherwise use a preapproved guard-band rule. Do not silently subtract fixture latency or disregard outliers.
6. Use explicit result states: **Pass**, **Fail**, **Blocked/TBD**, **Not Run**, or **Not Applicable with justification**. An implementation still in progress is Blocked or Not Run, not Pass. If a run aborts or a fixture overruns, retain the data and classify it; never report it as a clean pass.

## 7. Required test record and release gate

For each HIL ID, record: approved requirement revision and parameter values; firmware commit and binary hash; board revision/serial; motor/encoder/load identity; Zephyr/toolchain/configuration; model/fixture revision; instrument calibration and uncertainty; environment; exact stimulus and random seed; timestamps and raw CAN/waveform/measurement files; expected versus measured extrema; result; and linked follow-up action.

A release candidate passes this HIL/bench gate only when all applicable requirements have approved limits and passing evidence over Q, no protective-stop/restart failure remains open, and all blocked/not-applicable items have a reviewed disposition. Passing this gate does not substitute for separate system safety, EMC, environmental, or lifetime qualification.

## 8. Sources and design evidence

- [TI DRV8871 datasheet](https://www.ti.com/lit/gpn/drv8871): input truth table and timing, current regulation, electrical limits, protection, and automatic recovery. Check the exact board implementation and part revision before adopting test limits.
- [NI: What is hardware-in-the-loop?](https://www.ni.com/en/solutions/transportation/hardware-in-the-loop/what-is-hardware-in-the-loop-.html): basis for closing the real-controller loop through a real-time plant simulation.
- Project configuration: [`nucleo_g431kb.overlay`](../nucleo_g431kb.overlay), [`prj.conf`](../prj.conf), [`can_config.h`](../src/DRIVER_CONFIG/can_config.h), [`pid_config.h`](../src/DRIVER_CONFIG/pid_config.h), [`pwm.c`](../src/PWM/pwm.c), and the [README](../README.md). These establish the current interface baseline, not acceptance evidence.
