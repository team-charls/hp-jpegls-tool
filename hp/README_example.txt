JPEG-LS Test Program

Command Line Interface to the JPEG-LS DLL

jpeglstest is an example program which uses the JPEG-LS DLL to read and
write JPEG-LS image files.

*************************************************************************
** usage:
**

Running jpeglstest.exe with no parameters displays this message:

	JPEG-LS library test, (c) 1999 HPL
	jls [options] <in> [out]
	        (does enc & dec if no outfile is specified)
	options:
	   -t : do a thorough timing
	encoder options: [with default]
	   -p : pixel-interleave [line-int]
	   -e#: allow error of up to # [0]
	   -x#: do color xform # [0]
	   -cF: do custom color xform from file F

The option -t applies to the encoder and decoder; all others
affect encoder operation only.  The encoder options are shown
with the default setting in brackets.

Several usage modes are supported.  They are:

	1. Compress an image :
		jpeglstest [options] input.raw output.jls

	2. Decompress a JPEG-LS file :
		jpeglstest [options] input.jls output.raw

	3. Test the compression on an image :
		jpeglstest [options] input.raw

Options may be enterred anywhere on the command line (see later for a description
of all the options).

All images written and read are 'raw' format.  That means they have no headers,
simply pure 8bit image data.  Multi-compenent raw images are pixel-interleaved
(for example RGBRGB).  The test program tries to figure out the dimensions of the
raw image from the file size; it favours dimensions which are powers of two.  You
may put the dimensions of the image in the filename (for example : bike_768x413.raw)
to override this algorithm.

*************************************************************************
** options:
**

-t : runs the coder repeatedly, and averages the durations to create a more
	accurate timing report.  This is critical under Windows where timings may
	vary greatly due to multi-tasking abnormalities.  To make sure that times
	are accurate, the test program should be run several times with this
	option, and outliers eliminated.


-p : switches multi-component files to pixel-interleaved mode (instead of the
	default line interleaved).  Plane-interleaved mode is not accessible from
	the command-line options.

-e# : near-lossless operation with a maximum error of '#'.
	(for example, "-e 2")

-x# : do one of the built-in color transforms for 3-component color images.
	A parameter of 0 means no transform; values 1-4 indicate (in order:)
		COLORXFORM_HP1
		COLORXFORM_HP2
		COLORXFORM_HP3
		COLORXFORM_RGB_AS_YUV_LOSSY
	(see the JPEGLS DLL documentation for more information)

-cF : specifies a file which contains a user-specified color transform (forward
	and inverse).  Sets the JPEG-LS colorXForm to COLORXFORM_MATRIX.
	(for exaple, "-c hp1.xf")

The file format for the color transform files is as follows :

	rows of :
		"is dc?" "fractional bits for inverse"
	rows of forward matrix ( times 65536 )
	rows of inverse matrix ( shifted by the number of fractional bits )

so the simple XForm
	R -= G
	B -= G

is specified by the following file :

	1 0
	0 0
	1 0
	65536	-65536	0
	0		 65536	0
	0		-65536	65536
	1	1	0
	0	1	0
	0	1	1

See the JPEGLS DLL documentation for more information on the meaning and
usage of these entries.


*************************************************************************
** Credits
**

JPEG-LS is based on the LOCO-I algorithm developed by Marcelo Weinberger,
Gadiel Seroussi, and Guillermo Sapiro of Hewlett-Packard Laboratories.
See http://www.hpl.hp.com/loco .

The JPEG-LS DLL was developed by Charles Bloom at HP Labs, based on
HP Labs' JPEG-LS implementation.
