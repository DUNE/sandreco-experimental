import re


def find_class_comments(cpp_file_path):
    """
    Scans a C++ file for classes inheriting from 'ufw::process' and extracts their Doxygen comments.
    Returns a dictionary: {class_name: comment_text}
    """
    with open(cpp_file_path, "r", encoding="utf-8") as file:
        content = file.read()

    # Regex to match class definitions (including inheritance)
    class_match = re.compile(
        r"^\s*(?:class|struct)\s+(\w+)\s*(?:[:>]\s*|\s*:\s*)"  # Match class name followed by optional ':' or '>'
        r"\s*public\s+ufw::process\s*(\s*(?:,?\s*\w+\s*=>\s*\w+\s*)*)?\s*\{",  # Match public inheritance
        re.MULTILINE | re.DOTALL,
    )

    doxygen_comment_match = re.compile(r"/\*\*(.*?)\*/", re.MULTILINE | re.DOTALL)

    classes_found = {}
    for match in class_match.finditer(content):
        class_name = match.group(1)
        class_tag_match = re.compile(r"\\class (?:[\w:]+::)*" + re.escape(class_name))
        for comment in doxygen_comment_match.finditer(content):
            if class_tag_match.search(comment.group(1)):
                classes_found[class_name] = comment

    return classes_found


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
        # Add more patterns as needed
    }

    multiline = ["configuration"]

    # Initialize the result dictionary
    sections = {
        "text": "",  # Regular paragraphs without Doxygen tags
    }

    current_section = None
    current_content = ""

    def daoi(k, v):
        sections[k] = sections.get(k, "") + v

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


# Example usage
if __name__ == "__main__":
    import sys

    cpp_file = sys.argv[1]
    class_comments = find_class_comments(cpp_file)
    for class_name, comment in class_comments.items():
        if comment:
            tags = extract_doxygen_tags(comment.group(1))
            for k, v in tags.items():
                print(f"Section {k}:")
                print(v)
        else:
            print("No Doxygen comment found.")
        print("---")
