This repository contains the Dialog compiler and interactive
debugger, bundled with documentation and the Dialog Standard
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

	https://github.com/Dialog-IF/dialog/

Release notes:

	1b/01, Lib 1.1.1:
	
		Due to new built-in predicates in this release, all projects
		compiled for Å-machine will need version 1.0.0 or later of the
		Å-machine tools. Get them from github.com/Dialog-IF/aamachine/.
		
		Language: added (clear status bar).
		
		Compiler: access predicates consisting of a single multi-query
		no longer crash the compiler when queried with ~.
		
		Backend: improved text compression on Z-machine (at least for
		programs written in English), reducing text size by about 18%.
		This will help larger programs fit within the Z-machine's
		limits.
		
		Backend: a new --optimize-alphabet option improves dictionary
		word resolution on Z-machine for non-English writing systems.
		Previously, words in Greek or Russian could have as few as two
		characters in the essential part; this raises it to 9.
		
		Backend: added checks and diagnostics for Z-machine registers.
		
		Debugger: added a way to disable word-wrapping, for pipelines.
		
		Documentation: added cross-references to the syntax reference.
		
		Documentation: added "predicate-like" special syntax to the
		library reference. This includes anything involving words
		wrapped in parentheses, like (now) or (into $), but not ~{ }
		or @a/#b.
		
		Distribution: added unit.dg, a library for unit testing.

	1a/01, Lib 1.1.0:
	
		This is the first release handled by the community, and per
		Linus's wishes, we're bumping the major version for the first
		time! We're also taking this opportunity to unify the version
		numbers between the compiler, library, and manual.
		
		Backend: color and background-color properties are now supported
		on Z-machine backend.
		
		Backend: display:none is now tentatively supported on Z-machine
		backend, though spacing it properly can be difficult.
		
		Backend: styles can be stacked on Z-machine backend, allowing
		for (for example) bold italics, or monospace reverse-video.
		Spans are also allowed inside status bars as a result.
		
		Backend: fixed a bug that would crash the Z-machine if too many
		dictionary words were included in a wordmap, pushing the
		wordmap datatables past address $8000.
		
		Backend: lists containing punctuation marks will now print with
		proper spacing on Z-machine during tracing.
		
		Backend: Unicode characters are now allowed in dictionary words
		on Z-machine backend.
		
		Backend: generated Å-machine files now include the names of
		style classes instead of only the numbers, which interpreters
		may use to improve their output.
		
		Backend: resources can no include option strings on Å-machine
		backend; the meaning of these is left to the interpreter.
		
		Backend: the Z-machine backend can more effectively calculate
		the screen dimensions, even in certain buggy interpreters.
		
		Debugger: a new --numbered option shows the depth of the call
		stack with numbers instead of graphics during tracing. This
		makes the output significantly easier to understand with a
		screen reader.
		
		Language: fixed a bug when unifying a list containing multiple
		instances of a new variable, like ($Y = [$X $X]).
		
		Language: inline style predicates like (bold) are now deprecated.
		They're still supported for now, but may be removed in a future
		release.
		
		Language: new (current div width), (current div height), and
		(status bar $ with height $) predicates allow better control of
		status bar layout on Z-machine.
		
		Compiler: the compiler now shows how close a project is to
		various limits, both backend-specific (like addressable memory
		on the Z-machine) and universal (the compiler architecture
		imposes a limit of $1e00 objects on any backend).
		
		Compiler: the compiler now accepts JPEG files as well as PNG
		files for cover art.
		
		Compiler: in projects that include a library, objects used in
		rules but never declared as topics (the * syntax) will produce
		a warning. This can be disabled on the command line.
		
		Library: a new (gender-neutral $) trait will use the pronoun
		"they", but without forcing plural verb endings as (plural $)
		does.
		
		Library: serial commas in phrases like GET X, Y, AND Z are now
		properly understood. Previously, this would be parsed as GET X
		followed by TELL Y TO AND Z, which would generally produce an
		error.
		
		Library: "meta" text (that is, text like "enabling score
		notifications" that comes from the program itself, rather than
		from the game) is now always wrapped in a (div @meta), which
		the author is free to style as they like.
		
		Library: the program will backtrack to free heap memory before
		the final question, so that complicated parsing or status bars
		will no longer cause an overflow.
		
		Library: a new (startup) predicate is exhausted before printing
		the (intro). Library extensions can use this for computations
		that need to run at the start of play.
		
		Library: a default (appearance $) rule for pristine objects
		delegates to (initial appearance $), if defined.

	0m/03, Lib 0.46 (Manual revision 31):

		Library: Changed how darkness is handled: Unlit objects are now
		reachable, and a #darkness object is added to the scope.
		Updated the visibility, reachability, and scope rules
		accordingly.

		Library: Added support for clothes worn underneath other
		clothes. Updated the visibility, reachability, and scope rules
		accordingly.

		Library: Added '(wearing $ removes $)', '(wearing $ covers $)',
		and '($ goes underneath $)' for specifying how wearable objects
		interact with each other.

		Library: '(look $)' now falls back on '(descr $)' by default.

		Library: Searching animate objects is now prevented by default.

		Compiler: Improved typo detection. A warning is printed for
		each predicate that appears in just one of the following roles:
		As a rule definition, as a query, and as an interface
		declaration.

		Compiler: Don't include the standard library in the total
		linecount when deciding whether to warn about missing metadata.

		Aa-backend: Print a better error message when the output isn't
		a regular file.

		Documentation: Updated the section on light and darkness.
		Updated the section on reachability, visibility, and scope.
		Added a section about clothing.

		Documentation: Added a note about how the compiler looks for
		'(library version)' to identify the library.

	0m/02, Lib 0.45 (Manual revision 30):

		Library: SWITCH now only redirects to SWITCH ON if the object
		is switchable (and off).

		Library: EXITS now properly prints the name of an inherently
		dark room that is lit up by an object.

		Library: The output of '(list objects $ $)' no longer includes
		the current player.

		Library: Cleaned up the grammar rules to avoid ambiguous
		parsing of e.g. GET OUT as both '[leave]' and '[leave $]'.

		Library: Added PROFFER as a synonym for GIVE.

		Library: Added HEY as a synonym for GREET.

		Library: Asterisk-prefixed input is now consistently ignored,
		also when typed in response to disambiguation questions and in
		the game-over menu.

		Compiler: Improved the code for detecting cyclic access
		predicates.

		Debugger: Improved the algorithm for matching select statements
		when updating a running program.

		Debugger: Fixed an inconsistency in the interaction between
		'(uppercase)' and hyperlinks.

		Manual: Added clarifications on how to use '(plural name $)'
		and '(plural dict $)' and fixed the corresponding example in
		the section on identical objects.

		Manual: Miscellaneous improvements to Part I.

	0m/01, Lib 0.44 (Manual revision 29):

		Language: Added '(clear old)' to clear, from the main area, any
		text that the player has had a chance to read. Interpreters may
		ignore it.

		Language: Added '(clear div)' to clear, hide, or fold away the
		current div. Interpreters may ignore it.

		Language: Added '(inline status bar $) ...' for redirecting
		output to a separate status area that appears as an ordinary
		div inside the main area. The inline status bar is removed from
		display when a new one appears. Interpreters do not have to
		support status areas.

		Language: Added '(interpreter supports status bar)' and
		'(interpreter supports inline status bar)' to check for these
		features at runtime.

		Library: Certain actions (commands and '[inventory]' by
		default) now preserve the current implicit action as set up by
		'(asking for object in $)' or '(asking for direction in $)'.

		Library: The definition of takable has changed so that TAKE ALL
		no longer includes items inside a held container.

		Library: Changed '(narrate undoing)' to '(narrate undoing $)',
		where the parameter is the player input being undone, as a list
		of words. The default rule prints this.

		Library: Now uses noun heads to disambiguate when the grammar
		calls for any object (e.g. for FIND).

		Library: Close doors before locking them.

		Library: Fixed a bug that affected EXCEPT/BUT when parsing
		lists of objects.

		Library: Added '[takable child]' grammar token, used for TAKE
		ALL FROM.

		Library: It is now deemed unlikely to put something where it
		already is.

		Library: Refactored some rules, e.g. '(location headline)'.

		Frontend: Don't report more than ten interface violation
		warnings.

		Frontend: Increased maximum size of word-to-object maps.

		All backends: '(split word $ into $)' and '(get key $)' now
		return digits as numbers.

		Z-backend: Unicode bullet falls back on '*'.

		Aa-backend: Improved performance thanks to new Aa-machine 0.5
		features.

	(There is no language version 0l, for reasons of typography.)

	0k/06, Lib 0.43 (Manual revision 28):

		Library: Added a default perform-rule saying "You can't"
		followed by the action description. No more blank responses.

		Library: Changed '($ is opaque)' to '(opaque $)' and removed
		the alias '($ is transparent)'.

		Library: Changed the output of "exits" to make compass
		directions clickable if library links are enabled, and to use
		"the" instead of "a" for doors.

		Library: No longer understands "open" as any open object when
		default actions are enabled. This prevents "open door" from
		being ambiguously understood as '[open #door]' and '[examine
		#door]' when the door is open.

		Library: Added "hug" as a separate action from "kiss". Hug
		redirects to kiss by default, for backwards compatibility.

		Library: The default rules for NPC actions, '(let $NPC ...)',
		now invoke '(notice $NPC)' to set the pronouns.

		Library: Make object names clickable when enumerating a complex
		action (if library links are enabled).

		Library: When printing a group of fungible objects with
		'(the $)', don't create a hyperlink.

		Library: Prevent cycles in the object tree when the player
		attempts to pick up an object that is their ancestor, such as a
		seat.

		Library: The default rules for climb and enter now use
		'(move player to $ $)'.

		Library: No longer redraws the status bar twice when flowing
		between nodes in choice mode.

		Library: Miscellaneous bugfixes and optimizations.

		Debugger: Imposed a limit of 50 undo steps, to conserve memory.

		Manual: Added a section about creating custom grammar tokens.

		Manual: Updated an example in the Items chapter.

	0k/05, Lib 0.42 (Manual revision 27):

		Library: Added support for group actions.

		Library: The parser now deals properly with 'and' and ','
		inside object names, so that e.g. 'take black and white
		photograph' does not result in two separate attempts to take
		the same object.

		Library: '(The $)' and friends now list fungible objects with a
		definite article, i.e. 'the N items' instead of 'N items'.

		Library: Improved memory performance when working with fungible
		objects.

		Debugger: An informative message is printed when attempting to
		display memory statistics from within the debugger.

		Debugger: Fixed a bug that would occasionally crash the
		debugger when querying a dynamic flag for a new object.

		Compiler: Fixed a bug in the part of the optimizer that
		converts predicates to static flags.

		Z-backend: Now triggers a paragraph break after updating the
		status bar, which is consistent with other backends.

		Z-backend: Fixed a bug where '(no space)' would be ignored
		immediately after a unicode character.

		Z-backend: Fixed an off-by-one error in the progress bar.

	0k/04, Lib 0.41 (Manual revision 26):

		Library: Fixed a bug where objects immediately inside a closed
		opaque visibility ceiling weren't considered visible.

		Library: Open a closed container before stepping out of it.

		Library: Don't describe contents when opening a box from the inside.

		Library: Added '(Its $)'.

		Compiler: Warn if the last filename on the commandline does not
		refer to a library, i.e. a file that defines a rule for
		'(library version)'.

		Compiler: Allow '(now)' together with a double-negated flag,
		such as in '(now) ~($Obj is on)' when '@($ is on)' is an access
		predicate for '~($ is off)'.

		Debugger: Improved support for cursor keys in TTY driver.

		Manual: Removed wrongly formatted CSS comments.

	0k/03, Lib 0.40 (Manual revision 25):

		Library: Printing the name of each object when expanding a
		complex action, rather than spelling out the entire action.

		Library: Understanding DOWN, UP, and OUT as ambiguously
		referring to '[leave $]' and '[go $]' when the player is
		respectively on a supporter, in or on a seat, or in a
		container.

		Library: '(them $)' can now deal with a list argument.

		Library: Allowing '(from $ go $ to room $)' to be invoked with
		just the first parameter bound, to backtrack over all exits.

		Library: '(perform [leave $])' now uses '(move player to $ $)'
		instead of setting the location of the player object directly.

		Library: Updating the environment around the player (the cache
		variables) before drawing the status line.

		Debugger: Reimplemented and simplified the algorithm for
		matching select statements when updating a running program.

		Debugger: No longer crashes when encountering output boxes
		while collecting words.

		Aa-backend: Picking shorter opcodes in some situations.

	0k/02, Lib 0.39 (Manual revision 24):

		Compiler: Fixed a bug that prevented per-object variables from
		being initialized to complex values.

	0k/01, Lib 0.39 (Manual revision 24):

		Language: Structural matching of access predicate heads.

		Language: (accumulate $) ... (into $)

		Language: Added builtin '(fully bound $)' to check if a value
		is bound and, in case of a list, contains only fully bound
		elements.

		Language: The '(split $ by $ into $ and $)' built-in predicate
		now accepts a single keyword as its second parameter (or a list
		of keywords, as before).

		Language: The character '+' may now be used in the source-code
		names of objects and local variables.

		Language: Added special keypress words @\u, @\d, @\l, @\r, @\n,
		@\s, and @\b. Removed '(word representing up $)' and friends.

		Library: Simplified grammar rules, i.e. '(grammar $ for $)' as
		a complement to '(understand $ as $)'.

		Library: Remove duplicates from the list of choices in
		choice-mode.

		Library: Added intermediate action [switch $] that delegates to
		[switch on $] or [switch off $].

		Library: Relation objects (#on etc.) no longer have dictionary
		words. They appear directly inside action grammar rules
		instead, e.g. [put [held] on/onto [single]].

		Manual: Rewrote large parts of "Understanding player input".

		Manual: Clarified the use of conditional labels in choice mode.

		Manual: Reorganized some material, e.g. Debugging has moved to
		the "Input and output" chapter, and '(spell out $)' is now
		under "Miscellaneous features".

		Compiler: Bugfixes and improvements to the optimizer.

	0j/04, Lib 0.38 (Manual revision 23):

		Library: Undo is performed as an action, [undo], and reported
		via '(narrate undoing)'. It is still parsed as a special case.

		Library: When the player types an object name in response to a
		direct question (e.g. "To whom?"), don't understand it as a
		request to perform the default action (e.g. examine).

		Compiler: Fixed a bug that disallowed @-prefixed words in
		slash-expressions.

		Compiler: Several improvements to the optimizer.

	0j/03, Lib 0.37 (Manual revision 22):

		Library: Choice mode.

		Library: Added hooks that are queried early and late on every
		tick.

		Library: Undo now operates at the level of commandlines, rather
		than complex actions. "UNDO" must now be typed on a line of its
		own, but behaves in a less surprising way.

		Library: Before entering a closed container, try opening it.

		Compiler: Fixed rare bug related to variables bound inside
		if-statements.

		Compiler: Minor optimization of collect-into.

		Debugger: Removed spurious blank line when entering a status
		bar environment.

	0j/02, Lib 0.36 (Manual revision 21):

		Debugger: Removed stray warnings about singleton variables when
		merging changes to the running program.

		Library: Visibility is now recomputed after updating the
		current room variable and moving any floating objects.

	0j/01, Lib 0.35 (Manual revision 21):

		Language: '(link)' can now be followed by any kind of
		statement, not just a plain list of words.

		Language: A new builtin, '(clear links)', turns old hyperlinks
		into plain text, which is useful when the scope changes
		drastically, e.g. when moving to a different room.

		Language: Added spans as an inline complement to divs.

		Language: There are new builtins for splitting and joining
		words, and for checking if a word is listed in the game
		dictionary.

		Language: The word-separating characters are now ". , ; * ( )"
		and double-quote. These characters are treated as individual
		words during input parsing, and from now on also during
		'(collect words)' operations, even when they appear as part of
		larger words in the source code. When these single-character
		words are printed back as values, whitespace is inhibited
		before ". , ; )" and after "(".

		Language: '(append $A $B $AB)' is now built into the language,
		rather than defined by the library, for performance reasons.

		Language: Added support for interface declarations.

		Library: Added OOPS command, for fixing typos.

		Library: Fixed a bug in how reachability is determined.

		Library: Fixed a bug where understand-as-any-object didn't take
		the specified policy into account.

		Compiler: Added warnings about singleton variables.

		Compiler: Improved ability to trace unbound values, and warn
		about potential interface violations.

		Aa-backend: Improved performance thanks to new Aa-machine 0.4
		features.

		Manual: Moved the section about the pristineness of nested
		objects to the end of the Items chapter.

	0i/03 Lib 0.34 (Manual revision 20):

		Compiler: Fixed a bug where, under very specific circumstances,
		a register could get overwritten by an else-clause.

		Aa-backend: Fixed a bug where text containing non-ASCII
		characters would occasionally be converted to lowercase.

		Debugger: Fixed a bug in how UTF-8 input is divided into words.

		Manual: Added a clarification about the pristineness of nested
		objects, including the initial possessions of the player.

	0i/02 Lib 0.34 (Manual revision 19):

		Library: Fixed a bug in how the visibility ceiling was
		computed.

		Debugger: Removed spurious extra "Query succeeded" after each
		interactive query. Proper reporting of interactive queries to
		access predicates.

		Manual: Added missing multi-query asterisks to the fungibility
		examples.

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
