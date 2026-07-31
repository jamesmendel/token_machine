from pathlib import Path
import shutil

Import("env")

lib = (
    Path(env["PROJECT_LIBDEPS_DIR"])
    / env["PIOENV"]
    / "TFT_eSPI"
    / "TFT_Drivers"
    / "ILI9341_Init.h"
)

replacement = Path(env["PROJECT_DIR"]) / "patches" / "ILI9341_Init.h"

def patch():
    print("Replacing TFT_eSPI ILI9341_Init.h")
    shutil.copy2(replacement, lib)

print("patch_tft.py loaded")
patch()