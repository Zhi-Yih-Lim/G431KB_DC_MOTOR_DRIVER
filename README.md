# STM32 Motor Controller — Zephyr RTOS

An embedded motor-driver firmware project for the **STM32 Nucleo-G431KB**, exploring CAN FD commands, timer-driven coordination, encoder feedback, and PWM actuation with Zephyr RTOS.

**Development status: in progress.** Peripheral interfaces and event-processing paths are present, but integration is still incomplete. CANFD reception transmission is working. UART is a placeholder.

| Area | Details |
| --- | --- |
| Platform | STM32G431KB / Nucleo-G431KB; Zephyr C application |
| Motor interface | PWM on TIM4 channels 1 and 2 controlling DRV8871 motor driver|
| Feedback | Quadrature encoder angular readout |
| Communication | CAN FD configuration: 500 kbit/s arbitration, 2 Mbit/s data |
| Coordination | Queued state-machine events and motor alarms |
| Observability | Zephyr logging, heartbeat GPIO for timer and WS2812 status pixel for state-machine |
| Unit testing | Using ZTEST framework for state-machine transition testing |
| Debugging | Zephyr logging, OpenOCD + GDB, DMM, Oscilloscope |

## Architecture

```mermaid
flowchart LR
    Host[Central controller] -->|CAN FD commands| RX[Callback and enqueue message]
    RX --> Worker[RX message processing worker, generating pre-defined events]
    Worker --> Events[State-machine event queue]
    Events --> SM[State machine]
    SM --> Alarm[Schedule move alarm on TIM2]
    Alarm -->|MOVE event| Events
    SM --> Control[Controller thread: PI controller in progress]
    Encoder[Poll encoder angle] --> Control
    Control --> PWM[PWM motor control on TIM4]
    PWM --> Driver[DRV8871 motor driver]
    SM --> RGB[Visualize State machine status via WS2812]
    SM --> TX[CAN tick response: TX unfinished]
```

Received CAN RX messages are added onto a message queue to which an RX processing thread dequeues and generates relevant events onto a separate event processing message queue. State machine consumes new events posted on this event queue and transitions into the appropriate state, executing relevant state actions through entry and exit functions. Motor control performed in a dedicated high-priority thread, polling angular displacement of the DC motor through QDEC, calculating the actuation signal through an angular velocity PID controller and actuating the DRV8871 module through the PWM peripheral. 

## Hardware configuration

| Peripheral | Pins / configuration |
| --- | --- |
| TIM4 PWM | PB6 / channel 1, PB7 / channel 2; pull-ups, inverted polarity in code |
| TIM3 encoder | PA6 / channel 1, PA7 / channel 2; mode 3; 2448 counts/revolution |
| CAN FD | PA11 RX, PA12 TX; external transceiver and suitable bus termination required |
| TIM2 heartbeat | PA4 GPIO; intended 1 s toggle interval |
| SPI WS2812 | PB3 SCK, PB5 MOSI; one pixel, state colors at 10% brightness |

The encoder's configurations are set to 12 pulses/revolution, a 51:1 gear ratio, and quadrature x4 counting. 

## CAN command format

The logical payload uses 11 bytes in a **12-byte CAN FD frame**:

| Byte offset | Width | Meaning |
| --- | --- | --- |
| 0 | 1 | Payload target: local ID `0x65` or broadcast `0xFF` |
| 1–2 | 2 | Action characters |
| 3–6 | 4 | Action data; move uses big-endian float bits for rad/s |
| 7–10 | 4 | Big-endian target counter ticks |
| 11 | 1 | Padding for DLC of 9 = 12 bytes |

The RX hardware filter uses standard CAN identifier `0x64` for the central controller. The source contains stop (`S` prefix), reset (`R` prefix), and move (`MF` / `MB`) decoding. `GT` currently posts **COUNTER_RESET**, an implementation defect. The state machine can independently construct a `TT` response for `SM_EVENT_SEND_TICKS`, but the **CAN send pipeline is unfinished**. 
