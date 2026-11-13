# Fidelity Calibration Reference & Verification Protocols

**Benchmark:** Hill Climb Racing v1.0.5 / v1.18.0 (Android / PS Vita port)  
**Target:** Clean-room reimplementation on Sony PSP & PC  
**Acceptance Tolerance:** $\le 10\%$ deviation across all behavioral benchmarks

---

## 1. Benchmark Metrics & Ground Truth Targets

| Metric | Target Value (Stock Jeep, Countryside) | Max Allowed Deviation | Verification Method |
| :--- | :--- | :--- | :--- |
| **0 to 100 m Acceleration Time** | 9.4 s $\pm 0.8$ s | $\le 10\%$ | Frame count from start input to crossing 100m sensor |
| **Top Speed on Flat Ground** | 40.5 km/h $\pm 2.0$ km/h | $\le 10\%$ | Peak velocity on flat surface segment |
| **Max Climbable Slope** | $38^\circ \pm 2^\circ$ | $\le 10\%$ | Highest angle steep hill stock Jeep can climb without roll-back |
| **Airborne 360° Backflip Period** | 1.85 s $\pm 0.15$ s | $\le 10\%$ | Full rotation time in air holding throttle with stock Jeep |
| **Suspension Damping Decay** | $2.5$ oscillation cycles | $\le 10\%$ | Cycle count after a 3-meter vertical drop onto flat ground |
| **Fuel Tank Depletion (Full Throttle)** | 45.0 s $\pm 2.0$ s | $\le 10\%$ | Time from 100% fuel to empty while holding gas continuously |
| **Fuel Tank Depletion (Idle)** | 160.0 s $\pm 10.0$ s | $\le 10\%$ | Time from 100% fuel to empty while idling at start line |

---

## 2. Qualitative Behavioral Fidelity

1. **Moon Stage ("Floatiness"):**
   - Reduced gravity ($g = -3.5\text{ m/s}^2$) produces extended airtime.
   - High surface grip ($\mu = 0.80$) allows steep climbs despite low vehicle weight.
   - Soft bouncy landings without chassis penetration.
2. **Arctic Stage ("Slipperiness"):**
   - Very low friction ($\mu = 0.15$) causes realistic tire slippage during full throttle.
   - Uphill climbs require momentum rather than pure torque.
3. **Bridge Dynamic Behavior:**
   - 10 to 15 wooden planks connected via distance/revolute joints.
   - Sag under car weight, bounce back when crossed.
4. **Driver Vulnerability:**
   - Neck joint flexibility allows driver head to bob during acceleration/braking.
   - Game over triggers instantly when driver head contacts terrain geometry or obstacles.

---

## 3. PC Telemetry & In-Game Calibration Tool

To eliminate guesswork, the PC build (`hcr_pc`) provides an integrated telemetry overlay:
- Live display of:
  - Velocity ($v_x, v_y, \text{km/h}$)
  - Chassis pitch angle ($\theta$) and angular velocity ($\omega$)
  - Suspension compression per wheel
  - Fuel level and drain rate
  - Frame time breakdown: Physics ms, Render ms, Memory allocation count
- Deterministic input recording and replay to execute automated regression runs.

