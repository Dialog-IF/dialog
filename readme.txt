This archive contains version 0d/02 of the Dialog compiler and interactive
debugger, bundled with documentation and version 0.19 of the dialog Standard
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
