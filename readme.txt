This archive contains version 0g/06 of the Dialog compiler and interactive
debugger, bundled with documentation and version 0.29 of the dialog Standard
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
