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
    for path, name in (
        ("LICENSE", "NitroPatcher3DS.txt"),
        ("vendor/md4c/LICENSE.md", "md4c.txt"),
        ("vendor/fmt/LICENSE", "fmt.txt"),
        ("assets/fonts/Gulim-OFL.txt", "Gulim-OFL.txt"),
        ("assets/fonts/Gulim-AUTHORS.txt", "Gulim-AUTHORS.txt"),
        ("/opt/np3d/liblzma/COPYING", "liblzma.txt"),
    ):
        archive.write(path, f"licenses/{name}")

with zipfile.ZipFile("vendor/TWiLightMenu/slot1launch/data/np3d_base_font.bin", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    archive.write("assets/fonts/song12.nftr", "fonts/song12.nftr")
