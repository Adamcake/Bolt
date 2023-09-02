#!/usr/bin/env python3
# python file invoked by cmake to output a .cxx file for the icons in this directory
from PIL import Image
import os
import sys

images = [("16.png", "GetIcon16"), ("32.png", "GetIcon32"), ("64.png", "GetIcon64"), ("256.png", "GetIcon256"), ("tray.png", "GetTrayIcon")]
srcdir = sys.argv[1] if len(sys.argv) > 1 else '.'
icondir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(srcdir, 'icon')

print("#include \"" + srcdir + "/src/browser/client.hxx\"")

for (file_name, function_name) in images:
    var_name = "icon_" + file_name.replace('.', '_')
    out_px = []
    f = Image.open(os.path.join(icondir, file_name), 'r')
    pixels = f.load()
    for y in range(f.height):
        for x in range(f.width):
            out_px += pixels[x, y]
    print("constexpr unsigned char " + var_name + "[] = {" + ','.join(map(hex, out_px)) + "};")
    print("const unsigned char* Browser::Client::" + function_name + "() const { return " + var_name + "; }")
