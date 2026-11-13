import glob
from PIL import Image

for f in sorted(glob.glob("assets/*.png")):
    try:
        im = Image.open(f)
        print(f"{f:35s}: {im.size}")
    except Exception as e:
        print(f, e)

