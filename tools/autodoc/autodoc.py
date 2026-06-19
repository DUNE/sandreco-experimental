import re
import sys
import argparse
import os

# ANSI Color Codes (Foreground)
ANSI_RED = "\033[31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_BLUE = "\033[34m"
ANSI_RESET = "\033[0m"  # Reset to default


def print_red(text):
    print(ANSI_RED + text + ANSI_RESET)


def print_green(text):
    print(ANSI_GREEN + text + ANSI_RESET)


def print_yellow(text):
    print(ANSI_YELLOW + text + ANSI_RESET)


def print_blue(text):
    print(ANSI_BLUE + text + ANSI_RESET)


def find_class(source):

    # Regex to match class definitions (including inheritance)
    class_match = re.compile(
        r"^\s*(?:class|struct)\s+(\w+)\s*(?:[:>]\s*|\s*:\s*)"  # Match class name followed by optional ':' or '>'
        r"\s*public\s+ufw::process\s*(\s*(?:,?\s*\w+\s*=>\s*\w+\s*)*)?\s*\{",  # Match public inheritance
        re.MULTILINE | re.DOTALL,
    )

    doxygen_comment_match = re.compile(r"/\*\*(.*?)\*/", re.MULTILINE | re.DOTALL)

    match = class_match.search(source)
    if match is None:
        return None
    class_name = match.group(1)
    class_tag_match = re.compile(r"\\class (?:[\w:]+::)*" + re.escape(class_name))
    for comment in doxygen_comment_match.finditer(source):
        if class_tag_match.search(comment.group(1)):
            return (class_name, comment)
        else:
            return (class_name, None)


def extract_doxygen_tags(comment_text, clean_output=True):
    if comment_text.strip().startswith("/**") or comment_text.strip().startswith("/*"):
        # Remove the first and last lines if they are comment delimiters
        lines = comment_text.split("\n")[1:-1] if comment_text.count("\n") > 1 else []
    else:
        lines = comment_text.split("\n")
    # Define patterns for Doxygen sections
    patterns = {
        "brief": re.compile(r"\\brief\s+(.*)", re.DOTALL),
        "class": re.compile(r"\\class\s+(.*)", re.DOTALL),
        "configuration": re.compile(r"\\subsection\s+Configuration\s+(.*)", re.DOTALL),
        "dependencies": re.compile(r"\\subsection\s+Dependencies\s+(.*)", re.DOTALL),
        "inputs": re.compile(r"\\subsection\s+Requirements\s+(.*)", re.DOTALL),
        "outputs": re.compile(r"\\subsection\s+Products\s+(.*)", re.DOTALL),
        # Add more patterns as needed
    }

    multiline = ["configuration", "dependencies", "inputs", "outputs"]

    # Initialize the result dictionary
    sections = {
        "text": "",  # Regular paragraphs without Doxygen tags
    }

    current_section = None
    current_content = ""

    for line in lines:
        line = line.strip()
        if line.startswith("*"):
            line = line[2:].strip()
        line += "\n"
        match = None

        # Check for section matches
        for section_name, pattern in patterns.items():
            match = pattern.search(line)
            if match:
                # If we had content for the previous section, add it
                if current_section:
                    sections[current_section] = (
                        sections.get(current_section, "") + current_content
                    )
                    current_content = ""
                current_section = section_name
                content_after_tag = match.group(1).strip()
                # Handle multi-line content for this tag
                current_content += content_after_tag
                break

        if match is None:
            # No tag found, add as regular text or to current section
            if line.strip():  # Only if line isn't empty
                if (current_section is not None) and (current_section in multiline):
                    sections[current_section] = sections.get(current_section, "") + line
                else:
                    sections["text"] += line

    # Add the last section if it exists
    if current_section:
        sections[current_section] = sections.get(current_section, "") + current_content

    return sections


def examine_configuration(source):
    parameters = {"paths": [], "required": [], "optional": []}
    pattern = r"cfg\.at\(\"(.*?)\"\)"
    matches = re.findall(pattern, source)
    for match in matches:
        parameters["optional"].append(match)
    pattern = r"cfg\.value\(\"(.*?)\"(?:,|\s*)?(.*?)\)"
    matches = re.findall(pattern, source)
    for match in matches:
        parameters["required"].append(match[0])
    pattern = r"cfg\.path_at\(\"(.*?)\"\)"
    matches = re.findall(pattern, source)
    for match in matches:
        parameters["paths"].append(match)
    return parameters


def examine_dependencies(source):

    pattern = r"namespace\s+([a-zA-Z_]\w*(?:\s*::\s*[a-zA-Z_]\w*)*)\s*\{"
    namespaces = set(re.findall(pattern, source))

    dependencies = {
        "inputs": set(),
        "bad_globals": set(),
        "globals": set(),
        "instanced": set(),
        "outputs": set(),
    }

    def add_ns(tp, ns):
        if tp[0].startswith("sand::") or "::" in tp[0]:
            # Most likely already a fully qualified name
            return (tp[0], tp[1], "")
        ns = ns | set(["sand"])
        return (tp[0], tp[1], ";".join(ns))

    pattern = r"get<([^>]+)>\((?:\"([^\"]+)\")?\)"
    matches = re.findall(pattern, source)
    for match in matches:
        if match[1]:
            dependencies["inputs"].add(add_ns(match, namespaces))
        else:
            dependencies["bad_globals"].add(add_ns(match, namespaces))
    pattern = r"set<([^>]+)>\((?:\"([^\"]+)\")?\)"
    matches = re.findall(pattern, source)
    for match in matches:
        if match[1]:
            dependencies["outputs"].add(add_ns(match, namespaces))
        else:
            dependencies["bad_globals"].add(add_ns(match, namespaces))
    pattern = r"instance<([^>]+)>\((?:\"([^\"]+)\")?\)"
    matches = re.findall(pattern, source)
    for match in matches:
        if match[1]:
            dependencies["instanced"].add(add_ns(match, namespaces))
        else:
            dependencies["globals"].add(add_ns(match, namespaces))
    return dependencies


def match_configuration(doxy, src):
    pattern = r"^\|\s*`([^`]+)`\s*\|"
    matches = re.findall(pattern, doxy, re.MULTILINE)
    doxy = set(matches)
    src = set(sum(src.values(), []))
    return list(doxy ^ src)


def match_dependencies(doxy, src):
    pattern = r"^\|\s*`([^`]+)`\s*\|"
    types = re.findall(pattern, doxy, re.MULTILINE)
    doxy = set(types)
    missing = []
    for tp, name, ns in src:
        if tp in doxy:
            continue
        found = False
        for namespace in ns.split(";"):
            found = found or ((namespace + "::" + tp) in doxy)
        if not found:
            missing.append(tp)
    return missing


def match_ios(doxy, src):
    pattern = r"^\|\s*`([^`]+)`\s*\|"
    names = re.findall(pattern, doxy, re.MULTILINE)
    doxy = set(names)
    src = set([tp[1] for tp in src])
    return list(doxy ^ src)


def process_source(name, source):
    print(f"{name}:")
    cc = find_class(source)
    if cc is not None:
        print_green(f"Processing class `{cc[0]}`")
        if cc[1] is not None:
            tags = extract_doxygen_tags(cc[1].group(1))
            parameters = examine_configuration(source)
            deps = examine_dependencies(source)
            if deps["bad_globals"]:
                print_yellow(
                    f"class `{cc[0]}` is accessing globals using get/set instead of instance: {[x[0] for x in deps['bad_globals']]}"
                )
            if "brief" not in tags or len(tags["brief"]) < 20:
                print_yellow(f"class `{cc[0]}` lacks an appropriate brief")
            if "text" not in tags or len(tags["text"]) < 160:
                print_yellow(f"class `{cc[0]}` lacks an appropriate description")
            if "inputs" not in tags:
                print_red(f"class `{cc[0]}` does not have an input section")
            else:
                missing = match_ios(tags["inputs"], deps["inputs"])
                if missing:
                    print_red(f"class `{cc[0]}` has undocumented inputs: {missing}")
            if "outputs" not in tags:
                print_red(f"class `{cc[0]}` does not have an output section")
            else:
                missing = match_ios(tags["outputs"], deps["outputs"])
                if missing:
                    print_red(f"class `{cc[0]}` has undocumented outputs: {missing}")
            if "dependencies" not in tags:
                print_red(f"class `{cc[0]}` does not have a dependencies section")
            else:
                missing = match_dependencies(tags["dependencies"], deps["globals"])
                if missing:
                    print_red(
                        f"class `{cc[0]}` has undocumented dependencies: {missing}"
                    )
            if "configuration" not in tags:
                print_red(f"class `{cc[0]}` does not have a configuration section")
            else:
                missing = match_configuration(tags["configuration"], parameters)
                if missing:
                    print_red(
                        f"class `{cc[0]}` has undocumented configuration parameters: {missing}"
                    )
        else:
            print_red(f"class `{cc[0]}` has no documentation")


# Example usage
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Examine sources for documentation")
    parser.add_argument("files", nargs="+", help="Files or directories to process")
    args = parser.parse_args()
    file_paths = []
    for file_path in args.files:
        if os.path.isdir(file_path):
            for dirpath, _, filenames in os.walk(file_path):
                for filename in filenames:
                    file_paths.append(os.path.join(dirpath, filename))
        else:
            file_paths.append(file_path)
    contents = {}
    for fname in file_paths:
        name, ext = os.path.splitext(fname)
        if ext.lower() in [".cc", ".cpp", ".cxx"]:
            with open(fname, "r", encoding="utf-8") as f:
                contents[fname] = f.read()
            for hext in [".h", ".hpp", ".hxx"]:
                if os.path.exists(name + hext):
                    with open(name + hext, "r", encoding="utf-8") as f:
                        contents[fname] = f.read() + "\n\n" + contents[fname]
    for source in contents:
        process_source(source, contents[source])
