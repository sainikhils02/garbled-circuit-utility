# Let's create a comprehensive project structure for the garbled circuits utility
import os

# Create project directory structure
project_structure = {
    "src/": {
        "garbler.cpp": "",
        "evaluator.cpp": "",
        "garbled_circuit.cpp": "",
        "garbled_circuit.h": "",
        "ot_handler.cpp": "",
        "ot_handler.h": "",
        "crypto_utils.cpp": "",
        "crypto_utils.h": "",
        "socket_utils.cpp": "",
        "socket_utils.h": "",
        "main.cpp": ""
    },
    "include/": {
        "common.h": ""
    },
    "examples/": {
        "simple_and.cpp": "",
        "millionaires.cpp": ""
    },
    "cmake/": {
        "FindLibOTe.cmake": ""
    },
    "": {  # root directory files
        "CMakeLists.txt": "",
        "README.md": "",
        "build.sh": "",
        ".gitignore": ""
    }
}

# Create the directory structure
def create_directory_structure(base_path, structure):
    for folder, contents in structure.items():
        folder_path = os.path.join(base_path, folder)
        if folder:  # Not root directory
            os.makedirs(folder_path, exist_ok=True)
        
        # Create files in the folder
        for filename, content in contents.items():
            file_path = os.path.join(folder_path, filename)
            with open(file_path, 'w') as f:
                f.write(content)

# Create all directories and files
project_root = "garbled_circuits_utility"
os.makedirs(project_root, exist_ok=True)
create_directory_structure(project_root, project_structure)

print("Project structure created successfully!")
print("Directory tree:")
for root, dirs, files in os.walk(project_root):
    level = root.replace(project_root, '').count(os.sep)
    indent = ' ' * 2 * level
    print(f"{indent}{os.path.basename(root)}/")
    subindent = ' ' * 2 * (level + 1)
    for file in files:
        print(f"{subindent}{file}")