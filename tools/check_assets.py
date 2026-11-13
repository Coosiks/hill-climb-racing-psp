import os

files = [
    "assets/coin-icon.png",
    "assets/fuel-icon.png",
    "assets/lowres-pedal-gas-normal.png",
    "assets/lowres-pedal-gas-pressed.png",
    "assets/Car.png",
    "assets/Tire.png",
    "assets/driver-body.png",
    "assets/driver-head.png",
    "assets/terrain-ground.png",
    "assets/terrain-surface.png",
    "assets/desert-ground.png",
    "assets/desert-surface.png",
    "assets/arctic-ground.png",
    "assets/arctic-surface.png",
    "assets/moon-ground.png",
    "assets/moon-surface.png",
    "assets/Coin5.png",
    "assets/Coin25.png",
    "assets/Coin100.png",
    "assets/Coin500.png",
    "assets/fuel-canister.png",
    "assets/MainMenuUpgradeEngine.png",
    "assets/MainMenuUpgradeSuspension.png",
    "assets/MainMenuUpgradeTires.png",
    "assets/MainMenuUpgradeAWD.png",
    "assets/stage-icon-countryside.png",
    "assets/stage-icon-desert.png",
    "assets/stage-icon-arctic.png",
    "assets/stage-icon-moon.png",
    "assets/fonts/gamefont.fnt",
    "assets/fonts/gamefont_0.png",
    "assets/audio/engine.ogg",
    "assets/audio/coin.ogg",
    "assets/audio/crack.ogg",
    "assets/audio/refuel.ogg",
    "assets/audio/click.ogg"
]

base = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
missing = [f for f in files if not os.path.isfile(os.path.join(base, f))]
print("Missing files:", missing)
if not missing:
    print("ALL REQUIRED ASSETS ARE PRESENT AND VERIFIED!")

