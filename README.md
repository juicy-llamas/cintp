# C 'Interpreter'
I call it an intepreter to highlight how (hopefully) easy it will be to use. \
NOTE: in development, and it is...barely functional in it's current state. Do not use!

## Install
To create the binary, run `gcc -O2 c-interpreter-two.c -o cintp`. \
Then you can put the binary in `/usr/bin` or `/opt` or something, or create a `~/local/bin` and add that to your PATH for user executables.

## Use
Pressing enter upon running the application will throw you in a sort of text editor environment. There, you can just type C code, and when you're done, you can press Ctrl+D (EOF). After that, the code will be compiled and ran, and you can see the output. After you're done staring at the output, you can choose to edit the existing file (Ctrl+E), truncate the file and edit a new one (Ctrl+D), or quit (Ctrl+Q). It should be that simple. \\
To this end, the program will create two files in your directory: itmp.c and otmp, the first being where your code goes and the second being the output executable. The output is never saved (don't see a reason to, but easy enough to add), but you can save the input by specifying an option below.

#### Options 
You can only specify at most three program options as arguments, and the first argument that isn't a program option will be treated as the start of arguments to GCC.
- sav: when you press Ctrl+Q, your code will be saved (in itmp.c) rather than deleted, as it usually is.
- ndef: start with a blank file rather than a template
- bench: start with the benching template
- temp \[file name\]: use the file as your template
- help: opens help

## Other notes
- If you have a file called itmp.c in your current directory when you open the program, it will prompt you to recover the file or truncate it. Since the program will use itmp.c, you must choose one option.
