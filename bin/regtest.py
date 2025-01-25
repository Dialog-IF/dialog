#!/usr/bin/python3

# RegTest: a really simple IF regression tester.
#   Version 1.2
#   Andrew Plotkin <erkyrath@eblong.com>
#   This script is in the public domain.
#
# For a full description, see <http://eblong.com/zarf/plotex/regtest.html>
#
# (This software is not connected to PlotEx; I'm just distributing them
# from the same folder.)

# We use the print() function for Python 2/3 compatibility
from __future__ import print_function

# We use the Py2 unichr() function. In Py3 there is no such function,
# but we define a back-polyfill. (I'm lazy.)
try:
    unichr(32)
except NameError:
    unichr = chr

# In Py2, we'll need a bit of extra decoding.
py2_readline = False
try:
    unicode
    py2_readline = True
except:
    pass

import sys
import os
import optparse
import select
import time
import fnmatch
import subprocess
import re
import types

gamefile = None
terppath = None
terpargs = []
terpformat = 'cheap'    # 'cheap', 'rem', 'remsingle'
precommands = []

checkclasses = []
testmap = {}
testls = []
totalerrors = 0

popt = optparse.OptionParser()

popt.add_option('-g', '--game',
                action='store', dest='gamefile',
                help='game to test')
popt.add_option('-i', '--interpreter', '--terp',
                action='store', dest='terppath',
                help='interpreter to execute')
popt.add_option('-l', '--list',
                action='store_true', dest='listonly',
                help='list all tests (or all matching tests)')
popt.add_option('-p', '--pre', '--precommand',
                action='append', dest='precommands',
                help='extra command to execute before (each) test')
popt.add_option('-c', '--cc', '--checkclass',
                action='append', dest='checkfiles', metavar='FILE',
                help='module containing custom Check classes')
popt.add_option('-f', '--format',
                action='store', dest='terpformat',
                help='the interpreter format: cheap, rem, remsingle')
popt.add_option('-r', '--rem',
                action='store_true', dest='remformat',
                help='equivalent to --format rem')
popt.add_option('-E', '--env',
                action='append', dest='env',
                help='environment variables to set before running interpreter')
popt.add_option('-t', '--timeout',
                dest='timeout_secs', type=float, default=1.0,
                help='timeout interval (default: 1.0 sec)')
popt.add_option('--vital',
                action='count', dest='vital', default=0,
                help='abort a test on the first error (or the whole run, if repeated)')
popt.add_option('-v', '--verbose',
                action='count', dest='verbose', default=0,
                help='display the transcripts as they run')

(opts, args) = popt.parse_args()

if (not args):
    print('usage: regtest.py TESTFILE [ TESTPATS... ]')
    sys.exit(1)

class RegTest:
    """RegTest represents one test in the test file. (That is, a block
    beginning with a single asterisk.)

    A test is one session of the game, from the beginning. (Not necessarily
    to the end.) After every game command, tests can be run.
    """
    def __init__(self, name):
        self.name = name
        self.gamefile = None   # use global gamefile
        self.terp = None       # global terppath, terpargs
        self.precmd = None
        self.cmds = []
    def __repr__(self):
        return '<RegTest %s>' % (self.name,)
    def addcmd(self, cmd):
        self.cmds.append(cmd)

class Command:
    """Command is one cycle of a RegTest -- a game input, followed by
    tests to run on the game's output.
    """
    glk_key_names = {
        'left':0xfffffffe, 'right':0xfffffffd, 'up':0xfffffffc,
        'down':0xfffffffb, 'return':0xfffffffa, 'delete':0xfffffff9,
        'escape':0xfffffff8, 'tab':0xfffffff7, 'pageup':0xfffffff6,
        'pagedown':0xfffffff5, 'home':0xfffffff4, 'end':0xfffffff3,
        'func1':0xffffffef, 'func2':0xffffffee, 'func3':0xffffffed,
        'func4':0xffffffec, 'func5':0xffffffeb, 'func6':0xffffffea,
        'func7':0xffffffe9, 'func8':0xffffffe8, 'func9':0xffffffe7,
        'func10':0xffffffe6, 'func11':0xffffffe5, 'func12':0xffffffe4,
    }
    
    def __init__(self, cmd, type=None):
        if type is None:
            # Peel off the "{...}" prefix, if found.
            match = re.match('{([a-z_]*)}', cmd)
            if not match:
                type = 'line'
                cmd = cmd.strip()
            else:
                type = match.group(1)
                cmd = cmd[match.end() : ].strip()
            
        self.type = type
        if self.type == 'line':
            self.cmd = cmd
        elif self.type == 'char':
            self.cmd = None
            if len(cmd) == 0:
                self.cmd = '\n'
            elif len(cmd) == 1:
                self.cmd = cmd
            elif cmd.lower() in Command.glk_key_names:
                self.cmd = cmd.lower()
            elif cmd.lower() == 'space':
                self.cmd = ' '
            elif cmd.lower().startswith('0x'):
                self.cmd = unichr(int(cmd[2:], 16))
            else:
                try:
                    self.cmd = unichr(int(cmd))
                except:
                    pass
            if self.cmd is None:
                raise Exception('Unable to interpret char "%s"' % (cmd,))
        elif self.type == 'timer':
            self.cmd = None
        elif self.type == 'hyperlink':
            try:
                cmd = int(cmd)
            except:
                pass
            self.cmd = cmd
        elif self.type == 'mouse':
            try:
                ls = cmd.split()
                self.x = int(ls[0])
                self.y = int(ls[1])
                self.cmd = (self.x, self.y,)
            except:
                raise Exception('Mouse event must provide numeric x and y')
        elif self.type == 'refresh':
            self.cmd = None
        elif self.type == 'arrange':
            self.cmd = None
            self.width = None
            self.height = None
            try:
                ls = cmd.split()
                self.width = int(ls[0])
                self.height = int(ls[1])
            except:
                pass
        elif self.type == 'include':
            self.cmd = cmd
        elif self.type == 'fileref_prompt':
            self.cmd = cmd
        elif self.type == 'debug':
            self.cmd = cmd
        else:
            raise Exception('Unknown command type: %s' % (type,))
        self.checks = []
    def __repr__(self):
        return '<Command "%s">' % (self.cmd,)
    def addcheck(self, ln, linenum):
        args = { 'linenum':linenum }
        # First peel off "!" and "{...}" prefixes
        while True:
            match = re.match('!|{[a-z]*}', ln)
            if not match:
                break
            ln = ln[match.end() : ].strip()
            val = match.group()
            if val == '!' or val == '{invert}':
                args['inverse'] = True
            elif val == '{status}':
                args['instatus'] = True
            elif val == '{graphic}' or val == '{graphics}':
                args['ingraphics'] = True
            elif val == '{vital}':
                args['vital'] = True
            else:
                raise Exception('Unknown test modifier: %s' % (val,))
        # Then the test itself, which may have many formats. We try
        # each of the classes in the checkclasses array until one
        # returns a Check.
        for cla in checkclasses:
            check = cla.buildcheck(ln, args)
            if check is not None:
                self.checks.append(check)
                break
        else:
            raise Exception('Unrecognized test: %s' % (ln,))

class Check:
    """Represents a single test (applied to the output of a game command).

    This can be applied to the story, status, or graphics window. (The
    model is simplistic and assumes there is exactly one story window
    and at most one of the other two kinds.)

    An "inverse" test has reversed sense.

    A "vital" test will end the test run on failure.
    
    This is a virtual base class. Subclasses should customize the subeval()
    method to examine a list of lines, and return None (on success) or a
    string (explaining the failure).
    """
    inrawdata = False
    inverse = False
    instatus = False
    ingraphics = False
    showverbose = False

    @classmethod
    def buildcheck(cla, ln, args):
        raise Exception('No buildcheck method defined for class: %s' % (cla.__name__,))
    
    def __init__(self, ln, **args):
        self.linenum = args.get('linenum', None)
        self.inverse = args.get('inverse', False)
        self.instatus = args.get('instatus', False)
        self.ingraphics = args.get('ingraphics', False)
        self.showverbose = opts.verbose
        self.vital = args.get('vital', False) or opts.vital
        self.ln = ln
        
    def __repr__(self):
        val = self.ln
        if len(val) > 32 and not self.showverbose:
            val = val[:32] + '...'
        lnumflag = '' if self.linenum is None else ':%d' % (self.linenum,)
        invflag = '!' if self.inverse else ''
        if self.instatus:
            invflag += '{status}'
        if self.ingraphics:
            invflag += '{graphics}'
        detail = self.reprdetail()
        return '<%s%s %s%s"%s">' % (self.__class__.__name__, lnumflag, detail, invflag, val,)

    def reprdetail(self):
        return ''

    def eval(self, state):
        if not self.inrawdata:
            if self.instatus:
                lines = state.statuswin
            elif self.ingraphics:
                lines = state.graphicswin
            else:
                lines = state.storywin
        else:
            if self.instatus:
                lines = state.statuswindat
            elif self.ingraphics:
                lines = state.graphicswindat
            else:
                lines = state.storywindat
        res = self.subeval(lines)
        if (not self.inverse):
            return res
        else:
            if res:
                return
            return 'inverse test should fail'
    def subeval(self, lines):
        return 'not implemented'

class RegExpCheck(Check):
    """A Check which looks for a regular expression match in the output.
    """
    @classmethod
    def buildcheck(cla, ln, args):
        # Matches check lines starting with a slash
        if (ln.startswith('/')):
            return RegExpCheck(ln[1:].strip(), **args)
    def subeval(self, lines):
        for ln in lines:
            if re.search(self.ln, ln):
                return
        return 'not found'
        
class LiteralCheck(Check):
    """A Check which looks for a literal string match in the output.
    """
    @classmethod
    def buildcheck(cla, ln, args):
        # Always matches
        return LiteralCheck(ln, **args)
    def subeval(self, lines):
        for ln in lines:
            if self.ln in ln:
                return
        return 'not found'

class LiteralCountCheck(Check):
    """A Check which looks for a literal string match in the output,
    which must occur at least N times.
    """
    @classmethod
    def buildcheck(cla, ln, args):
        match = re.match('{count=([0-9]+)}', ln)
        if match:
            ln = ln[ match.end() : ].strip()
            res = LiteralCountCheck(ln, **args)
            res.count = int(match.group(1))
            return res
    def reprdetail(self):
        return '{count=%d} ' % (self.count,)
    def subeval(self, lines):
        counter = 0
        for ln in lines:
            start = 0
            while True:
                pos = ln.find(self.ln, start)
                if pos < 0:
                    break
                counter += 1
                start = pos+1
                if counter >= self.count:
                    return
        if counter == 0:
            return 'not found'
        else:
            return 'only found %d times' % (counter,)

class HyperlinkSpanCheck(Check):
    inrawdata = True
    @classmethod
    def buildcheck(cla, ln, args):
        match = re.match('{hyperlink=([0-9]+)}', ln)
        if match:
            ln = ln[ match.end() : ].strip()
            res = HyperlinkSpanCheck(ln, **args)
            res.linkvalue = int(match.group(1))
            return res
    def reprdetail(self):
        return '{hyperlink=%d} ' % (self.linkvalue,)
    def subeval(self, lines):
        for para in lines:
            for line in para:
                for span in line:
                    linkval = span.get('hyperlink')
                    text = span.get('text', '')
                    if linkval == self.linkvalue and self.ln in text:
                        return
        return 'not found'

class JSONSpanCheck(Check):
    inrawdata = True
    @classmethod
    def buildcheck(cla, ln, args):
        import ast
        match = re.match('{json (.*)}$', ln)
        if match:
            res = JSONSpanCheck(ln, **args)
            opts = match.group(1)
            ls = []
            while True:
                opts = opts.lstrip()
                if not opts:
                    break
                match = re.match('([a-z]+)\\s*:\\s*', opts)
                if not match:
                    raise Exception('{json} argument not recognized: %s' % opts)
                key = match.group(1)
                opts = opts[ match.end() : ]
                if opts.startswith('"'):
                    match = re.match('"([^"])*"', opts)
                    if not match:
                        raise Exception('{json} string has bad format: %s' % opts)
                    val = ast.literal_eval(opts[ : match.end() ])
                    opts = opts[ match.end() : ]
                elif opts.startswith("'"):
                    match = re.match("'([^'])*'", opts)
                    if not match:
                        raise Exception('{json} string has bad format: %s' % opts)
                    val = ast.literal_eval(opts[ : match.end() ])
                    opts = opts[ match.end() : ]
                else:
                    match = re.match('-?[0-9.]+', opts)
                    if match:
                        val = ast.literal_eval(opts[ : match.end() ])
                        opts = opts[ match.end() : ]
                    else:
                        match = re.match('[a-zA-Z0-9_]+', opts)
                        if match:
                            val0 = opts[ : match.end() ]
                            if val0 == 'true':
                                val = True
                            elif val0 == 'false':
                                val = False
                            elif val0 == 'null':
                                val = None
                            else:
                                val = ast.literal_eval(val0)
                            opts = opts[ match.end() : ]
                        else:
                            raise Exception('{json} literal has bad format: %s' % opts)
                ls.append( (key, val) )
            res.pairs = ls
            return res
    def subeval(self, lines):
        for para in lines:
            for line in para:
                for span in line:
                    got = True
                    for (key, val) in self.pairs:
                        if (key in span and span[key] == val):
                            continue
                        got = False
                        break
                    if got:
                        return
        return 'not found'

class ImageSpanCheck(Check):
    inrawdata = True
    @classmethod
    def buildcheck(cla, ln, args):
        match = re.match('{image=([0-9]+)([^}]*)}', ln)
        if match:
            ln = ln[ match.end() : ].strip()
            res = ImageSpanCheck(ln, **args)
            res.imagevalue = int(match.group(1))
            res.widthvalue = None
            res.heightvalue = None
            res.alignmentvalue = None
            res.xvalue = None
            res.yvalue = None
            opts = match.group(2)
            if opts:
                for val in opts.split(' '):
                    if not val:
                        continue
                    match = re.match('([a-z]+)=([a-z0-9]+)', val)
                    if not match:
                        raise Exception('{image} argument not recognized: %s' % val)
                    key = match.group(1)
                    val = match.group(2)
                    if key == 'width':
                        res.widthvalue = int(val)
                    elif key == 'height':
                        res.heightvalue = int(val)
                    elif key == 'alignment':
                        res.alignmentvalue = val
                    elif key == 'x':
                        res.xvalue = int(val)
                    elif key == 'y':
                        res.yvalue = int(val)
                    else:
                        raise Exception('{image} argument not recognized: %s' % key)
            return res
    def reprdetail(self):
        return '{image=%d} ' % (self.imagevalue,)
    def subeval(self, lines):
        for para in lines:
            for line in para:
                for span in line:
                    if span.get('special') == 'image' and span.get('image') == self.imagevalue:
                        if self.widthvalue is not None and span.get('width') != self.widthvalue:
                            continue
                        if self.heightvalue is not None and span.get('height') != self.heightvalue:
                            continue
                        if self.alignmentvalue is not None and span.get('alignment') != self.alignmentvalue:
                            continue
                        if self.xvalue is not None and span.get('x') != self.xvalue:
                            continue
                        if self.yvalue is not None and span.get('y') != self.yvalue:
                            continue
                        return
        return 'not found'

class GameState:
    """The GameState class wraps the connection to the interpreter subprocess
    (the pipe in and out streams). It's responsible for sending commands
    to the interpreter, and receiving the game output back.

    (The RemGlkSingle subclass doesn't maintain a running pipe-in and
    pipe-out, so those arguments are not passed in. Instead, we pass a
    set of interpreter arguments.)

    Currently this class is set up to manage exactly one each of story,
    status, and graphics windows. (A missing window is treated as blank.)
    This is not very general -- we should understand the notion of multiple
    windows -- but it's adequate for now.

    This is a virtual base class. Subclasses should customize the
    initialize, perform_input, and accept_output methods.
    """
    def __init__(self, infile, outfile, args=None):
        self.infile = infile
        self.outfile = outfile
        self.terpargs = args
        # Lists of strings
        self.statuswin = []
        self.graphicswin = []
        self.storywin = []
        # Lists of line data lists
        self.statuswindat = []
        self.graphicswindat = []
        self.storywindat = []
        # Gotta keep track of where each status window begins in the
        # (vertically) agglomerated statuswin[] array
        self.statuslinestarts = {}

    def initialize(self):
        pass

    def perform_input(self, cmd):
        raise Exception('perform_input not implemented')
        
    def accept_output(self):
        raise Exception('accept_output not implemented')

class GameStateCheap(GameState):
    """Wrapper for a simple stdin/stdout (dumb terminal) interpreter.
    This class never fills in the status window -- that's always blank.
    It can only handle line input (not character input).
    """

    def perform_input(self, cmd):
        if cmd.type != 'line':
            raise Exception('Cheap mode only supports line input')
        self.infile.write((cmd.cmd+'\n').encode())
        self.infile.flush()

    def accept_output(self):
        self.storywin = []
        output = bytearray()
        
        timeout_time = time.time() + opts.timeout_secs

        while (select.select([self.outfile],[],[],opts.timeout_secs)[0] != []):
            ch = self.outfile.read(1)
            if ch == b'':
                break
            output += ch
            if (output[-2:] == b'\n>'):
                break
            
        if time.time() >= timeout_time:
            raise Exception('Timed out awaiting output')
            
        dat = output.decode('utf-8')
        res = dat.split('\n')
        if (opts.verbose):
            for ln in res:
                if (ln == '>'):
                    continue
                print(ln)
        self.storywin = res
    
class GameStateRemGlk(GameState):
    """Wrapper for a RemGlk-based interpreter. This can in theory handle
    any I/O supported by Glk. But the current implementation is limited
    to line and char input, and no more than one graphics window.
    Multiple story (buffer) windows are accepted, but their output for
    a given turn is agglomerated. The same goes for multiple status (grid)
    windows.
    """

    @staticmethod
    def assert_json(dat):
        # Given a block of text, complain if it starts with lines that
        # don't look like JSON. Raises an exception contining the offending
        # lines.
        # Note that this doesn't check whether the text *is* JSON. (We
        # might get a partial JSON output.) We just want to make sure that
        # it starts with an open-brace.
        dat = dat.lstrip()
        badlines = []
        while dat and not dat.startswith('{'):
            ln, _, dat = dat.partition('\n')
            badlines.append(ln.rstrip())
            dat = dat.lstrip()
        if badlines:
            raise NotJSONException(*badlines)

    @staticmethod
    def extract_text(line):
        # Extract the text from a line object, ignoring styles.
        con = line.get('content')
        if not con:
            return ''
        dat = []
        i = 0
        while i < len(con):
            val = con[i]
            i += 1
            if type(val) is dict:
                dat.append(val.get('text', ''))
            else:
                dat.append(con[i])
                i += 1
        return ''.join(dat)
    
    @staticmethod
    def extract_raw(line):
        # Extract the content array from a line object.
        con = line.get('content')
        if not con:
            return []
        return con

    @staticmethod
    def create_metrics(width=None, height=None):
        if not width:
            width = 800
        if not height:
            height = 480
        res = {
            'width':width, 'height':height,
            'gridcharwidth':10, 'gridcharheight':12,
            'buffercharwidth':10, 'buffercharheight':12,
        }
        return res
    
    def initialize(self):
        import json
        update = { 'type':'init', 'gen':0,
                   'metrics': GameStateRemGlk.create_metrics(),
                   'support': [ 'timer', 'hyperlinks', 'graphics', 'graphicswin' ],
                   }
        cmd = json.dumps(update)
        self.infile.write((cmd+'\n').encode())
        self.infile.flush()
        self.generation = 0
        self.windows = {}
        # This doesn't track multiple-window input the way it should,
        # nor distinguish hyperlink input state across multiple windows.
        self.lineinputwin = None
        self.charinputwin = None
        self.specialinput = None
        self.hyperlinkinputwin = None
        self.mouseinputwin = None

    def perform_input(self, cmd):
        import json
        update = self.construct_remglk_input(cmd)
        cmd = json.dumps(update)
        self.infile.write((cmd+'\n').encode())
        self.infile.flush()
        
    def accept_output(self):
        import json
        output = bytearray()
        update = None

        timeout_time = time.time() + opts.timeout_secs

        # Read until a complete JSON object comes through the pipe (or
        # we time out).
        # We sneakily rely on the fact that RemGlk always uses dicts
        # as the JSON object, so it always ends with "}".
        while (select.select([self.outfile],[],[],opts.timeout_secs)[0] != []):
            ch = self.outfile.read(1)
            if ch == b'':
                # End of stream. Hopefully we have a valid object.
                dat = output.decode('utf-8')
                self.assert_json(dat)
                update = json.loads(dat)
                break
            output += ch
            if (output[-1] == ord('}')):
                # Test and see if we have a complete valid object.
                # (It might be partial, in which case we'll try again later.)
                dat = output.decode('utf-8')
                self.assert_json(dat)
                try:
                    update = json.loads(dat)
                    break
                except:
                    pass
                
        if time.time() >= timeout_time:
            raise Exception('Timed out awaiting output')

        self.parse_remglk_update(update)

    def construct_remglk_input(self, cmd):
        if cmd.type == 'line':
            if not self.lineinputwin:
                raise Exception('Game is not expecting line input')
            update = { 'type':'line', 'gen':self.generation,
                       'window':self.lineinputwin, 'value':cmd.cmd
                       }
        elif cmd.type == 'char':
            if not self.charinputwin:
                raise Exception('Game is not expecting char input')
            val = cmd.cmd
            if val == '\n':
                val = 'return'
            # We should handle arrow keys, too
            update = { 'type':'char', 'gen':self.generation,
                       'window':self.charinputwin, 'value':val
                       }
        elif cmd.type == 'hyperlink':
            if not self.hyperlinkinputwin:
                raise Exception('Game is not expecting hyperlink input')
            update = { 'type':'hyperlink', 'gen':self.generation,
                       'window':self.hyperlinkinputwin, 'value':cmd.cmd
                       }
        elif cmd.type == 'mouse':
            if not self.mouseinputwin:
                raise Exception('Game is not expecting mouse input')
            update = { 'type':'mouse', 'gen':self.generation,
                       'window':self.mouseinputwin, 'x':cmd.x, 'y':cmd.y
                      }
        elif cmd.type == 'timer':
            update = { 'type':'timer', 'gen':self.generation }
        elif cmd.type == 'arrange':
            update = { 'type':'arrange', 'gen':self.generation,
                       'metrics': GameStateRemGlk.create_metrics(cmd.width, cmd.height)
                       }
        elif cmd.type == 'refresh':
            update = { 'type':'refresh', 'gen':0 }
        elif cmd.type == 'fileref_prompt':
            if self.specialinput != 'fileref_prompt':
                raise Exception('Game is not expecting a fileref_prompt')
            update = { 'type':'specialresponse', 'gen':self.generation,
                       'response':'fileref_prompt', 'value':cmd.cmd
                       }
        elif cmd.type == 'debug':
            update = { 'type':'debuginput', 'gen':self.generation,
                       'value':cmd.cmd
                       }
        else:
            raise Exception('Rem mode does not recognize command type: %s' % (cmd.type))
        if opts.verbose >= 2:
            ObjPrint.pprint(update)
            print()
        return update

    def parse_remglk_update(self, update):
        # Parse the update object. This is complicated. For the format,
        # see http://eblong.com/zarf/glk/glkote/docs.html

        if opts.verbose >= 2:
            ObjPrint.pprint(update)
            print()

        self.generation = update.get('gen')

        windows = update.get('windows')
        if windows is not None:
            self.windows = {}
            for win in windows:
                id = win.get('id')
                self.windows[id] = win
            
            grids = [ win for win in self.windows.values() if win.get('type') == 'grid' ]
            totalheight = 0
            # This doesn't work if just one status window resizes.
            # We should be keeping track of them separately and merging
            # the lists on every update.
            self.statuslinestarts.clear()
            for win in grids:
                self.statuslinestarts[win.get('id')] = totalheight
                totalheight += win.get('gridheight', 0)
            if totalheight < len(self.statuswin):
                self.statuswin = self.statuswin[0:totalheight]
                self.statuswindat = self.statuswindat[0:totalheight]
            while totalheight > len(self.statuswin):
                self.statuswin.append('')
                self.statuswindat.append([])

        contents = update.get('content')
        if contents is not None:
            for content in contents:
                id = content.get('id')
                win = self.windows.get(id)
                if not win:
                    raise Exception('No such window')
                if win.get('type') == 'buffer':
                    self.storywin = []
                    self.storywindat = []
                    text = content.get('text')
                    if text:
                        for line in text:
                            dat = self.extract_text(line)
                            if (opts.verbose == 1):
                                if (dat != '>'):
                                    print(dat)
                            if line.get('append') and len(self.storywin):
                                self.storywin[-1] += dat
                            else:
                                self.storywin.append(dat)
                            dat = self.extract_raw(line)
                            if line.get('append') and len(self.storywindat):
                                self.storywindat[-1].append(dat)
                            else:
                                self.storywindat.append([dat])
                elif win.get('type') == 'grid':
                    lines = content.get('lines')
                    for line in lines:
                        linenum = self.statuslinestarts[id] + line.get('line')
                        dat = self.extract_text(line)
                        if linenum >= 0 and linenum < len(self.statuswin):
                            self.statuswin[linenum] = dat
                        dat = self.extract_raw(line)
                        if linenum >= 0 and linenum < len(self.statuswindat):
                            self.statuswindat[linenum].append(dat)
                elif win.get('type') == 'graphics':
                    self.graphicswin = []
                    self.graphicswindat = []
                    draw = content.get('draw')
                    if draw:
                        self.graphicswindat.append([draw])

        inputs = update.get('input')
        specialinputs = update.get('specialinput')
        if specialinputs is not None:
            self.specialinput = specialinputs.get('type')
            self.lineinputwin = None
            self.charinputwin = None
            self.hyperlinkinputwin = None
            self.mouseinputwin = None
        elif inputs is not None:
            self.specialinput = None
            self.lineinputwin = None
            self.charinputwin = None
            self.hyperlinkinputwin = None
            self.mouseinputwin = None
            for input in inputs:
                if input.get('type') == 'line':
                    if self.lineinputwin:
                        raise Exception('Multiple windows accepting line input')
                    self.lineinputwin = input.get('id')
                if input.get('type') == 'char':
                    if self.charinputwin:
                        raise Exception('Multiple windows accepting char input')
                    self.charinputwin = input.get('id')
                if input.get('hyperlink'):
                    self.hyperlinkinputwin = input.get('id')
                if input.get('mouse'):
                    self.mouseinputwin = input.get('id')

class GameStateRemGlkSingle(GameStateRemGlk):
    """Wrapper for a RemGlk-based interpreter in single-turn mode. That is,
    rather than keeping an interpreter open in the background, we launch
    it once for each input. RemGlk will autosave each turn and autorestore
    for the next turn.
    This has the same limitations as GameStateRemGlk.
    """
    
    def initialize(self):
        import json
        update = { 'type':'init', 'gen':0,
                   'metrics': GameStateRemGlk.create_metrics(),
                   'support': [ 'timer', 'hyperlinks', 'graphics', 'graphicswin' ],
                   }
        cmd = json.dumps(update)
        
        proc = subprocess.Popen(
            self.terpargs + [ '-singleturn', '--autosave' ],
            env=terpenv,
            bufsize=0,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        (outdat, errdat) = proc.communicate((cmd+'\n').encode(), timeout=opts.timeout_secs)
        self.pendingupdate = outdat.decode()
        
        self.generation = 0
        self.windows = {}
        # This doesn't track multiple-window input the way it should,
        # nor distinguish hyperlink input state across multiple windows.
        self.lineinputwin = None
        self.charinputwin = None
        self.specialinput = None
        self.hyperlinkinputwin = None
        self.mouseinputwin = None

    def perform_input(self, cmd):
        import json
        update = self.construct_remglk_input(cmd)
        cmd = json.dumps(update)
        
        proc = subprocess.Popen(
            self.terpargs + [ '-singleturn', '-autometrics', '--autosave', '--autorestore' ],
            env=terpenv,
            bufsize=0,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        (outdat, errdat) = proc.communicate((cmd+'\n').encode(), timeout=opts.timeout_secs)
        self.pendingupdate = outdat.decode()
        
    def accept_output(self):
        import json
        dat = self.pendingupdate
        self.assert_json(dat)
        update = json.loads(dat)
        self.pendingupdate = None

        self.parse_remglk_update(update)

class ObjPrint:
    NoneType = type(None)
    try:
        UnicodeType = unicode
    except:
        UnicodeType = str
    
    @staticmethod
    def pprint(obj):
        printer = ObjPrint()
        printer.printval(obj, depth=0)
        print(''.join(printer.arr))
    
    def __init__(self):
        self.arr = []

    @staticmethod
    def valislong(val):
        typ = type(val)
        if typ is ObjPrint.NoneType:
            return False
        elif typ is bool or typ is int or typ is float:
            return False
        elif typ is str or typ is ObjPrint.UnicodeType:
            return (len(val) > 16)
        elif typ is list or typ is dict:
            return (len(val) > 0)
        else:
            return True

    def printval(self, val, depth=0):
        typ = type(val)
        
        if typ is ObjPrint.NoneType:
            self.arr.append('None')
        elif typ is bool or typ is int or typ is float:
            self.arr.append(str(val))
        elif typ is str:
            self.arr.append(repr(val))
        elif typ is ObjPrint.UnicodeType:
            st = repr(val)
            if st.startswith('u'):
                st = st[1:]
            self.arr.append(st)
        elif typ is list:
            if len(val) == 0:
                self.arr.append('[]')
            else:
                anylong = False
                for subval in val:
                    if ObjPrint.valislong(subval):
                        anylong = True
                        break
                self.arr.append('[')
                if anylong:
                    self.arr.append('\n')
                first = True
                for subval in val:
                    if first:
                        if anylong:
                            self.arr.append((depth+1)*'  ')
                    else:
                        if anylong:
                            self.arr.append(',\n')
                            self.arr.append((depth+1)*'  ')
                        else:
                            self.arr.append(', ')
                    self.printval(subval, depth+1)
                    first = False
                if anylong:
                    self.arr.append('\n')
                    self.arr.append(depth*'  ')
                self.arr.append(']')
        elif typ is dict:
            if len(val) == 0:
                self.arr.append('{}')
            else:
                anylong = False
                for subval in val.values():
                    if ObjPrint.valislong(subval):
                        anylong = True
                        break
                self.arr.append('{')
                if anylong:
                    self.arr.append('\n')
                first = True
                keyls = sorted(val.keys())
                for subkey in keyls:
                    subval = val[subkey]
                    if first:
                        if anylong:
                            self.arr.append((depth+1)*'  ')
                    else:
                        if anylong:
                            self.arr.append(',\n')
                            self.arr.append((depth+1)*'  ')
                        else:
                            self.arr.append(', ')
                    self.printval(subkey, depth+1)
                    self.arr.append(':')
                    self.printval(subval, depth+1)
                    first = False
                if anylong:
                    self.arr.append('\n')
                    self.arr.append(depth*'  ')
                self.arr.append('}')
        else:
            raise Exception('unknown type: %r' % (val,))


checkfile_counter = 0

def parse_checkfile(filename):
    """Load a module containing extra Check subclasses. This is a nuisance;
    programmatic module loading is different in Py2 and Py3, and it's not
    pleasant in either.
    """
    global checkfile_counter
    
    modname = '_cc_%d' % (checkfile_counter,)
    checkfile_counter += 1

    fl = open(filename)
    try:
        if sys.version_info.major == 2:
            import imp
            mod = imp.load_module(modname, fl, filename, ('.py', 'U', imp.PY_SOURCE))
            # For checking the contents...
            classtype = types.ClassType
        else:  # Python3
            import importlib.util
            spec = importlib.util.spec_from_file_location(modname, filename)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            # For checking the contents...
            classtype = type
        for key in dir(mod):
            val = getattr(mod, key)
            if type(val) is classtype and issubclass(val, Check):
                if val is Check:
                    continue
                if val in checkclasses:
                    continue
                checkclasses.insert(0, val)
    finally:
        fl.close()

def parse_tests(filename):
    """Parse the test file. This fills out the testls array, and the
    other globals which will be used during testing.
    """
    global gamefile, terppath, terpargs, terpformat
    
    fl = open(filename)
    curtest = None
    curcmd = None
    linenum = 0

    while True:
        ln = fl.readline()
        if (not ln):
            break
        linenum += 1
        if py2_readline:
            ln = ln.decode('utf-8')
        ln = ln.strip()
        if (not ln or ln.startswith('#')):
            continue

        if (ln.startswith('**')):
            ln = ln[2:].strip()
            pos = ln.find(':')
            if (pos < 0):
                continue
            key = ln[:pos].strip()
            val = ln[pos+1:].strip()
            if not curtest:
                if (key == 'pre' or key == 'precommand'):
                    precommands.append(Command(val))
                elif (key == 'game'):
                    gamefile = val
                elif (key == 'interpreter'):
                    subls = val.split()
                    terppath = subls[0]
                    terpargs = subls[1:]
                elif (key == 'remformat'):
                    terpformat = 'rem' if (val.lower() > 'og') else 'cheap'
                elif (key == 'checkclass'):
                    parse_checkfile(val)
                else:
                    raise Exception('Unknown option: ** ' + key)
            else:
                if (key == 'game'):
                    curtest.gamefile = val
                elif (key == 'interpreter'):
                    subls = val.split()
                    curtest.terp = (subls[0], subls[1:])
                else:
                    raise Exception('Unknown option: ** ' + key + ' in * ' + curtest.name)
            continue
        
        if (ln.startswith('*')):
            ln = ln[1:].strip()
            if (ln in testmap):
                raise Exception('Test name used twice: ' + ln)
            curtest = RegTest(ln)
            testls.append(curtest)
            testmap[curtest.name] = curtest
            curcmd = Command('(init)')
            curtest.precmd = curcmd
            continue

        if (ln.startswith('>')):
            curcmd = Command(ln[1:])
            curtest.addcmd(curcmd)
            continue

        curcmd.addcheck(ln, linenum)

    fl.close()


def list_commands(ls, res=None, nested=()):
    """Given a list of commands, replace any {include} commands with the
    commands in the named subtests. This works recursively.
    """
    if res is None:
        res = []
    for cmd in ls:
        if cmd.type == 'include':
            if cmd.cmd in nested:
                raise Exception('Included test includes itself: %s' % (cmd.cmd,))
            test = testmap.get(cmd.cmd)
            if not test:
                raise Exception('Included test not found: %s' % (cmd.cmd,))
            list_commands(test.cmds, res, nested+(cmd.cmd,))
            continue
        res.append(cmd)
    return res

class VitalCheckException(Exception):
    pass
class NotJSONException(Exception):
    pass

def run(test):
    """Run a single RegTest.
    """
    global totalerrors

    testgamefile = gamefile
    if (test.gamefile):
        testgamefile = test.gamefile
    testterppath, testterpargs = (terppath, terpargs)
    if (test.terp):
        testterppath, testterpargs = test.terp
    
    print('* ' + test.name)
    args = [ testterppath ] + testterpargs + [ testgamefile ]
    proc = None
    if terpformat != 'remsingle':
        proc = subprocess.Popen(
            args,
            env=terpenv,
            bufsize=0,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    if terpformat == 'cheap':
        gamestate = GameStateCheap(proc.stdin, proc.stdout)
    elif terpformat == 'rem':
        gamestate = GameStateRemGlk(proc.stdin, proc.stdout)
    elif terpformat == 'remsingle':
        gamestate = GameStateRemGlkSingle(None, None, args=args)
    else:
        raise Exception('Unrecognized format: %s' % (terpformat,))

    cmdlist = list_commands(precommands + test.cmds)

    try:
        gamestate.initialize()
        gamestate.accept_output()
        if (test.precmd):
            for check in test.precmd.checks:
                res = check.eval(gamestate)
                if (res):
                    totalerrors += 1
                    val = '*** ' if opts.verbose else ''
                    print('%s%s: %s' % (val, check, res))
                    if check.vital:
                        raise VitalCheckException()
    
        for cmd in cmdlist:
            if (opts.verbose):
                if cmd.type == 'line':
                    if terpformat == 'cheap':
                        print('> %s' % (cmd.cmd,))
                    else:
                        # The input line is echoed by the game.
                        print('> ', end='')
                else:
                    print('> {%s} %s' % (cmd.type, repr(cmd.cmd),))
            gamestate.perform_input(cmd)
            gamestate.accept_output()
            for check in cmd.checks:
                res = check.eval(gamestate)
                if (res):
                    totalerrors += 1
                    val = '*** ' if opts.verbose else ''
                    print('%s%s: %s' % (val, check, res))
                    if check.vital:
                        raise VitalCheckException()

    except VitalCheckException as ex:
        # An error has already been logged; just fall out.
        pass
    except NotJSONException as ex:
        totalerrors += 1
        val = '*** ' if opts.verbose else ''
        print('%s%s, interpreter output:' % (val, ex.__class__.__name__))
        for ln in ex.args:
            print('  %s' % (ln,))
    except Exception as ex:
        totalerrors += 1
        val = '*** ' if opts.verbose else ''
        print('%s%s: %s' % (val, ex.__class__.__name__, ex))

    gamestate = None
    if proc:
        proc.stdin.close()
        proc.stdout.close()
        proc.kill()
        proc.poll()
    
    
checkclasses.append(RegExpCheck)
checkclasses.append(LiteralCountCheck)
checkclasses.append(HyperlinkSpanCheck)
checkclasses.append(ImageSpanCheck)
checkclasses.append(JSONSpanCheck)
checkclasses.append(LiteralCheck)
if (opts.checkfiles):
    for cc in opts.checkfiles:
        parse_checkfile(cc)

parse_tests(args[0])

if (len(args) <= 1):
    testnames = ['*']
else:
    testnames = args[1:]

if (opts.gamefile):
    gamefile = opts.gamefile
if (not gamefile):
    print('No game file specified')
    sys.exit(-1)

if (opts.terppath):
    subls = opts.terppath.split()
    terppath = subls[0]
    terpargs = subls[1:]
if (not terppath):
    print('No interpreter path specified')
    sys.exit(-1)
if (opts.remformat):
    terpformat = 'rem'
if (opts.terpformat):
    terpformat = opts.terpformat

terpenv = dict(os.environ)
if (opts.env):
    for val in opts.env:
        key, _, val = val.partition('=')
        if not val:
            val = '1'
        terpenv[key] = val
    
if (opts.precommands):
    for cmd in opts.precommands:
        precommands.append(Command(cmd))

testcount = 0
for test in testls:
    use = False
    for pat in testnames:
        if pat == '*' and (test.name.startswith('-') or test.name.startswith('_')):
            continue
        if (fnmatch.fnmatch(test.name, pat)):
            use = True
            break
    if (use):
        testcount += 1
        if (opts.listonly):
            print(test.name)
        else:
            run(test)
            if totalerrors and opts.vital >= 2:
                break

if (not testcount):
    print('No tests performed!')
if (totalerrors):
    print()
    print('FAILED: %d errors' % (totalerrors,))
    sys.exit(1)
