#!/usr/bin/env python3

import subprocess
from io import StringIO
import re
import sys

class Option:
    def __init__(self, long_opt, short_opt, optional):
        self.long_opt = long_opt
        self.short_opt = short_opt
        self.optional = optional
        self.values = []
        self.path = False

def get_all_options(cmd):
    opt_pattern = re.compile(r'\s+(?:(-.), )?(--[^\s\[=]+)(\[)?(?:=(\S+))?')
    values_pattern = re.compile(r'\s+Possible Values: (.+)')
    proc = subprocess.Popen([cmd, "--help=#all"], stdout=subprocess.PIPE)
    stdoutdata, stderrdata = proc.communicate()
    cur_option = None
    opts = {}
    for line in StringIO(stdoutdata.decode('utf-8')):
        match = opt_pattern.match(line)
        if match:
            long_opt = match.group(2)
            short_opt = match.group(1)
            optional = match.group(3) == '['
            if cur_option:
                opts[cur_option.long_opt] = cur_option
            cur_option = Option(long_opt, short_opt, optional)
            argument = match.group(4) or ''
            if '|' in argument:
                cur_option.values = argument.split('|')
            cur_option.path = argument == 'path'
        else:
            match = values_pattern.match(line)
            if match:
                cur_option.values = match.group(1).split(', ')
    if cur_option:
        opts[cur_option.long_opt] = cur_option

    # for opt in opts.values():
    #     print(opt.short_opt, opt.long_opt, opt.optional, opt.values)

    return opts

def output_value_case(out, key, values):
    out.write("""\
        {0})
            COMPREPLY=( $( compgen -W '{1}' -- "$cur" ) )
            return 0
            ;;
""".format(key, " ".join(values)))

def output_value_case_file_comp(out, key, exts):
    out.write("""\
        {0})
            _filedir '@({1})'
            return 0
            ;;
""".format(key, '|'.join(exts)))

def output_value_case_dir_comp(out, key):
    out.write("""\
        {0})
            _filedir -d
            return 0
            ;;
""".format(key))

def output_value_case_any_file_comp(out, key):
    out.write("""        {0})
            _filedir
            return 0
            ;;
""".format(key))

def output_case(out, opts):
    out.write("""\
_aria2_next()
{
    local cur prev split=false
    COMPREPLY=()
    COMP_WORDBREAKS=${COMP_WORDBREAKS//=}

    cmd=${COMP_WORDS[0]}
    _get_comp_words_by_ref cur prev
""")
    bool_opts = []
    nonbool_opts = []
    for opt in opts.values():
        if opt.values == ['true', 'false']:
            bool_opts.append(opt)
        else:
            nonbool_opts.append(opt)

    out.write("""\
    case $prev in
""")
    # Complete pre-defined option arguments
    for opt in opts.values():
        if opt.values:
            output_value_case(out, opt.long_opt, opt.values)
    # Complete directory
    typed = {'--torrent-file': ['torrent'], '--metalink-file': ['meta4', 'metalink']}
    paths = [opt.long_opt for opt in opts.values() if opt.path and opt.long_opt not in typed]
    dirs = [name for name in paths if name.endswith('dir')]
    files = [name for name in paths if not name.endswith('dir')]
    output_value_case_dir_comp(out, '|'.join(dirs))
    output_value_case_any_file_comp(out, '|'.join(files))
    for name, exts in typed.items():
        output_value_case_file_comp(out, name, exts)
    out.write("""\
    esac
""")
    # Complete option name.
    out.write("""\
    case $cur in
        -*)
            COMPREPLY=( $( compgen -W '\
""")
    bool_values = [ 'true', 'false' ]
    for opt in opts.values():
        out.write(opt.long_opt)
        out.write(' ')
        # Options which takes optional argument needs "=" between
        # option name and value, so we complete them including "=" and
        # value here.
        if opt.optional:
            if bool_values == opt.values:
                # Because boolean option takes true when argument is
                # omitted, we just complete additional 'false' option
                # only.
                out.write('='.join([opt.long_opt, 'false']))
                out.write(' ')
            else:
                for value in opt.values:
                    out.write('='.join([opt.long_opt, value]))
                    out.write(' ')

    out.write("""\
' -- "$cur" ) )
            ;;
""")
    # If no option found for completion then complete with files.
    out.write("""\
        *)
            _filedir '@(torrent|meta4|metalink|text|txt|list|lst)'
            [ ${#COMPREPLY[@]} -eq 0 ] && _filedir
            return 0
    esac
    return 0
}
complete -F _aria2_next aria2-next
""")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Generates aria2-next(1) bash_completion using `aria2-next --help=#all'")
        print("Usage: make_bash_completion.py /path/to/aria2-next")
        exit(1)
    opts = get_all_options(sys.argv[1])
    output_case(sys.stdout, opts)
