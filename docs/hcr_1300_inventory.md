# Hill Climb Racing v1.30.0 — Complete Content Inventory & Reverse Engineering Registry

## 1. Overview & Provenance
This registry documents the canonical vehicles and stages present in Hill Climb Racing v1.30.0. All identifiers, in-game titles, unlock prices, icon file paths, and RTTI classes were automatically extracted from the original release binary (`libgame.so`) via `tools/extract_1300_info.py` and verified in `docs/bench/re_1300_dump.txt`.

- **Source APK:** `com.fingersoft.hillclimb_1.30.0-119_minAPI17(armeabi,armeabi-v7a,x86)(nodpi)_apkmirror.com.apk`
- **APK SHA-256:** `d21df47ee110baf1e9e5b3cdd8c6aa815b180473c8b42d48a044bde0bbfacb26`
- **`libgame_x86.so` SHA-256:** `0a8a1ebf405e7830fd25a77442f571ac8c69221eb36f931a28ccb177eab1aa3e`
- **Total Registered Vehicles:** 29
- **Total Registered Stages:** 28 (27 standard + Xmas/North Pole)

---

## 2. Complete Vehicle Registry (29 Vehicles)

| # | ID (`vid`) | In-Game Name | Unlock Cost | Garage Preview Icon | Primary Body Texture | Primary Wheel Texture | 4th Slot Type | Extracted Addr |
|---|---|---|---|---|---|---|---|---|
| 1 | `jeep` | JEEP (Hill Climber) | 0 | `car-icon-jeep.png` | `Car.png` | `Tire.png` | 4WD | `0x0f7387` |
| 2 | `bike` | MOTOCROSS BIKE | 75,000 | `car-icon-motocross-bike.png` | `bike-motocross.png` | `bike-tire.png` | AIR CONTROL (`bike_rotation`) | `0x0f75b5` |
| 3 | `monster` | MONSTER TRUCK | 100,000 | `car-icon-monster.png` | `car-monster.png` | `car-monster-tire.png` | 4WD | `0x0f77e9` |
| 4 | `tractor` | TRACTOR | 100,000 | `car-icon-tractor.png` | `car-tractor.png` | `car-tractor-tire.png` | STABILITY / FUEL | `0x0f7a1d` |
| 5 | `hippievan` | HIPPIE VAN | 120,000 | `car-icon-hippievan.png` | `car-hippievan-chassis.png` | `car-hippievan-tire.png` | BOOST / FUEL | `0x0f7c51` |
| 6 | `onewheeler` | ONEWHEELER | 150,000 | `car-icon-onewheeler.png` | `car-onewheeler.png` | `Tire.png` | BATTERY | `0x0f7e85` |
| 7 | `quadbike` | QUAD BIKE | 175,000 | `car-icon-quadbike.png` | `car-quadbike.png` | `Tire.png` | 4WD | `0x0f80b9` |
| 8 | `touristbus` | TOURIST BUS | 200,000 | `car-icon-touristbus.png` | `car-touristbus.png` | `Tire.png` | 4WD | `0x0f82ed` |
| 9 | `racecar` | RACE CAR | 250,000 | `car-icon-racecar.png` | `car-racecar.png` | `Tire.png` | DOWNFORCE | `0x0f8521` |
| 10 | `screwfinger` | FINGER SCREW | 300,000 | `car-icon-screw-finger.png` | `car-screwfinger-hull.png` | `Tire.png` | SCREW SPEED | `0x0f8755` |
| 11 | `policecar` | POLICE CAR | 350,000 | `car-icon-policecar.png` | `car-policecar-chassis.png` | `car-policecar-tire.png` | SIREN / 4WD | `0x0f8989` |
| 12 | `ambulance` | AMBULANCE | 400,000 | `car-icon-ambulance.png` | `car-ambulance-chassis.png` | `car-ambulance-tire.png` | 4WD | `0x0f8bbd` |
| 13 | `electric` | ELECTRIC CAR | 450,000 | `car-icon-electric.png` | `car-electric-chassis.png` | `car-electric-tire.png` | BATTERY | `0x0f8df1` |
| 14 | `dieseltruck`| SUPER DIESEL 4X4 | 500,000 | `car-icon-4x4.png` | `car-pickup.png` | `Tire.png` | 4WD | `0x0f9025` |
| 15 | `rallycar` | RALLY CAR | 750,000 | `car-icon-rallycar.png` | `car-rallycar.png` | `Tire.png` | DOWNFORCE | `0x0f9259` |
| 16 | `dragster` | DRAGSTER | 800,000 | `car-icon-dragster.png` | `car-dragster.png` | `car-dragster-rear-tire.png` | DOWNFORCE | `0x0f948d` |
| 17 | `firetruck` | FIRE TRUCK | 800,000 | `car-icon-firetruck.png` | `car-firetruck.png` | `car-firetruck-tire.png` | 4WD | `0x0f96c1` |
| 18 | `tank` | TANK | 1,000,000 | `car-icon-tank.png` | `car-tank.png` | `car-tank-tire.png` | CANNON / TREADS | `0x0f98f5` |
| 19 | `snowmobile`| SNOW MOBILE | 1,000,000 | `car-icon-snowmobile.png` | `car-snowmobile-body.png` | `car-snowmobile-ski.png` | TRACK DRIVE | `0x0f9b29` |
| 20 | `superoffroad`| SUPER OFFROAD | 1,000,000 | `car-icon-super-offroad.png`| `car-superoffroad-fg.png` | `Tire.png` | DOWNFORCE / 4WD | `0x0f9d5d` |
| 21 | `truck` | TRUCK | 1,000,000 | `car-icon-truck.png` | `car-truck.png` | `Tire.png` | 4WD | `0x0f9f91` |
| 22 | `dunebuggy` | DUNE BUGGY | 800,000 | `car-icon-dunebuggy.png` | `car-dune-buggy.png` | `car-dune-buggy-tire.png` | 4WD | `0x0fa1c5` |
| 23 | `hovercraft`| HOVERCRAFT | 1,000,000 | `car-icon-hovercraft.png` | `lowres-hovercraft.png` | N/A (cushion) | THRUST | `0x0fa3f9` |
| 24 | `minibike` | MINIBIKE | 1,000,000 | `car-icon-minibike.png` | `car-minibike.png` | `bike-tire.png` | AIR CONTROL | `0x0fa62d` |
| 25 | `bigfinger` | BIG FINGER | 1,200,000 | `car-icon-bigfinger.png` | `car-bigfinger.png` | `car-bigfinger-tire.png` | 4WD | `0x0fa861` |
| 26 | `train` | KIDDIE EXPRESS | 1,500,000 | `car-icon-train.png` | `car-train.png` | `car-train-tire.png` | TORQUE | `0x0faa95` |
| 27 | `moonlander`| MOONLANDER | 1,500,000 | `car-icon-moonlander.png` | `car-moonlander-hull.png` | `car-moonlander-tire.png` | THRUSTERS | `0x0facc9` |
| 28 | `hotrod` | HOT ROD | 1,500,000 | `car-icon-hotrod.png` | `car-hotrod-chassis.png` | `car-hotrod-tire.png` | OVERDRIVE | `0x0faefd` |
| 29 | `sleigh` | SLEIGH | 1,000,000 | `car-icon-sleigh.png` | `assets/atlas/sleigh.png`| N/A (skis/reindeer) | MAGIC | `0x0fb131` |

---

## 3. Complete Stage Registry (28 Stages)

| # | ID (`id`) | In-Game Title | Unlock Cost | Stage Icon | Extracted Addr |
|---|---|---|---|---|---|
| 1 | `countryside` | Countryside | 0 | `stage-icon-countryside.png` | `0x14c38e` |
| 2 | `seasons` | Seasons | 50,000 | `stage-icon-seasons.png` | `0x14c407` |
| 3 | `desert` | Desert | 35,000 | `stage-icon-desert.png` | `0x14c483` |
| 4 | `arctic` | Arctic | 75,000 | `stage-icon-arctic.png` | `0x14c4ff` |
| 5 | `highway` | Highway | 125,000 | `stage-icon-highway.png` | `0x14c57b` |
| 6 | `cave` | Cave | 150,000 | `stage-icon-cave.png` | `0x14c5f7` |
| 7 | `moon` | Moon | 175,000 | `stage-icon-moon.png` | `0x14c673` |
| 8 | `bootcamp` | Boot Camp | 200,000 | `stage-icon-bootcamp.png` | `0x14c6ef` |
| 9 | `mars` | Mars | 250,000 | `stage-icon-mars.png` | `0x14c76b` |
| 10 | `xmas` | Xmas | 300,000 | `stage-icon-xmas.png` | `0x14c84a` |
| 11 | `alienplanet` | Alien Planet | 750,000 | `stage-icon-alienplanet.png` | `0x14c8c6` |
| 12 | `arcticcave` | Arctic Cave | 800,000 | `stage-icon-arctic-cave.png` | `0x14c942` |
| 13 | `forest` | Forest | 850,000 | `stage-icon-forest.png` | `0x14c9be` |
| 14 | `mountain` | Mountain | 850,000 | `stage-icon-cliffs.png` | `0x14ca3a` |
| 15 | `ragnarok` | Ragnarok | 1,000,000 | `stage-icon-ragnarok.png` | `0x14cab6` |
| 16 | `mudpool` | Mudpool | 1,000,000 | `stage-icon-mud.png` | `0x14cb32` |
| 17 | `volcano` | Volcano | 1,000,000 | `stage-icon-volcano.png` | `0x14cbae` |
| 18 | `beach` | Beach | 1,000,000 | `stage-icon-beach.png` | `0x14cc2a` |
| 19 | `factory` | Factory | 1,000,000 | `stage-icon-factory.png` | `0x14cca6` |
| 20 | `rollercoaster`| Roller Coaster | 1,200,000 | `stage-icon-rollercoaster.png`| `0x14cd28` |
| 21 | `night` | Night | 1,200,000 | `stage-icon-night.png` | `0x14cda4` |
| 22 | `rooftops` | Rooftops | 1,200,000 | `stage-icon-rooftops.png` | `0x14ce20` |
| 23 | `junkyard` | Junkyard | 1,400,000 | `stage-icon-junkyard.png` | `0x14ce9c` |
| 24 | `construction` | Construction | 1,400,000 | `stage-icon-construction.png` | `0x14cf18` |
| 25 | `rainbow` | Rainbow | 1,500,000 | `stage-icon-rainbow.png` | `0x14d005` |
| 26 | `haunted` | Haunted | 1,500,000 | `stage-icon-haunted.png` | `0x14d081` |
| 27 | `northpole` | North Pole | 1,500,000 | `stage-icon-northpole.png` | `0x14d0fd` |
| 28 | `nuclear` | Nuclear Plant | 1,500,000 | `stage-icon-nuclear.png` | `0x14d179` |

---

## 4. Key Takeaways for Architecture
1. **Total Count Bounds:** 29 vehicles and 28 stages comfortably fit within `MAX_CONFIG_VEHICLES = 64` and `MAX_CONFIG_STAGES = 64`.
2. **Asset Formats:** Standalone `.png` textures are used for almost all vehicles and stages, avoiding complex Cocos2d atlas reconstruction.
3. **Hardware Texture Formats for PSP:**
   - Previews and icons use `RGBA5551` to keep memory footprint $<0.2$ MB in garage carousel.
   - Vehicle bodies and wheels use `RGBA8888` for crisp color reproduction.
