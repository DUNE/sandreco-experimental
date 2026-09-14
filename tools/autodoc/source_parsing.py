import re


def find_class(source):

    pattern = r"namespace\s+([a-zA-Z_]\w*(?:\s*::\s*[a-zA-Z_]\w*)*)\s*\{"
    namespaces = list(set(re.findall(pattern, source)))

    class_match = re.compile(
        r"^\s*(?:class|struct)\s+(\w+)\s*:(?:\s*public\s+|\s*)ufw::process\s*{",
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
            return (class_name, comment, namespaces)
    return (class_name, None, namespaces)


def extract_doxygen_tags(comment_text, clean_output=True):
    if comment_text.strip().startswith("/**") or comment_text.strip().startswith("/*"):
        # Remove the first and last lines if they are comment delimiters
        lines = comment_text.split("\n")[1:-1] if comment_text.count("\n") > 1 else []
    else:
        lines = comment_text.split("\n")
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

    sections = {"text": ""}
    current_section = None
    current_content = ""

    for line in lines:
        line = line.strip()
        if line.startswith("*"):
            line = line[2:].strip()
        line += "\n"
        match = None

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
            if line.strip():
                if (current_section is not None) and (current_section in multiline):
                    sections[current_section] = sections.get(current_section, "") + line
                else:
                    sections["text"] += line

    # Add the last section if it exists
    if current_section:
        sections[current_section] = sections.get(current_section, "") + current_content

    return sections


def extract_dependencies(source):

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
        if "::" in tp[0]:
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
            with_ns = add_ns(match, namespaces)
            dependencies["globals"].add(with_ns)
            dependencies["bad_globals"].add(with_ns)
    pattern = r"set<([^>]+)>\((?:\"([^\"]+)\")?\)"
    matches = re.findall(pattern, source)
    for match in matches:
        if match[1]:
            dependencies["outputs"].add(add_ns(match, namespaces))
        else:
            with_ns = add_ns(match, namespaces)
            dependencies["globals"].add(with_ns)
            dependencies["bad_globals"].add(with_ns)
    pattern = r"instance<([^>]+)>\((?:\"([^\"]+)\")?\)"
    matches = re.findall(pattern, source)
    for match in matches:
        if match[1]:
            dependencies["instanced"].add(add_ns(match, namespaces))
        else:
            dependencies["globals"].add(add_ns(match, namespaces))
    return dependencies


def extract_configuration(source):
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
