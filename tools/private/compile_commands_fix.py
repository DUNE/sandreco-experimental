import json
import os

def replace_paths_in_strings(data, old_prefix, new_prefix):
    """
    Recursively replace substrings in JSON strings that start with `old_prefix`.
    Only replaces the longest possible match starting with the prefix.
    """
    if isinstance(data, str):
        data = data.replace(old_prefix, new_prefix)
        data = data.replace('-std=c++17', '-std=c++17 -I' + new_prefix + '/ufw/include/ -I'  + new_prefix + '/ufw/build/lib/include/')
        data = data.replace('/opt/root/include', '/usr/include/root')
        return data
    elif isinstance(data, dict):
        return {k: replace_paths_in_strings(v, old_prefix, new_prefix) for k, v in data.items()}
    elif isinstance(data, list):
        return [replace_paths_in_strings(item, old_prefix, new_prefix) for item in data]
    else:
        return data  # Leave non-string data unchanged

def replace_paths_in_json(file_path, old_prefix, new_prefix):
    """
    Replace paths in a JSON file's strings that start with `old_prefix`.
    """
    # Read the JSON file
    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Replace paths in all strings
    modified_data = replace_paths_in_strings(data, old_prefix, new_prefix)

    # Write the modified JSON back to the file
    with open(file_path, 'w', encoding='utf-8') as f:
        json.dump(modified_data, f, indent=2, ensure_ascii=False)

    print(f"Paths replaced in {file_path}. Old prefix: '{old_prefix}' → New prefix: '{new_prefix}'")

# Run me from project root folder
if __name__ == "__main__":
    replace_paths_in_json("compile_commands.json", "/home/sand", os.path.dirname(os.getcwd()))
