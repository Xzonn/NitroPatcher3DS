import glob
import zipfile


with zipfile.ZipFile("vendor/TWiLightMenu/slot1launch/data/np3d_resources.twl.bin", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    archive.write("out/nds-bootstrap.nds", "bootstrap.nds")
    for path in sorted(path.replace("\\", "/") for path in glob.glob("assets/fonts/*.nftr")):
        if path != "assets/fonts/song12.nftr":
            archive.write(path, f"fonts/{path[13:]}")
    archive.write("assets/startup.bin", "ui/startup.bin")
    for path in sorted(glob.glob("texts/*.json")):
        archive.write(path, f"languages/{path[6:]}")

with zipfile.ZipFile("vendor/TWiLightMenu/slot1launch/data/np3d_base_font.bin", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    archive.write("assets/fonts/song12.nftr", "fonts/song12.nftr")
