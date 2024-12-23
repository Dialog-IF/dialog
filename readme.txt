This is version 0b/01 of the Dialog compiler, bundled with version 0.12 of its
standard library.

Directory structure:

	readme.txt	This file.
	license.txt	License and disclaimer (standard 2-clause BSD).

	src		Complete source code for the Dialog compiler.
	prebuilt	Binaries for Linux (i386, x86_64) and Windows.

	docs		Documentation for the programming language and library.

	stdlib.dg	The Dialog standard library.
	stddebug.dg	The Dialog standard debugging extension.

Building the compiler (requires a C compiler and make):

	cd src
	make

	(this will produce an executable file called dialogc)

Project website:

	https://linusakesson.net/dialog/

Release notes:

	0b/01 Lib 0.12 (Manual revision 1):

	This is the first public release of Dialog.

	Dialog is currently in its beta stage, which means that the language
	may still undergo significant changes. It also means that you, dear
	potential story author, still have a substantial chance to influence
	what those changes will be.

	The source code for the compiler is currently rather messy, and I'm
	planning a major clean-up. However, it should be portable, and it works
	according to the language specification (as far as I know).

Happy authoring!
