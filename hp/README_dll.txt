JPEG-LS DLL Version 1.0BETA

Win32 Dynamic Library for JPEG-LS

************************************************************************
** general help
**

The JPEGLS DLL can be used on Windows 9x or NT systems. The library was
created with Microsoft's Visual C.

necessary files :

jpegls.dll
jpegls.lib
jpeglsdll.h

installation :

jpegls.dll should placed be in your windows\system or winnt\system
directory. jpegls.lib and jpeglsdll.h should be placed where your
compiler can find them.

usage :

To access the dll, link with jpegls.lib and call functions in
jpeglsdll.h All data types and functions are in the jpeglsdll header,
with some comments.

************************************************************************
** coding with the DLL
**

All JPEG-LS operations go through a JPEGLS object. This object contains
all the information about the image and the IO methods. You may have as
many of these objects active as you please.

Users of the DLL always begin with a call to JPEGLS_Create() , to create
a new JPEGLS object. When you are done with the object, you must call
_Destroy() to destroy it. (since all library functions begin with the
prefix JPEGLS_, this prefix will be left off in this tutorial).

You should then call _SetMessageCallback() as soon as possible, to give
the library a pathway for displaying critical errors to the user. An
error return will percolate out of the library in the form of a 'false'
return from a bool-type function.

Now, if you are decoding, you can call _StartDecode. You must provide
callbacks for the library to read from the compressed data stream.
Callbacks that read and write from stdio files are included in the
sample source. Once you have called _StartDecode(), you may now call
_GetInfo() to retreive information about the image. At this point, the
JPEGLS object is set up for decoding a scan, and it will stay that way
until you retreive all the lines in the scan. See later on coding scans.
There are several GetDescription utility functions which will give you a
text string describing some of the numerical parameters in the info
structures.

If you are encoding, you must set up an Info structure to pass to
_StartEncode. The JPEGLS_Info structure contains all the information
about the scan lines that you will pass to the library. The easiest way
to fill this structure is to first call _GetDefaultInfo() to fill your
copy of the info structure with valid entries. Now, simply enter the
parameters which you know you want to override. For example, width and
height should always be filled out. Note that you may wish to change
some of the settings in the scans as well, such as the components or the
loss. Once you have the desired info structure, you may call
_StartEncode(). Once this is done, you simply pass scan lines of pixels
to the JPEGLS object until the coding scan is complete. See later for
information on coding scans and the members of the info structures.

Coding a scan : once your JPEGLS object is started on a coding scan,
must keep coding that scan until you have processed all the scan lines.
You may destroy the JPEGLS object at any time to prematurely terminate a
scan, but you cannot start a new one until a scan is completed. You
cannot mix encoding and decoding operations without a new call to
_StartEncode or _StartDecode. You should either call the 8b (eight bit,
ubyte) or 16b (16 bit, uword) coding routines depending on whether your
alphabet (as specified in the info structure) is greater than 256 or
not.

All pixel scan lines must be in component-interleaved order. For one
component scans, this is irrelevant, and all interleave types are the
same. NOTEZ : the interleave variable in the JPEGLS_ScanInfo applied to
the JPEGLS file, NOT to the scanlines which the you provide to the
library! The scanlines which come in or go out of the library are always
interleaved with all the components in that scan. For example, a 3
component scan will be in the form 123123, et cetera.

The fastest way to interface the DLL is by using the EncodeFromCB or
DecodeToCB functions; these functions use a callback to read or write
(respectively) scan lines from the client. This pathway avoids expensive
repeated DLL entry.

If you wish to code the lines one by one, you may call _EncodeLine() or
_DecodeLine() repeatedly. The DecodeLine functions fill out a pointer to
a scan line. This pointer is valid only until the next JPEGLS method is
called on that object, eg. it is not persistent. You should copy the
data out of it (or whatever) before you call DecodeLine again. When
using EncodeLine, you may retreive a workspace using _GetEncodeLine(),
which you may then pass back to the _EncodeLine() function. This method
is slightly faster than using your own workspace, but the same warnings
apply to this workspace as did to the line returned by DecodeLine : you
must call _GetEncodeLine() again after each call to _EncodeLine().

************************************************************************
** The Info structures
**

Communication with the JPEGLS coder is done through two info structures
: the per-image JPEGLS_Info structure, and the per-scan JPEGLS_ScanInfo
structure. Prior to encoding (when you call StartEncode), you must set
these up to tell the encoder how to process your data; similarly the
decoder will provide these to you in a GetInfo call any time after you
do a StartDecode. The fields are:

basic types used in the JPEGLS DLL :

	ulong		:	32-bit unsigned int
	uword		:	16-bit unsigned int
	ubyte		:	a byte, an 8-bit unsigned int
	uint		:	generic positive-only integer
	bool		:	boolean; has value true or false
					also indicates success or failure of function calls

JPEGLS_Info :

	width,height	:	the size of the un-sampled image
	alphabet	:	the number of pixel values; typically 256
				if alphabet > 256, then the lines are 16 bit
	components	:	the total number of color planes in the image

	doRestart (bool):	tells whether or not to send restart markers in 
				the compressed stream; they are sent every 
				restartInterval lines

	samplingX,samplingY : specified for each component.  The sampling is 
				the number of pixels in each direction per 
				minimum sampled size.  For example, to subsample
				the 2nd component by two in X, you would set 
				samplingX[0] = 2,  samplingX[1] = 1 to specify 
				that the 1st component has twice as many x 
				pixels as the 2nd.
				note: 	the lines you get & receive from the 
				DLL are un-sampled; it does the down or 
				up-sampling.

	componentId	:   	transmitted but ignored by this implementation

	scanCount	:	the number of scans

JPEGLS_ScanInfo :
	interleave	:	specifies the interleave type for multiple 
				components.  Use INTERLEAVE_LINE unless you are
				wise in the ways  of JPEG-LS
	loss		:	specifies the error allowed in near-lossless 
				mode. set loss=0 for lossless transmission.
	shift		:	transmitted but not supported
	thresholds	:	the gradient thresholds for context formation.
				do not change unless you know better.
	resetCount	:	the occurance count at which contexts are scaled
				down.  do not change unless you know better.
	colorSpace	:	the color space of the image planes, chosen from 
				the enum typedef JPEGLS_ColorSpace.  Informative
				only.  This is not part of the JPEG-LS standard.
				See below for more information.
	colorXForm	:	a color transform of the image planes, chosen
				from the enum typedef JPEGLS_ColorXForm.  This 
				is not part of the JPEG-LS standard; see below.
				If colorXForm == COLORXFORM_MATRIX, then the 
				Matrix fields are filled out as well.
	alphabet	:	should be the same as in the main Info for this 
				implentation.
	components	:	the number of components in this scan.  The 
				number of components in all the scans should 
				sum up to the number in the main Info.
	componentId	:   	transmitted but ignored by this implementation
	hasTable, etc.	:	optionally specifies a color-remapping table.  
				The remapping is not done automatically, you 
				must do it if you want it.  Typically used for 
				sending palettes of indexed-color images.

Warning : in this implementation, configuration parameters in the Scan
(such as the thresholds, and resetCount) should not differ from one scan
to the next.

It is highly recommended that you never fill out a JPEGLS_Info from
scratch. Use JPEGLS_GetDefaultInfo to fill all values with typical
parameters.

************************************************************************
** Color Transforms
**

Two non-standard fields are provided in the JPEGLS_ScanInfo : colorSpace
and colorXForm. These are transmitted using JPEG application markers,
which can be interpretted by our decoder, but will be ignored by other
decoders. If an image is recompressed with another JPEGLS coder, this
information may be lost. See later on how your JPEGLS coder can read
these markers.

The colorSpace is an optional informative field. It allows applications
to communicate about the meaning of the planes in the JPEGLS file (eg.
are they RGB, or CMYK, etc.). The possible values are specified in the
JPEGLS_ColorSpace enum type in the jpeglsdll header.

The colorXForm specifies a transform performed on the color planes. This
transform is performed in the DLL, a forward transform for each
EncodeLine, and an inverse for each DecodeLine. There are four built-in
transforms, and a colorXForm value of COLORXFORM_MATRIX which means the
user has specified a custom lifting transform matrix to rotate the color
space. The built in transforms all act on 3-component images. If we name
the three components, R, G, & B, then the four (forward) transforms are:

COLORXFORM_HP1 :
	G = G
	R = R - G
	B = B - G

COLORXFORM_HP2 :
	G = G
	B = B - (R + G)/2
	R = R - G
	
COLORXFORM_HP3 : (a lossless version of Y-Cb-Cr)
	R = R - G
	B = B - G
	G = G + (R + B)/4

COLORXFORM_RGB_AS_YUV_LOSSY :
	The standard lossy RGB to YCbCr transform used in JPEG.

The setting COLORXFORM_MATRIX indicates that the user is providing a
custom color transform. The user must then fill out the following
fields:

	colorMatrixForward :
		a normal matrix specifying the forward transform.  Value are 
		divided by 65536 before use (that is, values should be entered 
		as fixed-point fractions with the decimal at the 16th bit).
		This matrix is not transmitted, and colorMatrixBits does not 
		affect this transform.

	colorMatrixInverse :
		a lifting matrix for the inverse transform.  That means this is
		like a normal matrix multiply, but the input values are replaced
		by the results after each row multiplication.  In other words,
		the nth component is built from (1,2..n-1) post-transform values
		and (n+1,..N) pre-transform values.  The decimal point is in the
		bit position specified by "colorMatrixBits".  Notez : the 
		diagonal components are always the identity, and are not 
		transmitted!

	colorMatrixDC :
		specifies whether this transformed component is AC (near zero)
		or DC (like an image component).  The AC values should have
		distributions around zero (eg. like R-G) and the DC values
		should have distributions around 128 (like G).

	colorMatrixBits :
		specifies the number of fractional bits in the inverse matrix.

For example, to create the transform "HP1", one would fill out these
fields as follows (pseduo-code) :

	colorMatrixForward[0] 	= { 65536,	- 65536,	0		}  	// r = R - G
	colorMatrixForward[1] 	= { 0	,	  65536,	0		}  	// g = G
	colorMatrixForward[2] 	= { 0	,	- 65536,	65536	} 	// b = B - G

	colorMatrixDC			= { false, true, false }			// g is DC, r and b are AC

	colorMatrixBits			= { 0, 0, 0 }						// no fractional bits 

	colorMatrixInverse[0]	= { 1, 1, 0 }						// R = r + g
	colorMatrixInverse[1]	= { 0, 1, 0 }						// G = g
	colorMatrixInverse[2]	= { 0, 1, 1 }						// B = b + G

************************************************************************
** Color Transform Application Markers & Code Stream
**

The colorspace is stored in JPEG app marker 0xFFE7.  The total frame
length is 7.  The 5 bytes of data in the frame consist of :
	a 4 byte magic number, 0x726C6F63 (colr), to make sure this is our
		application marker
	the 1 byte colorSpace enum value

The colorXForm is stored in JPEG app marker 0xFFE8.	The frame length is
7 (plus any bytes need for the inverse matrix). The frame consists of :

	a 4 byte magic number, 0x6D726678 (xfrm)
	the 1 byte colorXForm enum value
	the optional inverse transform matrix 
		(if colorXForm == COLORXFORM_MATRIX )

The color transform matrix is transmitted bit-wise using the standard
JPEG-LS bit-io mechanics. For each row, we write bits as follows :

	one bit for the AC/DC boolean (on = DC)
	five bits for the number of fractional bits in the coefficients (F)
	for each coefficient :
		the integer part is sent in unary
		the fractional part is sent in F bits
		if the value is non-zero, a sign bit is sent (on = negative)

************************************************************************
** Overview of all the methods
**

JPEGLS * JPEGLS_Create(void);

	Creates an encapsulated JPEGLS object.

void JPEGLS_Destroy(JPEGLS * j);

	Destroys a JPEGLS object. You should _Destroy every JPEGLS you
	_Create.

bool JPEGLS_CheckOk(JPEGLS * j);

	Returns a true/false for ok/notok.  If not ok, an error message is 
	displayed via the message call back.

void JPEGLS_SetMessageCallback(JPEGLS * j,JPEGLS_MessageCallback mcb,
								void * messageContext);

	Sets a callback for displaying error messages.

bool JPEGLS_GetInfo(const JPEGLS * j,JPEGLS_Info *jinfo);

	Fills out the jinfo structure with the current JPEGLS stream's
	specifications.  Returns false if called before StartDecode.

void JPEGLS_GetDefaultInfo(JPEGLS_Info *jinfo);

	Fills out the jinfo structure with typical & valid parameters.

bool JPEGLS_IsStreamJLS(JPEGLS_ReadBufCallback rcb,void * readContext);

	Reads speculatively from the 'rcb' input stream.  Returns true/false
	to indicate whether the file is a valid JPEGLS stream.  Does not 
	reset the input stream.

bool JPEGLS_StartDecode(JPEGLS * j, JPEGLS_ReadBufCallback rcb,
						void * readContext );

	Begins decoding from the specified input callback rcb.  Reads the 
	JPEGLS_Info.

bool JPEGLS_StartEncode(JPEGLS * j,JPEGLS_WriteBufCallback wcb,
						void * writeContext,const JPEGLS_Info *jinfo);

	Begins writing to the output callback wcb.  Writes the info 
	specified in jinfo.

void * JPEGLS_GetEncodeLine(JPEGLS * j);

	Gives you a line to work into and then pass to _EncodeLine
	You must re-call this after each EncodeLine, it is not a persistent
	pointer.  (This is faster that using your own buffer).

bool JPEGLS_EncodeLine8b (JPEGLS * j,ubyte * line);
bool JPEGLS_EncodeLine16b(JPEGLS * j,uword * line);

	Encode a scan line of pixel-interleaved data. All lines passed to
	JPEGLS are component interleaved (eg. RGBRGB..) regardless of how
	the scan's interleave value is set. All 16-bit lines (eg. 
	alphabet > 256) should be valid integers in the current machine's
	convention. Warning : EncodeLine may modify your data after you
	pass it in.

bool JPEGLS_DecodeLine8b (JPEGLS * j,ubyte ** pLine);
bool JPEGLS_DecodeLine16b(JPEGLS * j,uword ** pLine);

	The decode functions give you a pointer to a line which is
	valid only until any other JPEGLS function is called.  See the 
	notes in EncodeLine about how the lines are stored.

bool JPEGLS_EncodeFromCB (JPEGLS * j,JPEGLS_ReadBufCallback  rcb,
												void *rcbContext);
bool JPEGLS_DecodeToCB   (JPEGLS * j,JPEGLS_WriteBufCallback wcb,
												void *wcbContext);

	These functions make repeated calls to EncodeLine or DecodeLine 
	until all the data is coded.  They pass the uncoded pixel data 
	through the specified I/O callbacks.  This is the recommended
	pathway when using the DLL, because of the large time cost of DLL
	function calls.

const char * JPEGLS_GetInterleaveDescription(JPEGLS_Interleave 
														interleave);
const char * JPEGLS_GetColorXFormDescription(JPEGLS_ColorXForm xf);
const char * JPEGLS_GetColorSpaceDescription(JPEGLS_ColorSpace s);

	These functions provide string descriptions of the various enum
	types used in the JPEGLS_Info.

************************************************************************
** Typical I/O Callbacks:
**

uint freadCallBack(FILE *fp, ubyte *buf,uint len)
{
	// reads fp to buf
	return fread(buf,1,len,fp);
}

bool fwriteCallBack(FILE *fp,const ubyte *buf,uint len)
{
	// writes buf to fp
	return ( fwrite(buf,1,len,fp) == len ) ? true : false;
}

void putsMessageCallBack(FILE *fp,const char * String)
{
	fputs(String,fp);
}

uint memReadCallBack(ubyte **pMem, ubyte *buf,uint len)
{
	// copies from pMem to buf
	memcpy(buf,*pMem,len);
	*pMem += len;
return len;
}

bool memWriteCallBack(ubyte **pMem,const ubyte *buf,uint len)
{
	// copies from buf to *pMem
	memcpy(*pMem,buf,len);
	*pMem += len;
return true;
}

*************************************************************************
** Credits
**

JPEG-LS is based on the LOCO-I algorithm developed by Marcelo Weinberger,
Gadiel Seroussi, and Guillermo Sapiro of Hewlett-Packard Laboratories.
See http://www.hpl.hp.com/loco .

The JPEG-LS DLL was developed by Charles Bloom and Giovanni Motta at HP Labs, based on HP Labs' JPEG-LS implementation.
