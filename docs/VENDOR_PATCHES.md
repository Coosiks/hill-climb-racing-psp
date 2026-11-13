# Vendor Patches & Upstream Modifications

This document records all modifications applied to third-party vendor code included in this repository, in compliance with upstream licenses (specifically zlib Clause 2: *"Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software"*).

---

## 1. Box2D 2.3.1 — Wheel Joint Translation Limits Backport

- **Upstream Project:** [Box2D](http://www.box2d.org) (c) 2006-2014 Erin Catto
- **License:** zlib license (see [vendor/Box2D/Box2D/License.txt](../vendor/Box2D/Box2D/License.txt) or top of source files)
- **Base Version:** 2.3.1 (`b2_version = {2, 3, 1}` in `vendor/Box2D/Box2D/Common/b2Settings.cpp`)
- **Patch Commit:** `9195b305c1a7bb58ae08ecb0a2b06a0d91b29926` (Task T-109)
- **Modified Files:**
  - `vendor/Box2D/Box2D/Dynamics/Joints/b2WheelJoint.h`
  - `vendor/Box2D/Box2D/Dynamics/Joints/b2WheelJoint.cpp`

### Motivation & Rationale
In Box2D 2.3.1, `b2WheelJoint` supports linear spring/damper suspension and rotational motor torque, but lacks physical translation limit constraints (`enableLimit`, `lowerTranslation`, `upperTranslation`). In Hill Climb Racing, free-fall landings from heights $> 4\text{ m}$ compress the suspension beyond physical vehicle bounds, pushing wheel centers through the chassis polygon into inverted configurations.

Box2D 2.4.1 added linear limit constraints to `b2WheelJoint`. Rather than upgrading the entire physics engine (which would introduce breaking API and simulation differences across all joints and broadphase), the translation limit constraint solver was backported to 2.3.1.

### Technical Summary of Modifications
1. **`b2WheelJointDef` Additions:**
   - `bool enableLimit` (default `false`)
   - `float32 lowerTranslation` (default `0.0f`)
   - `float32 upperTranslation` (default `0.0f`)
2. **`b2WheelJoint` Class Interface Additions:**
   - `bool IsLimitEnabled() const`
   - `void EnableLimit(bool flag)`
   - `float32 GetLowerLimit() const`
   - `float32 GetUpperLimit() const`
   - `void SetLimits(float32 lower, float32 upper)`
   - Internal limit impulse storage: `m_lowerImpulse`, `m_upperImpulse`
3. **Constraint Solver Integration:**
   - `InitVelocityConstraints`: Evaluates current joint translation along axis $a_x$; activates lower or upper limit state (`e_atLowerLimit`, `e_atUpperLimit`, `e_equalLimits`).
   - `SolveVelocityConstraints`: Solves 1D limit impulses clamped to $[0, \infty)$ using standard Karush-Kuhn-Tucker (KKT) complementary conditions.
   - `SolvePositionConstraints`: Enforces position limits within `b2_linearSlop` tolerance.

### Attribution & Header Marking
Both altered files contain explicit attribution notice immediately following the original Erin Catto license block:
```cpp
// MODIFIED: translation-limit solver backported from Box2D 2.4.1 (see docs/VENDOR_PATCHES.md). Original Box2D (c) Erin Catto, zlib license.
```
