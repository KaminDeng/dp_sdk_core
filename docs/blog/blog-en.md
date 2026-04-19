# One C++ Interface Across Linux and Multiple RTOSes: The Design Value of `dp_sdk_core`

GitHub: <https://github.com/KaminDeng/dp_sdk_core>

Many embedded projects move quickly in the beginning.  
A workflow is validated on Linux first, then moved to an MCU. One board is supported first, then more hardware variants follow. One RTOS is adopted first, and portability is considered later.

The real difficulty is usually not whether the first version can run, but whether the codebase remains stable when platforms start changing:

- can business logic remain stable when the OS changes?
- can Linux and multiple RTOSes share a reasonably aligned programming model?
- can board, chip, or system changes stay confined to a small number of boundaries?
- can C++ abstractions preserve expressiveness on MCUs without letting resource cost get out of control?

`dp_sdk_core` is aimed at exactly those problems.  
It is not “one more wrapper layer.” It is an attempt to provide a core layer better suited for long-term embedded evolution.

---

## 1. Its real value is not just “cross-platform support”

`dp_sdk_core` consists of three core modules:

| Module | Focus | Role |
|---|---|---|
| `dp_osal` | OS abstraction | Unifies threads, mutexes, queues, timers, semaphores, and related primitives |
| `dp_hal` | Hardware abstraction | Unifies UART, GPIO, SPI, I2C, ADC, PWM, and related peripheral interfaces |
| `dp_device` | Device management | Unifies registration, lookup, open/close control, and lifecycle handling |

The dependency structure is intentionally restrained:

```text
device  ->  hal   (public)
device  ->  osal  (private)
hal     ->  independent
osal    ->  POSIX / CMSIS-OS backend
```

What matters is not only that the project is split into three modules.  
What matters is that this split gives change a clear home:

- OS differences are contained in `dp_osal`
- hardware differences are contained in `dp_hal`
- device lifecycle concerns are contained in `dp_device`

For embedded systems that must evolve over time, those boundaries matter more than how fast a single driver can be wrapped.

---

## 2. CMSIS-OS is used to adapt multiple RTOSes and align Linux / RTOS programming interfaces

This is one of the most important characteristics of `dp_sdk_core`.

Many embedded projects do not fail because they support only one system. They fail because, even when multiple systems are supported, the programming model becomes fragmented:

- Linux uses one threading and synchronization style
- FreeRTOS uses another
- RT-Thread or Zephyr requires another adaptation layer
- the business logic stays conceptually similar, but the upper-layer programming style does not

`dp_sdk_core` does not expose each RTOS’s native API directly to the application layer.  
Instead, it stabilizes the upper-layer system programming model through `dp_osal`, and converges different RTOSes through the **CMSIS-OS interface**.

That means:

1. upper-layer code is primarily written against one OSAL interface,
2. RTOS-specific variation is concentrated in the adaptation layer,
3. any RTOS that can be adapted to CMSIS-OS can, in principle, fit into this framework.

The project has already been adapted and validated on **FreeRTOS, RT-Thread, and Zephyr**.  
This is why its value is not merely “multi-RTOS support,” but a **unified interface model across Linux and multiple RTOS environments**.

---

## 3. Structurally, it solves the problem of where change should live

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-arch.png" width="860" />
</div>

The main value of this diagram is not just its layered layout.  
It shows several engineering realities:

1. **Application logic should remain as stable as possible**  
   Upper-layer business code depends on core abstractions rather than direct platform APIs.

2. **OS differences are concentrated in `dp_osal`**  
   Linux / POSIX and RTOS environments both expose capabilities upward through one OSAL model.

3. **Hardware differences are concentrated in `dp_hal`**  
   UART, GPIO, SPI, I2C, and similar peripheral implementations do not directly leak into business code.

4. **Device lifecycle is concentrated in `dp_device`**  
   Registration, lookup, open/close behavior, and centralized management are not scattered across modules.

Cross-platform projects rarely fail because differences exist.  
They fail because those differences have no disciplined boundary.  
That is one of the clearest strengths of `dp_sdk_core`.

---

## 4. From an evolution perspective, it makes migration and product expansion more controllable

<div align="center">
<img src="https://cdn.jsdelivr.net/gh/KaminDeng/dp_sdk_core@master/docs/blog/images/dp-sdk-core-migration.png" width="860" />
</div>

Compared with the architecture diagram, this migration view is closer to the project’s practical engineering value.

The left side reflects a common embedded state:

- business logic, RTOS APIs, vendor HAL calls, and device initialization order are all mixed together
- the firmware runs, but any system, hardware, or product-profile change quickly expands the modification surface

The right side reflects the state `dp_sdk_core` is designed to establish:

- upper-layer business code depends on stable contracts
- OS differences are absorbed through POSIX / CMSIS-OS paths
- hardware differences are absorbed through HAL ports
- device lifecycle is gathered into the Device layer

That is also why this design is especially suitable when a team moves from a demo mindset to a product-line mindset.  
A demo optimizes for “getting it running.”  
A product line optimizes for “keeping change under control.”

---

## 5. It is not only about abstraction, but also about the engineering value of C++ interfaces

Many embedded abstraction layers eventually become little more than renamed C APIs. They may use C++ syntax, but they do not significantly improve expressiveness or cost control.

`dp_sdk_core` is more interesting because it does not give up the engineering strengths of C++.

### 5.1 Production paths prefer CRTP instead of default all-virtual interfaces

In `dp_osal` and `dp_hal`, many core interfaces are built around CRTP.

The point is not to appear “more template-oriented.”  
The point is compile-time binding of implementation types:

- hot paths are easier to inline
- a vtable is not introduced by default
- abstraction remains available without automatically becoming a runtime burden

That matters on MCUs because abstraction becomes risky when runtime cost is spread across every path by default.

### 5.2 Virtual wrappers are used only when test injection is needed

The project also provides `dp_osal_virtual.h` and `dp_hal_virtual.h` for host-side dependency injection and GMock scenarios.

That reflects a clear design strategy:

- **shipping / production paths** prefer CRTP
- **host-test paths** can switch to virtual wrappers only when mocking is required

So testability is preserved, but test-related runtime cost is not forced onto all firmware paths.

### 5.3 This is disciplined use of modern C++

From an engineering perspective, the project uses several C++ features that are genuinely valuable for long-term maintenance:

- typed interfaces instead of raw pointer-heavy plumbing
- RAII-style helpers such as lock guards
- templates combined with compile-time switches for trimming and portability
- clearer separation of interfaces and implementations for collaborative development

So the notable characteristic is not merely that `dp_sdk_core` uses C++17.  
It is that it **uses C++ interface design to improve engineering structure while keeping the added cost under control**.

---

## 6. `dp_device` addresses one of the most failure-prone late-stage problems: device lifecycle

Early in an embedded project, teams tend to focus on whether drivers work. Very few teams treat device lifecycle management as a first-class design concern from the beginning.

Once the system grows, the following problems usually appear:

- device initialization order becomes unclear
- some modules reopen devices while others close them too early
- naming and type expectations remain informal conventions
- lookup, registration, and cleanup logic spreads across modules

`dp_sdk_core` turns that into a dedicated layer through `dp_device`:

- `Device` provides a unified base for naming, typing, and open/close reference counting
- `DeviceManager` centralizes registration, lookup, iteration, and removal
- the registry uses a fixed-size array with an explicit upper bound
- type handling relies on `DeviceType` instead of RTTI
- synchronization can be protected by OSAL primitives

This may be less visually obvious than a driver optimization, but it is critical for long-term maintainability.  
It turns device usage rules from “things the team is expected to remember” into “things the framework enforces.”

---

## 7. Its treatment of resource cost is “controlled,” not “pretending there is no cost”

This is a very important point.

Many embedded abstraction layers talk about elegance, consistency, and modernization, but avoid the harder question: **what exactly does the abstraction cost, and where is it appropriate?**

`dp_sdk_core` takes a more grounded approach. It does not market all paths as “zero cost.” Instead, it tries to keep costs analyzable, trimmable, and verifiable.

### 7.1 What cost is intentionally reduced

- `dp_hal` is primarily header-only + CRTP, with the goal of eliminating most abstraction overhead at compile time
- `dp_osal` core interfaces are not all-virtual by default
- `dp_device` uses fixed-size arrays instead of default heap allocation
- type handling avoids RTTI where practical

### 7.2 What cost is explicitly acknowledged

- threads, thread pools, and timers are not cost-free primitives
- once `std::function`, synchronization objects, and extra state are introduced, RAM / Flash impact must be evaluated
- switching to virtual wrappers for host-side testing naturally adds runtime dispatch cost

That is much more credible than pretending abstraction is free.

### 7.3 The MCU feasibility table is already a good way to explain this

| Target MCU | Feasibility | Notes |
|---|---|---|
| Cortex-M4 / M7, RAM ≥ 192 KB (CCM recommended) | ✅ current full-test profile is feasible | already has a practical build-and-validate basis |
| Cortex-M3 / small-memory M4, RAM 64–128 KB | ⚠️ feasible only with aggressive trimming | `test_all` must be disabled, features reduced, and the result re-measured |
| Cortex-M0 / M0+, RAM ≤ 64 KB | ❌ not something the current profile should claim directly | requires a dedicated tiny profile and measured proof |

The importance of this table is not only the conclusion itself.  
More importantly, it shows an engineering attitude:

- not every MCU should be treated as equally suitable for the same profile
- architectural value does not remove the need to respect hardware limits
- whether the abstraction is viable depends on target profile, trimming strategy, and measurement

So the project’s stance on cost is not “there is no cost.”  
It is that **cost can be controlled and reasoned about through CRTP, fixed limits, feature trimming, and platform validation**.

---

## 8. The best-fit workflow is to validate on Linux first, then move smoothly to RTOS and hardware targets

Looking at its structure and interface model, `dp_sdk_core` fits a workflow like this:

1. validate core business logic in Linux / POSIX first
2. keep upper-layer programming stable through unified OSAL / HAL interfaces
3. bring different RTOSes into the same model through CMSIS-OS adaptation
4. finally switch to target-specific hardware implementations and complete MCU integration and measurement

The benefits are direct:

- business logic can be verified earlier on the host
- Linux and RTOS-side programming styles stay closer
- board and system changes stay more localized
- teams can treat business validation, RTOS adaptation, and hardware porting as one coherent engineering path

---

## 9. What kind of teams benefit most from this kind of core layer

This type of architecture tends to pay off most when:

- business logic must run in both Linux simulation and MCU firmware
- the product must span multiple RTOSes or multiple hardware platforms
- the team is already paying visible maintenance cost for platform glue code
- a reusable C++17 foundation layer is desired
- a more consistent Linux / multi-RTOS interface experience matters

By contrast, if the firmware is short-lived, one-off, and tied to a single fixed platform, a custom lightweight platform layer may still be enough.

The value of `dp_sdk_core` appears most clearly in **long-term evolution**, not in **one-time bring-up**.

---

## 10. Conclusion

What makes `dp_sdk_core` worth attention is not merely that it wraps threads, UART, or GPIO.  
Its real value is that it places several of the most failure-prone parts of embedded development into one coherent structure:

- how to keep Linux and multiple RTOSes closer to one programming model
- how to converge different RTOSes through CMSIS-OS
- how to isolate hardware differences through HAL ports
- how to centralize device lifecycle handling
- how to retain C++ expressiveness while controlling resource cost

From that perspective, it is not just “another abstraction layer.”  
It is a **more stable, more unified, and more evolvable foundation for embedded development across Linux, RTOS, and hardware platforms**.

<!--
SEO tags:
#embedded #C++17 #crossplatform #FreeRTOS #RTThread #Zephyr #CMSISOS #HAL #OSAL #architecture
-->
