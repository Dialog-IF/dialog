This archive contains version 0i/01 of the Dialog compiler and interactive
debugger, bundled with documentation and version 0.33 of the Dialog Standard
Library.

Directory structure:

	readme.txt	This file.

	license.txt	License and disclaimer.

	src		Complete source code for the Dialog compiler and
			interactive debugger.

	prebuilt	Binaries for Linux (i386, x86_64) and Windows.

	docs		Documentation for the programming language and library.

	stdlib.dg	The Dialog standard library.

	stddebug.dg	The Dialog standard debugging extension.

Building the software under Linux (requires a C compiler and make):

	cd src
	make

	(this will produce two executable files called dialogc and dgdebug)

Cross-compiling the Windows version of the software under Linux (requires
mingw32):

	cd src
	make dialogc.exe dgdebug.exe

Project website:

	https://linusakesson.net/dialog/

Release notes:

	0i/01 Lib 0.33 (Manual revision 18):

		Language: The initial values for ($ has parent $) are derived
		from a compile-time multi-query.

		Language: New syntax: (link) { ... } for simple lists of words.

		Language: (log) { ... } for printing text in a different
		colour, and only when running in the interactive debugger.

		Library: The library defines no rewrite-rules anymore.

		Library: Short form of appearance, '(appearance $)'. Objects
		are noticed if the appearance predicate is queried succesfully.

		Library: New standard action: '[talk to $Obj about $Topic]'.
		Ask/tell redirect to it by default, and it in turn redirects to
		'[talk to $Obj]' by default.

		Library: Added '(list objects $ $)'.

		Library: Fixed several cases of a hardcoded "is" where
		plural-sensitivity was required.

		Library: Rewrote some of the code dealing with unlikely
		actions.

	0h/05 Lib 0.32 (Manual revision 17):

		Library: Facilities for implementing moving NPCs.

		Library: Topic objects. The unrecognized topic.

		Library: Added '(after $)' stage to action handling.

		Library: Disallow taking other people's clothing and
		possessions.

		Library: Narration callbacks for holding or wearing nothing.

		Library: New predicates for understanding words as any object
		(which must be located in a visited room, and not marked as
		hidden).

		Library: Made it possible to use '(from $ go $ to room $)'
		backwards, i.e. to specify the current and neighbouring rooms,
		and obtain the direction.

		Library: Adverbs for directions (e.g. above/below), and a
		predicate for obtaining the opposite of a direction.

		Library: Utility predicate to select a random element from a
		list.

		Library: Hook for inserting additional banner text.

		Library: Disambiguation will consider '(dict $)' synonyms.

		Library: Refinements to the path-finding algorithm. Added
		'(first step from $ to $ is $)', for when the complete path
		isn't needed.

		Manual: Documentation of the new library features, including a
		new NPC chapter.

		Manual: Brief mention of the Aa-machine 6502 interpreter, and
		an updated chart in the Software chapter.

		Z-backend: Now correctly enables auto-whitespace after a word
		that ends with a unicode character.

		Debugger: Properly deals with Delete and some other special
		keys.

	0h/04 Lib 0.31 (Manual revision 16):

		Library: Rewrote shortest-path algorithm to reduce memory
		footprint.

		Library: Reduced heap usage during nested disambiguation
		questions.

		Compiler: Improved information about unbound variables in -vvv
		output.

		Compiler: Added support for '(now) ~($ has parent $)', for
		completeness.

		Compiler: Reduced temporary register usage. Added a check to
		report an error if we're running out of registers.

		Compiler: Improved optimization of if-statements.

		Compiler: Fixed additional corner-case bugs discovered through
		fuzzing.

		Z-backend: Allows dynamic predicates to be unset for
		non-objects (nothing happens).

		Aa-backend: Generates smaller and faster code.

		Aa-backend: Collecting into a value (e.g. a list expression)
		now works properly.

		Aa-backend: Fixed a compiler error due to e.g. '(#a = $X)'.

		Debugger: Allows dynamic predicates to be unset for non-objects
		(nothing happens).

		Debugger: Now handles auxiliary heap overflow gracefully.

		Debugger: Correct handling of undo inside div, e.g. from the
		runtime error handler.

		Debugger: Fixed a bug related to single-digit input.

	0h/03 Lib 0.30 (Manual revision 16):

		Compiler: No longer crashes when trying to generate a zblorb
		that lacks certain metadata.

	0h/02 Lib 0.30 (Manual revision 16):

		Debugger: Results from interactive queries to *(split $ by $
		into $ and $) are displayed properly.

		Debugger: Attempts to define closures interactively are
		rejected, but no longer crash.

		Aa-backend: Improved code generation.

	0h/01 Lib 0.30 (Manual revision 15):

		Language: Added support for resources, such as pictures and
		external links. This includes two new syntactic elements,
		'(define resource $)' and '(link resource $)', and two built-in
		predicates, '(embed resource $)' and '(interpreter can
		embed $)'. This feature is primarily intended for use with the
		aa-machine backend.

		Language: Added runtime check '(interpreter supports quit)'.

		Library: The game-over menu is displayed differently, and the
		'quit' item only appears if '(interpreter supports quit)'
		succeeds. The normal 'quit' verb is still handled as before.

	0g/06 Lib 0.29 (Manual revision 14):

		Library: In the before-rules for eating and drinking, only
		attempt to pick up the indicated object if it is edible or
		potable.

		Library: Treat "X, tell me about Y" as "ask X about Y".

		Library: Fixed a bug where '(describe topic $)' couldn't deal
		with ambiguity.

		Compiler: Fixed a bug related to the optimization of nested
		disjunctions.

	0g/05 Lib 0.28 (Manual revision 14):

		Library and documentation: Added '(heads $)'.

		Debugger: Commandline option -L to disable hyperlinks in the
		output. This also affects '(interpreter supports links)'.

		Aa-backend: Now obeys the -s option for stripping away internal
		object names (hash tags) from the story file.

		Compiler: The quadruple-verbose output (-vvvv) now includes a
		list of all the words the story might print. With a bit of
		scripting, these can be sent to an external spell checker.

		Compiler: Fixed a bug where '(determine object $)' didn't
		accept integers in the input.

		Library: If parsing fails when default actions are enabled,
		don't assume that a default action was intended.

		Compiler: Fixed several corner-case bugs discovered through
		fuzzing.

	0g/04 Lib 0.27 (Manual revision 13):

		Debugger and aa-machine backend: Improved support for Unicode
		characters, including case conversion.

		Aa-backend bugfix: Non-ASCII characters are now treated
		properly during string encoding.

		Library: Added a new '(game over option)' predicate, for adding
		custom options to the game over menu.

		Compiler: Rephrased a confusing warning message.

		Documentation: Clarifications and minor updates.

	0g/03 Lib 0.26 (Manual revision 12):

		Z-machine backend: Added support for selecting the fixed-width
		font using CSS (font-family: monospace).

		Compiler bugfix: '(determine object $)' would sometimes return
		the same object twice.

		Debugger bugfix: '(uppercase)' followed by '(link $)' now works
		correctly.

		Z-machine backend: Bugfix in '(progress bar $ of $)'.

		Documentation: Minor updates.

	0g/02 Lib 0.26 (Manual revision 11):

		Re-release of 0g/01, including several files that were missing.

	0g/01 Lib 0.26 (Manual revision 11):

		Compiler: Aa-machine backend. Hyperlinks.

		Library: Hyperlink-related features.

	Library bugfix release 0.25:

		When parsing commands to a non-player character, understand
		nouns based on their relation to the actor rather than the
		player.

	0f/07 Lib 0.24 (Manual revision 10):

		Documentation: Added predicate index. Various minor
		improvements and clarifications.

		Library: Style class definitions use em and ch properly.

		Library bugfix: With fungibility enabled, under certain
		circumstances, object appearances didn't get printed.

		Z-machine backend: ASCII fallbacks for en-dash, em-dash, and
		three kinds of fancy quotes.

	0f/06 Lib 0.23 (Manual revision 9):

		Bugfix: Removed a case where the Z-machine backend would
		attempt to set an undefined style bit.

	0f/05 Lib 0.23 (Manual revision 9):

		Debugger: Fixed crashing bug when hot-reloading code with
		closures.

	0f/04 Lib 0.23 (Manual revision 9):

		Bugfix related to certain if-conditions.

	0f/03 Lib 0.23 (Manual revision 9):

		Compiler: Fixed a bug in the Z-machine backend where, under
		certain conditions, long dictionary words didn't get truncated
		at compile-time.

	0f/02 Lib 0.23 (Manual revision 9):

		Compiler: Fixed a bug that caused heavily nested conditional
		expressions to compile very slowly.

		Documentation: Removed two obsolete entries from the quick
		reference.

	0f/01 Lib 0.23 (Manual revision 8):

		Introduction of closures. Related changes in the standard
		library.

		Introduction of '(div $)' and style classes. Added '(unstyle)'.
		Div-based control of the status bar area. Added '(progress bar
		$ of $)'.

		Removed built-in predicates: '(par $)', '(status bar width $)',
		'(cursor to row $ column $)'.

		Removed '(unbound $)'. Added '(bound $)' with opposite
		semantics.

		Corresponding changes to the documentation.

		Bugfix: The transcript builtins are working again.

		Minor bugfixes and optimizations.

	0e/03 Lib 0.22 (Manual revision 7):

		Compiler: Internal restructuring and cleanup, as well as new
		optimizations.

		Library: Added '(them $)' for object pronouns (them, her, it,
		and so on).

		Debugger: Bugfix related to select and undo.

	0e/02 Lib 0.21 (Manual revision 6):

		Library: In object-based disambiguation, the answer is now
		matched against '(the full $)'.

		Library: Object-based disambiguation can now be undone with a
		single undo.

		Library: Scope is now computed on the fly, whereas the current
		visibility is represented with global variables. Query
		'(recompute visibility)' to update them. The new representation
		has better performance due to the changes in 0e/01.

		Library: All objects around the perimeter of a room (not just
		doors) are attracted to the room.

		Library: Objects around the perimeter of a room are no longer
		in scope if the player is unable to see them.

		Bugfixes and performance improvements in the internals of
		'(determine object $)'.

		Bugfix related to certain multi-queries in tail position.

		Bugfix: Reporting a number of syntax errors instead of
		asserting.

	0e/01 Lib 0.20 (Manual revision 5):

		Long-term heap for complex values stored in global and
		per-object variables. Removed the syntax for declaring a global
		variable with a maximum size.

		Removed '(collect words) / (and check $)'. Added
		'(determine object $) / (from words) / (matching all of $)'.

		Added support for dictionary words with essential and optional
		parts. Removed '(get raw input $)'.

		Library: Removed '(print raw input $)'. Added '(print words $)'
		and '(Print Words $)'. Adapted the parser to the new
		'(determine object $)' syntax.

		Various bugfixes in the debugger and compiler.

	0d/02 Lib 0.19 (Manual revision 4):

		A couple of bugfixes in the debugger.

	0d/01 Lib 0.19 (Manual revision 4):

		Introduced the Interactive Debugger, with corresponding
		modifications to the documentation.

		Added '(breakpoint)' built-in predicate.

		Library: Added '(actions on)', '(actions off)', and '(scope)'
		predicates to be queried from the debugger. The corresponding
		player-accessible commands remain in the debugging extension.

		Library: Modified the treatment of UNDO and AGAIN, to better
		support the interactive debugger.

		Library: Minor improvements.

	Library release 0.18:

		Added '(print raw input $)'.

	0c/05 Lib 0.17 (Manual revision 3):

		Added support for the .z5 output format.

		Bugfix: '(uppercase)' now works properly with dictionary words.

		Library: Improved a few default responses. Added '(narrate
		failing to look $Dir)'.

	0c/04 Lib 0.16 (Manual revision 2):

		Bugfix related to the allocation of a temporary register in a
		'has parent' optimization.

		Bugfix related to nested stoppable environments.

		Library: Added a synonym ('toss' for 'throw').

	0c/03 Lib 0.15 (Manual revision 2):

		Improved disambiguation: Now the library will ask the player to
		choose from a list of objects, if that makes all the
		difference. For more complicated situations, it falls back on a
		numbered list of actions.

		Miscellaneous compiler bugfixes.

	0c/02 Lib 0.14 (Manual revision 2):

		Compiler bugfix related to '(status bar width $)'.

	0c/01 Lib 0.14 (Manual revision 2):

		Added slash expressions, for specifying alternatives in rule
		heads. In the standard library, most synonyms are now handled
		directly by the understand-rules instead of being rewritten.

		Added a mechanism for infinite loops, '(repeat forever)'.
		Backends are no longer required to support tail-call
		optimizations (the Z-machine backend still does, of course, but
		a future debugging backend might not).

		Added stemming support for non-English games. During parsing,
		if a word isn't found in the dictionary, Dialog will attempt to
		remove certain word endings (typically declared by the library)
		and try again.

		Made it possible to specify the initial values of complex
		global variables.

		Added built-in predicate '(interpreter supports undo)'. The
		library can now avoid suggesting UNDO in the game over menu
		when undo is not available.

		Bugfix: FIND deals correctly with (not here $) objects.

		Additional compiler optimizations.

		Removed overly restrictive feature-test macros.

	Library bugfix release 0.13:

		Bugfix: Made it possible to (try [look]) from within (intro).

		Bugfix: Made it possible to drive vehicles from room to room.

	0b/01 Lib 0.12 (Manual revision 1):

		This is the first public release of Dialog.

		Dialog is currently in its beta stage, which means that the
		language may still undergo significant changes. It also means
		that you, dear potential story author, still have a substantial
		chance to influence what those changes will be.

		The source code for the compiler is currently rather messy, and
		I'm planning a major clean-up. However, it should be portable,
		and it works according to the language specification (as far as
		I know).

Happy authoring!
