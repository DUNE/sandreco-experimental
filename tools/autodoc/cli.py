import re
import argparse
import os

from source_parsing import (
    extract_configuration,
    extract_dependencies,
    extract_doxygen_tags,
    find_class,
)

# ANSI Color Codes (Foreground)
ANSI_RED = "\033[31m"
ANSI_GREEN = "\033[32m"
ANSI_YELLOW = "\033[33m"
ANSI_BLUE = "\033[34m"
ANSI_RESET = "\033[0m"  # Reset to default


def print_red(text):
    if not quiet:
        print(ANSI_RED + text + ANSI_RESET)


def print_green(text):
    if not quiet:
        print(ANSI_GREEN + text + ANSI_RESET)


def print_yellow(text):
    if not quiet:
        print(ANSI_YELLOW + text + ANSI_RESET)


def print_blue(text):
    if not quiet:
        print(ANSI_BLUE + text + ANSI_RESET)


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


def suggestion(section, content, header=False):
    headers = {
        "configuration": "\\subsection Configuration\n"
        "| Parameter Name | Type | Unit | Required/Default | Description |\n"
        "|----------------|------|------|------------------|-------------|\n",
        "dependencies": "\\subsection Dependencies\n"
        "| Type | Comment |\n"
        "|------|---------|\n",
        "inputs": "\\subsection Requirements\n"
        "| Name | Type | Comment |\n"
        "|------|------|---------|\n",
        "outputs": "\\subsection Products\n"
        "| Name | Type | Comment |\n"
        "|------|------|---------|\n",
    }
    if header:
        suggest = headers[section]
    else:
        suggest = ""

    def add_ns(tp, ns):
        if "::" in tp:
            return [tp]
        else:
            return [n + "::" + tp for n in ns.split(";")]

    if section in ["inputs", "outputs"]:
        for tp, name, ns in content:
            for variant in add_ns(tp, ns):
                suggest += f"| `{name}` | `{variant}` | <description> |\n"
    elif section == "dependencies":
        for tp, name, ns in content:
            for variant in add_ns(tp, ns):
                suggest += f"| `{variant}` | <purpose of {name}> |\n"
    elif section == "configuration":
        for line in content:
            suggest += f"| `{line}` | `<type>` | <unit> | <req> | <description> |\n"
    return suggest[:-1]  # strip final neline


def match_section(clname, section, tags, deps):
    if section not in tags:
        tag = ""
    else:
        tag = tags[section]
    suggestitems = []
    if section in ["inputs", "outputs"]:
        missing = match_ios(tag, deps[section])
        for item in missing:
            for tp, name, ns in deps[section]:
                if item == name:
                    suggestitems.append((tp, name, ns))
    elif section == "dependencies":
        fullset = deps["globals"] | deps["instanced"]
        missing = match_dependencies(tag, fullset)
        for item in missing:
            for tp, name, ns in fullset:
                if item == tp:
                    suggestitems.append((tp, name, ns))
    elif section == "configuration":
        missing = match_configuration(tag, deps)
    if suggestitems:
        if section not in tags:
            print_red(f"class `{clname}` is missing the {section} section")
            print_blue(f"Suggested correction: Add the {section} section:")
        else:
            print_red(f"class `{clname}` has undocumented {section}: {missing}")
            print_blue(f"Suggested correction: Add to the {section} section:")
        print_blue(suggestion(section, suggestitems, section not in tags))


def missing_docs(name, source):
    print(f"{name}:")
    cc = find_class(source)
    if cc is not None:
        print_green(f"Processing class `{cc[0]}`")
        if cc[1] is not None:
            tags = extract_doxygen_tags(cc[1].group(1))
            if "brief" not in tags or len(tags["brief"]) < 20:
                print_yellow(f"class `{cc[0]}` lacks an adequate brief")
            if "text" not in tags or len(tags["text"]) < 160:
                print_yellow(f"class `{cc[0]}` lacks an adequate description")
            deps = extract_dependencies(source)
            if deps["bad_globals"]:
                print_yellow(
                    f"class `{cc[0]}` is accessing globals using `get`/`set` "
                    f"instead of `instance`: {[x[0] for x in deps['bad_globals']]}"
                )
            match_section(cc[0], "inputs", tags, deps)
            match_section(cc[0], "outputs", tags, deps)
            match_section(cc[0], "dependencies", tags, deps)
            parameters = extract_configuration(source)
            match_section(cc[0], "configuration", tags, parameters)
        else:
            print_red(f"class `{cc[0]}` has no documentation")


def list_modules(name, source):
    cc = find_class(source)
    if cc is not None:
        print_green(f"Processing class `{cc[0]}`")
        if cc[1] is not None:
            tags = extract_doxygen_tags(cc[1].group(1))
            if "brief" not in tags:
                print_yellow(f"class `{cc[0]}` lacks a brief")
                return (cc[0], None, cc[2][0])
            else:
                return (cc[0], tags["brief"], cc[2][0])
        else:
            print_red(f"class `{cc[0]}` has no documentation")
            return (cc[0], None, cc[2][0])
    return None


def md_table(header, lines):
    ticks = [s.startswith("`") and s.startswith("`") for s in header]
    fixedheader = [s.strip("`") for s in header]
    ncols = len(header)
    assert min(map(len, lines)) >= ncols
    lengths = list(
        max(len(s) for s in col) + 4 for col in zip(*([fixedheader] + lines))
    )
    table = []
    ln = "|"
    for i, head in enumerate(fixedheader):
        ln += f" {head:<{lengths[i] - 1}}|"
    table.append(ln)
    ln = "|"
    for i in range(ncols):
        ln += "-" * lengths[i] + "|"
    table.append(ln)
    for line in lines:
        ln = "|"
        for i in range(ncols):
            if ticks[i]:
                s = f"`{line[i]}`"
            else:
                s = line[i]
            ln += f" {s:<{lengths[i] - 1}}|"
        table.append(ln)
    return "\n".join(table)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Examine sources for documentation")
    parser.add_argument(
        "-q", "--quiet", action="store_true", help="Suppress all outputs"
    )
    action = parser.add_mutually_exclusive_group(required=True)

    action.add_argument(
        "-m", "--missing", action="store_true", help="Find missing documentation"
    )
    action.add_argument(
        "-l",
        "--list",
        type=str,
        nargs="?",
        const="docs/modules.md",
        metavar="<.md file>",
        help="List modules",
    )
    parser.add_argument("files", nargs="+", help="Files or directories to process")
    args = parser.parse_args()
    global quiet
    quiet = args.quiet
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
    if args.missing:
        for source in contents:
            missing_docs(source, contents[source])
    elif args.list:
        lines = [list_modules(source, contents[source]) for source in contents]
        lines = [
            (
                line[2] + "::" + line[0],
                line[1] if line[1] is not None else "not available",
            )
            for line in lines
            if line is not None
        ]
        table = md_table(("`Module Name`", "Description"), lines)
        print_blue(table)
        with open(args.list, "w") as fout:
            fout.write("# Module list\n\n")
            fout.write(table + "\n")
