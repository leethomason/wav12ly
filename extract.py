# extract.py name input/zip/file zip/path [xml]

from genericpath import isdir
import shutil
import sys
import os
import os.path
import zipfile

def setup_dir(dir):
    if os.path.isdir(dir):
        shutil.rmtree(dir)
    os.mkdir(dir)

def extract_type(input, prefix, dir):
    with zipfile.ZipFile(input, mode="r") as archive:
        for info in archive.infolist():
            if info.filename.startswith(prefix) and info.file_size > 0:
                name = info.filename[len(prefix):]
                print(name + " ->" + dir)

                data = archive.read(info)
                fp = open(dir + "/" + name, "wb")
                fp.write(data)
                fp.close()


FONT_NAME = sys.argv[1]
INPUT_FILE = sys.argv[2]
ZIP_PATH = sys.argv[3]
GEN_XML = False
if len(sys.argv) == 5 and sys.argv[4] == 'xml':
    GEN_XML = True
    print("Generating XML file")

print("Font name:  " + FONT_NAME)
print("Input file: " + INPUT_FILE)
print("Zip:        " + ZIP_PATH)

# Get directories set up.
IN_PATH = './fonts/in/' + FONT_NAME
OUT_PATH = './fonts/out/' + FONT_NAME
POST_PATH = './fonts/post/' + FONT_NAME

if not os.path.isdir("fonts"):
    os.mkdir("fonts")
if not os.path.isdir("fonts/in"):
    os.mkdir("fonts/in")
if not os.path.isdir("fonts/out"):
    os.mkdir("fonts/out")
if not os.path.isdir("fonts/post"):
    os.mkdir("fonts/post")

setup_dir(IN_PATH)
setup_dir(OUT_PATH)
setup_dir(POST_PATH)

extract_type(INPUT_FILE, ZIP_PATH + "/blst/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/clsh/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/hum/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/out/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/in/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/swingh/", IN_PATH)
extract_type(INPUT_FILE, ZIP_PATH + "/swingl/", IN_PATH)

if GEN_XML:
    leaf_name = INPUT_FILE[INPUT_FILE.rfind('/') + 1:]

    xml_fp = open("./fonts/" + FONT_NAME + ".xml", "w")
    xml_fp.write('<?xml version="1.0" encoding="utf-8"?>\n' +
                 '<Image path="' + leaf_name + '" zip="' + ZIP_PATH + '">\n' + 
                 '  <Directory name="' + FONT_NAME + '" path="./out/' + FONT_NAME + '" post="./post/' + FONT_NAME + '">\n')

# sox ./fonts/in/vaderanh/hum01.wav -b 16 -c 1 -r 22050 ./fonts/out/vaderanh/hum01.wav lowpass 10000
for root, dirs, files in os.walk(IN_PATH):
    for name in files:
        in_path = IN_PATH + "/" + name
        out_path = OUT_PATH + "/" + name
        #print(in_path + " -> " + out_path)

        cmd = "sox.exe " + in_path + " -b 16 -c 1 -r 22050 " + out_path + " lowpass 10000 fade 0.001 -0 0.003"
        #print(cmd)
        os.system(cmd)
        if GEN_XML:
            xml_fp.write('    <File path="' + name + '" />\n')

if GEN_XML:
    xml_fp.write('  </Directory>\n</Image>\n')
