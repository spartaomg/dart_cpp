# DART - Directory Art Importer for the Commodore 64 by Sparta/Genesis Project (C) 2022-2023

DART is a simple command-line tool that imports directory art from a variety of source file types to D64 disk images.

Usage:
------
dart input -o [output.d64] -s [skipped entries] -t [default entry type] -f [first imported entry] -l [last imported entry]

input - the file from which the directory art will be imported. See accepted file types below.

-o [output.d64] - the D64 file to which the directory art will be imported. This parameter is optional. If not
       entered, DART will create an input_out.d64 file. The output file name can also be defined in KickAss ASM
       input files.

-s [skipped entries] - the number of entries in the directory of the output.d64 that you don't want to overwrite.
       E.g. use 1 if you want to leave the first entry untouched. To append the directory art to the end of the
       existing directory, use 'all' instead of a numeric value. This parameter is optional. If not specified, the
       default value is 0 and DART will overwrite all existing directory entries.

-t [default entry type] - DART will use the file type specified here for each directory art entry, if not otherwise
       defined in the input file. Accepted values include: del, prg, usr, seq, *del, del<, etc. This parameter is
       optional. If not specified, DART will use del as default entry type. The -t option will be ignored if the
       type is specified in the input file (ASM or D64, see below).

-f [first imported entry] - a numeric value (1-based) which is used by DART to determine the first DirArt entry to be
       imported. This parameter is optional. If not specified, DART will start import with the first DirArt entry.

-l [last imported entry] - a numeric value (1-based which DART uses to determine the last DirArt entry to be imported.
       This parameter is optional. If not specified, DART will finish import after the last DirArt entry.

You can only import from one input file at a time. DART will overwrite the existing directory entries in the output
file (after skipping the number of entries defined with the -s option) with the new, imported ones, leaving only the
track:sector pointers intact. To attach the imported DirArt to the end of the existing directory, use the -s option
with 'all' instead of a number. This allows importing multiple DirArts into the same D64 as long as there is space
on track 18. DART does not support directories expanding beyond track 18. The new entries' type can be modified with
the -t option or it can be defined separately in D64 and KickAss ASM input files. You can also define which DirArt
entries you want to import from the input file using the -f and -l options. Both can take 1-based numbers as values
(i.e., 1 means first).The directory header and ID can only be imported from D64 and ASM files. All other input file types must only
consist of directory entries, without a directory header and ID. DART will always import the directory header and ID
from D64 and ASM files, except when the -s option is used with 'all' (append mode), in which case it will only import
them if they are not defined in the output file.

Accepted input file types:
--------------------------
D64  - DART will import all directory entries from the input file. The entry type, track and sector, and file size
       will also be imported. In overwrite mode the directory header and ID will be imported if they are defined
       in the input D64. In append mode they will only be imported if they are not defined in the output D64.

PRG  - DART accepts two file formats: screen RAM grabs (40-byte long char rows of which the first 16 are used as dir
       entries, can have more than 25 char rows) and $a0 byte terminated directory entries*. If an $a0 byte is not
       detected sooner, then 16 bytes are imported per directory entry. DART then skips up to 24 bytes or until an
       $a0 byte detected. Example source code for $a0-terminated .PRG (KickAss format, must be compiled):

           * = $1000              // Address can be anything, will be skipped by DART
           .text "hello world!"   // This will be upper case once compiled
           .byte $a0              // Terminates directory entry
           .byte $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$01,$02,$03,$04,$05,$06,$a0

       *Char code $a0 (inverted space) is also used in standard directories to mark the end of entries.

BIN  - DART will treat this file type the same way as PRGs, without the first two header bytes.

ASM  - KickAss ASM DirArt source file. Please refer to Chapter 11 in the Kick Assembler Reference Manual for details.
       The ASM file may contain both disk and file parameters within [] brackets. DART recognizes the 'filename',
       'name', and 'id' disk parameters and the 'name' and 'type' file parameters. If a 'filename' disk parameter is
       provided, it will overwrite the -o [output.d64] option. In overwrite mode the directory header and ID will be
       imported if they are defined in the input D64. In append mode they will only be imported if they are not
       defined in the output D64. If a 'type' file parameter is specified then it will be used instead of the default
       entry type defined by the -t option.
       Example:

           .disk [filename= "Test.d64", name="test disk", id="-omg-"]
           {
               [name = "0123456789ABCDEF", type = "prg"],
               [name = @"\$75\$69\$75\$69\$B2\$69\$75\$69\$75\$ae\$B2\$75\$AE\$20\$20\$20", type="del"],
           }

C    - Marq's PETSCII Editor C array file. This file type can also be produced using Petmate. This is essentially a
       C source file which consists of a single unsigned char array declaration initialized with the dir entries.
       If present, DART will use the 'META:' comment after the array to determine the dimensions of the DirArt.

PET  - This format is supported by Marq's PETSCII Editor and Petmate. DART will use the first two bytes of the input
       file to determine the dimensions of the DirArt, but it will ignore the next three bytes (border and background
       colors, charset) as well as color RAM data. DART will import max. 16 chars per directory entry.

JSON - This format can be created using Petmate. DART will import max. 16 chars from each character row.

PNG  - Portable Network Graphics image file. Image input files can only use two colors (background and foreground).
       DART will try to identify the colors by looking for a space character. If none found, it will use the darker
       of the two colors as background color. Image width must be exactly 16 characters (128 pixels or its multiples
       if the image is resized) and the height must be a multiple of 8 pixels. Borders are not allowed. Image files
       must display the uppercase charset and their appearance cannot rely on command characters. DART uses the
       LodePNG library by Lode Vandevenne to decode PNG files.

BMP  - Bitmap image file. Same rules and limitations as with PNGs.

Any other file type will be handled as a binary file and will be treated as PRGs without the first two header bytes.

Example 1:
----------

dart MyDirArt.pet

DART will create MyDirArt_out.d64 (if it doesn't already exist), and will import all DirArt entries from MyDirArt.pet
into it, overwriting all existing directory entries, using del as entry type.

Example 2:
----------

dart MyDirArt.c -o MyDemo.d64

DART will import the DirArt from a Marq's PETSCII Editor C array file into MyDemo.d64 overwriting all existing
directory entries, using del as entry type.

Example 3:
----------

dart MyDirArt.asm -o MyDemo.d64 -s all

DART will append the DirArt from a KickAss ASM source file to the existing directory entries of MyDemo.d64.

Example 4:
----------

dart MyDirArt1.png -o MyDemo.d64 -s 1 -t prg

dart MyDirArt2.png -o MyDemo.d64 -s all -f 3 -l 7

DART will first import the DirArt from a PNG image file into MyDemo.d64 overwriting all directory entries except
the first one, using prg as entry type. Then DART will append entries 3-7 from the second PNG file to the directory
of MyDemo.d64 (keeping all already existing entries in it), using del as entry type.
