import shutil
import os
from subprocess import call

# Define the paths
script_dir = os.path.dirname(os.path.abspath(__file__))  # Directory where the script is located
source_dir = os.path.abspath("_build/source")
samples_dir = os.path.abspath("../../samples")
static_dir = os.path.abspath("../_static")  # Path to the _static directory
requirements_path = os.path.join(script_dir, "requirements.txt")  # Path to the requirements.txt file
includes_dir = os.path.join(script_dir, "includes")  # Path to the includes directory

# Install packages from requirements.txt
if os.path.exists(requirements_path):
    print("Installing packages from requirements.txt...")
    call(["pip", "install", "-r", requirements_path])
else:
    print("requirements.txt not found.")
    exit()

# Create the source directory if it doesn't exist
os.makedirs(source_dir, exist_ok=True)

# Copy RST files
for root, dirs, files in os.walk(".", topdown=True):
    # Exclude the source directory from the search
    dirs[:] = [d for d in dirs if os.path.abspath(os.path.join(root, d)) != source_dir]
    for file in files:
        if file.endswith(".rst"):
            src_file_path = os.path.join(root, file)
            if os.path.abspath(src_file_path) != os.path.join(source_dir, file):
                shutil.copy2(src_file_path, source_dir)

# Copy sample READMEs
if os.path.exists(samples_dir):
    for root, dirs, files in os.walk(samples_dir):
        for file in files:
            if file == "README.rst":
                relative_dir = os.path.relpath(root, samples_dir)
                dest_dir = os.path.join(source_dir, "samples", relative_dir)
                os.makedirs(dest_dir, exist_ok=True)
                shutil.copy2(os.path.join(root, file), dest_dir)

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

# Copy Sphinx configuration file and other text files
conf_path = os.path.join(script_dir, "conf.py")
links_path = os.path.join(script_dir, "links.txt")
shortcuts_path = os.path.join(script_dir, "shortcuts.txt")

for file_path in [conf_path, links_path, shortcuts_path]:
    if os.path.exists(file_path):
        shutil.copy2(file_path, source_dir)
    else:
        print(f"File not found: {file_path}")
        exit()

# Run Sphinx
call(["sphinx-build", "-b", "html", source_dir, "_build/html"])

print("Build finished. The HTML pages are in _build/html.")
