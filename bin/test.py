#!/usr/bin/python3
import os
import argparse
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import List, Set


DEFAULT_DGDEBUG = ['src/dgdebug', '--quit', '--width=1000']
LANG_DOCS = Path('manual/modules/lang')
LIB_DOCS = Path('manual/modules/lib')

ERROR_MESSAGE = re.compile('^Error: ([^,]+), line ([0-9]+): (.+)$')
WARN_MESSAGE = re.compile('^Warning: (.+)$')


@dataclass
class CodeBlock:
    file_path: Path
    line_no: int
    is_source: bool
    roles: Set[str]
    contents: str


@dataclass
class CodeBlockError:
    block: CodeBlock
    file_name: str
    line_no: int
    error: str


def parse_blocks(file_path):
    """
    Parse out blocks from a given Asciidoc file. This is far from a complete parser;
    it just supports the syntax that we're using in practice.
    """
    blocks = []
    with open(file_path, 'r', encoding='utf-8') as file:
        open_block = None
        is_source = False
        roles = set()
        lines = []
        for line_no, line in enumerate(file):
            line = line.rstrip()
            if line == open_block:
                # Got a matching delimiter; close the open block.
                blocks.append(CodeBlock(
                    file_path=file_path,
                    line_no=line_no,
                    is_source=is_source,
                    roles=roles,
                    contents='\n'.join(lines).rstrip(),
                ))
                open_block = None
                lines.clear()
            elif open_block:
                # Add the current line to the open block.
                lines.append(line)
            elif line.startswith('['):
                # Extract just the attributes we care about.
                # Note that role attributes have two different syntaxes to handle... see
                # https://docs.asciidoctor.org/asciidoc/latest/attributes/role/#assign-roles-to-blocks
                line = line.lstrip('[').rstrip(']')
                is_source = False
                roles = set()
                for attr in line.split(','):
                    if attr == 'source':
                        is_source = True
                    elif attr.startswith("."):
                        roles.update(attr.lstrip('.').split('.'))
                    else:
                        kv = attr.split('=', 1)
                        if len(kv) > 1 and kv[0] == 'role':
                            roles.update(kv[1].strip('"').split(' '))
            elif line.startswith('```') or line.startswith('----'):
                # Block delimiter, and we're not in a block: open a fresh one.
                open_block = line
            else:
                # Clear any attributes we've captured from a previous line.
                is_source = False
                roles = []

    return blocks


def check_blocks(blocks: List[CodeBlock], command_template: List[str | None]):
    """
    Check that the given blocks execute as expected, using the provided command template for `dgdebug`.
    """
    errors = []
    previous_output = None
    for block in blocks:
        if block.is_source and not block.contents.startswith('\t') and 'fragment' not in block.roles:
            previous_output = None
            should_error = 'should-error' in block.roles
            with tempfile.NamedTemporaryFile(mode='w', suffix=f'.{block.file_path.stem}.dg') as source_file:
                source_file.write(block.contents)
                source_file.flush()
                command = [part if part else source_file.name for part in command_template]
                process = subprocess.Popen(
                    command,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                try:
                    stdout, _ = process.communicate(timeout=2.0)
                    return_code = process.returncode
                    error_messages = []
                    warn_messages = []
                    output = []
                    for line in stdout.splitlines():
                        error_match = ERROR_MESSAGE.fullmatch(line)
                        if error_match:
                            error_messages.append((error_match[1], int(error_match[2]) - 2, error_match[3]))
                        elif WARN_MESSAGE.fullmatch(line):
                            warn_messages.append(' '.join(line.split()[1:]))
                        else:
                            output.append(line)
                    if return_code != 0 and not should_error:
                        file_name, line_no, message = error_messages[0] or (0, f'return code {return_code}')
                        if file_name == source_file.name:
                            file_name = str(block.file_path)
                            line_no += block.line_no
                        errors.append(CodeBlockError(
                            block=block,
                            file_name=file_name,
                            line_no=line_no,
                            error=message
                        ))
                    elif return_code == 0 and should_error:
                        errors.append(CodeBlockError(
                            block=block,
                            file_name=str(block.file_path),
                            line_no=block.line_no,
                            error='expected error, but dgdebug succeeded'
                        ))
                    else:
                        previous_output = '\n'.join(output).strip()
                except subprocess.TimeoutExpired:
                    if not should_error:
                        errors.append(CodeBlockError(
                            block=block,
                            file_name=str(block.file_path),
                            line_no=block.line_no,
                            error=f'dgdebug timed out'
                        ))
        if 'output' in block.roles:
            matches_previous = 'matches-previous' in block.roles
            if previous_output != block.contents and matches_previous:
                import difflib
                diff = difflib.ndiff(
                    block.contents.splitlines(),
                    previous_output.splitlines(),
                )
                errors.append(CodeBlockError(
                    block=block,
                    file_name=str(block.file_path),
                    line_no=block.line_no,
                    error=f'output does not match:\n {'\n '.join(x.strip() for x in diff)}'
                ))
            if previous_output == block.contents and not matches_previous:
                errors.append(CodeBlockError(
                    block=block,
                    file_name=str(block.file_path),
                    line_no=block.line_no,
                    error=f'output matches previous: add a .matches-previous annotation to the block if this is expected'
                ))
    return errors


def check_docs(dir_path, dgdebug_template):
    """
    Parse and check all the files in a directory.
    """
    paths = sorted(dir_path.glob('**/*.adoc'))
    errors = []
    for path in paths:
        blocks = parse_blocks(path)
        errors.extend(check_blocks(blocks, dgdebug_template))
    return errors


def doc_command(args):
    """
    Implement the `doc` subcommand.
    """
    errors = []
    if args.directory:
        directory = Path(args.directory)
        command = list(DEFAULT_DGDEBUG)
        command.append(None)
        command.extend(args.library)
        errors.extend(check_docs(directory, command))
    else:
        command = list(DEFAULT_DGDEBUG)
        command.append(None)
        errors.extend(check_docs(LANG_DOCS, command))
        command.append('stdlib.dg')
        errors.extend(check_docs(LIB_DOCS, command))

    for error in errors:
        print(f'{error.file_name}, line {error.line_no}: {error.error}')

    exit_code = 1 if errors else 0
    return exit_code


def main():
    parser = argparse.ArgumentParser(
        description='Dialog-specific test utility.'
    )
    subparsers = parser.add_subparsers(
        title='subcommands',
        description='Individual testing commands.',
        metavar='<command>',
        dest='command',
    )

    # Check command
    check_parser = subparsers.add_parser(
        'doc',
        help='Assert that the code blocks in the documentation are valid. '
             'If no directory is specified, checks the default documentation location.'
    )
    check_parser.add_argument(
        'directory',
        help='Documentation directory to process.',
        nargs='?'
    )
    check_parser.add_argument(
        'library',
        help='Additional library file to include with all examples.',
        nargs='*',
        action='append',
    )

    args = parser.parse_args()

    if args.command == 'doc':
        return doc_command(args)
    else:
        parser.print_help()
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
