import os
import shutil
from subprocess import call

# Define the paths
script_dir = os.path.dirname(os.path.abspath(__file__))  # Directory where the script is located
source_dir = os.path.abspath("_build/source")
samples_dir = os.path.abspath("../../samples")
static_dir = os.path.abspath("../_static")  # Path to the _static directory
requirements_path = os.path.join(script_dir, "requirements.txt")  # Path to the requirements.txt file
includes_dir = os.path.join(script_dir, "includes")  # Path to the includes directory
sample_dir = os.path.join(script_dir, "sample")  # Path to the sample directory
pdf_source_dir = os.path.abspath('../../components/softdevice/s115/')  # Path to the PDF source directory
pdf_destination_dir = os.path.join(source_dir, 'pdfs')  # Destination directory within _static
images_dir = os.path.join(script_dir, "images")  # Path to the images directory
softdevice_rst_source = os.path.abspath('../../components/softdevice/s115/doc')

# Install packages from requirements.txt
if os.path.exists(requirements_path):
    print("Installing packages from requirements.txt...")
    call(["pip", "install", "-r", requirements_path])
else:
    print("requirements.txt not found.")
    exit()

# Create the source directory if it doesn't exist
os.makedirs(source_dir, exist_ok=True)
os.makedirs(pdf_destination_dir, exist_ok=True)

# Copy RST files and maintain directory structure
for root, dirs, files in os.walk(".", topdown=True):
    # Exclude the source directory from the search
    dirs[:] = [d for d in dirs if os.path.abspath(os.path.join(root, d)) != source_dir]
    for file in files:
        if file.endswith(".rst"):
            src_file_path = os.path.join(root, file)
            relative_path = os.path.relpath(root, ".")
            dest_dir = os.path.join(source_dir, relative_path)
            os.makedirs(dest_dir, exist_ok=True)
            shutil.copy2(src_file_path, dest_dir)

# Copy PDFs
if os.path.exists(pdf_source_dir):
    for file in os.listdir(pdf_source_dir):
        if file.endswith(".pdf"):
            src_file_path = os.path.join(pdf_source_dir, file)
            shutil.copy2(src_file_path, pdf_destination_dir)
            print(f"Copied PDFs to {pdf_destination_dir}")
else:
    print("PDF source directory not found.")
    exit()

# Copy sample READMEs
if os.path.exists(samples_dir):
    for root, dirs, files in os.walk(samples_dir):
        for file in files:
            if file == "README.rst":
                relative_dir = os.path.relpath(root, samples_dir)
                dest_dir = os.path.join(source_dir, "samples", relative_dir)
                os.makedirs(dest_dir, exist_ok=True)
                shutil.copy2(os.path.join(root, file), dest_dir)

# Copy SoftDevice RST files to the _build/source directory
for file_name in os.listdir(softdevice_rst_source):
    if file_name.endswith('.rst'):
        src_file_path = os.path.join(softdevice_rst_source, file_name)
        shutil.copy2(src_file_path, source_dir)
        print(f"Copied {file_name} to {source_dir}")

# Copy the _static directory
dest_static_dir = os.path.join(source_dir, "_static")
if os.path.exists(dest_static_dir):
    shutil.rmtree(dest_static_dir)  # Remove the existing directory if it exists
shutil.copytree(static_dir, dest_static_dir)

# Copy the includes directory
if os.path.exists(includes_dir):
    dest_includes_dir = os.path.join(source_dir, "includes")
    if os.path.exists(dest_includes_dir):
        shutil.rmtree(dest_includes_dir)  # Remove the existing directory if it exists
    shutil.copytree(includes_dir, dest_includes_dir)
else:
    print("includes directory not found.")
    exit()

# Copy the images directory
if os.path.exists(images_dir):
    dest_images_dir = os.path.join(source_dir, "images")
    if os.path.exists(dest_images_dir):
        shutil.rmtree(dest_images_dir)  # Remove the existing directory if it exists
    shutil.copytree(images_dir, dest_images_dir)
else:
    print("images directory not found.")
    exit()

# Copy Sphinx configuration file and other text files
conf_path = os.path.join(script_dir, "conf.py")
links_path = os.path.join(script_dir, "links.txt")
substitutions_path = os.path.join(script_dir, "substitutions.txt")
shortcuts_path = os.path.join(script_dir, "shortcuts.txt")

for file_path in [conf_path, links_path, substitutions_path, shortcuts_path]:
    if os.path.exists(file_path):
        shutil.copy2(file_path, source_dir)
    else:
        print(f"File not found: {file_path}")
        exit()

# Prepare Doxygen XML output for Breathe

# Save the current directory
original_dir = os.getcwd()

# Path to the Doxyfile
doxyfile_dir = os.path.abspath('doxygen')
doxyfile_path = os.path.join(doxyfile_dir, 'nrf-bm.doxyfile')

# Change to the directory containing the Doxyfile
os.chdir(doxyfile_dir)

# Run Doxygen
print("Running Doxygen...")
call(['doxygen', 'nrf-bm.doxyfile'])

# Change back to the original directory
os.chdir(original_dir)

# Path to the Doxygen output directory
doxygen_output_dir = os.path.abspath('doxygen/doxygen/nrf-bm_api_xml')

# Destination directory for Doxygen XML files within the Sphinx source
doxygen_dest_dir = os.path.join(source_dir, 'doxygen/nrf-bm_api_xml')

# Copy Doxygen XML output to the Sphinx source directory
if os.path.exists(doxygen_output_dir):
    if os.path.exists(doxygen_dest_dir):
        shutil.rmtree(doxygen_dest_dir)  # Remove the existing directory if it exists
    shutil.copytree(doxygen_output_dir, doxygen_dest_dir)
    print(f"Copied Doxygen XML files to {doxygen_dest_dir}")
else:
    print("Doxygen output directory not found.")
    exit()

# Run Sphinx
call(["sphinx-build", "-b", "html", source_dir, "_build/html"])

print("Build finished. The HTML pages are in _build/html.")
