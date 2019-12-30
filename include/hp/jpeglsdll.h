// Copyright (c) HEWLETT-PACKARD
// License details can be found in: HP_ LICENSE.txt

#ifndef JPEGLSLIB_H
#define JPEGLSLIB_H

extern "C"
{

//---------------------------------------------
// the (opaque) JPEGLS module

typedef struct JPEGLS JPEGLS;

//---------------------------------------------
// base types

typedef unsigned long	ulong;
typedef unsigned short	uword;
typedef unsigned char	ubyte;
typedef unsigned int 	uint;

typedef unsigned short	pixel;	// int is faster; short for less memory

typedef enum
{
	FALSE=0,
	TRUE=1
} BOOL;


typedef void	(*JPEGLS_MessageCallback)	(void * context,const char *message);

typedef uint	(*JPEGLS_ReadBufCallback)	(void * context,      ubyte * buffer,uint len);
typedef BOOL	(*JPEGLS_WriteBufCallback)	(void * context,const ubyte * buffer,uint len);

//---------------------------------------------
// the Info structures

#define MAX_COMPONENTS	(8)
#define MAX_SCANS		(4)

typedef enum
{
	INTERLEAVE_PLANE	=0,
	INTERLEAVE_NONE		= INTERLEAVE_PLANE,
	INTERLEAVE_LINE		=1,
	INTERLEAVE_PIXEL	=2,
	INTERLEAVE_COUNT	=3
} JPEGLS_Interleave;

typedef enum
{
	COLORSPACE_NONE = 0, // means don't transmit to decoder

	COLORSPACE_GRAY,
	COLORSPACE_PALETTIZED,
	COLORSPACE_RGB,
	COLORSPACE_YUV,
	COLORSPACE_HSV,
	COLORSPACE_HSB, COLORSPACE_HSL,
	COLORSPACE_LAB, COLORSPACE_CMYK,
	COLORSPACE_COUNT
} JPEGLS_ColorSpace;

typedef enum
{
	COLORXFORM_NONE = 0,  // means don't transmit to decoder

	COLORXFORM_HP1,
	COLORXFORM_HP2,
	COLORXFORM_HP3,
	COLORXFORM_RGB_AS_YUV_LOSSY,

	COLORXFORM_MATRIX,			// if you set this at encode time, you must
							// specify a colorMatrixForward & colorMatrixInverse
							// color inverse matrix must have diagonal ones!

	COLORXFORM_COUNT

} JPEGLS_ColorXForm;
	// the colorspaces are *not* in the JPEG-LS standard
	//	most of these are just information communication
	//	from the encoder to the decoder
	// the _AS_ values tell my code to do an xform at enc & dec

typedef struct JPEGLS_ScanInfo
{
	JPEGLS_Interleave interleave;

	uint loss,shift;
	uint thresholds[3];
	uint resetCount;

	JPEGLS_ColorSpace colorSpace;

	// optional colorXform stuff:
	JPEGLS_ColorXForm colorXForm;			// the matrices only exist if xform == MATRIX
	int colorMatrixForward[MAX_COMPONENTS][MAX_COMPONENTS];
											// the forward matrix is a real matrix, left-shifted 16
											// (encoder only)
	int colorMatrixInverse[MAX_COMPONENTS][MAX_COMPONENTS];
	BOOL colorMatrixDC[MAX_COMPONENTS];		// ACDC flag (eg. DC means you've changed the average to zero)
	uint colorMatrixBits[MAX_COMPONENTS];	// Value = Matrix[row][col] >> Bits
											// DC and Bits are per-row

	// stuff that's repeated from the main info :
	uint alphabet;
	uint components;
	uint componentId[MAX_COMPONENTS];

	// the JPEGLS spec allows for multiple tables, but we just allow one :

	BOOL hasTable;
	uint tableSize,tableEntryWidth;	// this stuff only exists if hasTable is true
    ulong mapTable[256];

} JPEGLS_ScanInfo;

typedef struct JPEGLS_Info
{
	uint width,height;
	uint alphabet,components;

	BOOL doRestart;
	uint restartInterval;

	uint samplingX[MAX_COMPONENTS],
		samplingY[MAX_COMPONENTS],
		componentId[MAX_COMPONENTS]; // we read & write these, but do nothing with them

	uint	scanCount;
	JPEGLS_ScanInfo scan[MAX_SCANS];

} JPEGLS_Info;

//----------------------------------------------------------
// the function import style

#define JPEGLSCC	_declspec(dllimport)

//---------------------------------------------
// the library functions

JPEGLSCC	JPEGLS * JPEGLS_Create(void);

JPEGLSCC	void JPEGLS_Destroy(JPEGLS * j);

JPEGLSCC	BOOL JPEGLS_CheckOk(JPEGLS * j);

JPEGLSCC	void JPEGLS_SetMessageCallback(JPEGLS * j,JPEGLS_MessageCallback mcb,void * messageContext);

JPEGLSCC	BOOL JPEGLS_GetInfo(const JPEGLS * j,JPEGLS_Info *jinfo);
						// can only GetInfo after StartDecode
JPEGLSCC	void JPEGLS_GetDefaultInfo(JPEGLS_Info *jinfo);

JPEGLSCC	BOOL JPEGLS_IsStreamJLS(JPEGLS_ReadBufCallback rcb,void * readContext);
						// test to see if a file is JLS file
						// note : we may read & not seek back! you must do any seek-backing you want

JPEGLSCC	BOOL JPEGLS_StartDecode(JPEGLS * j, JPEGLS_ReadBufCallback rcb,void * readContext );
JPEGLSCC	BOOL JPEGLS_StartEncode(JPEGLS * j,JPEGLS_WriteBufCallback wcb,void * writeContext,const JPEGLS_Info *jinfo);

JPEGLSCC	void * JPEGLS_GetEncodeLine(JPEGLS * j);
					// gives you a line to work into and then pass to EncodeLine()
					// you must re-call this after each EncodeLine

JPEGLSCC	BOOL JPEGLS_EncodeLine8b (JPEGLS * j,ubyte * line);
JPEGLSCC	BOOL JPEGLS_EncodeLine16b(JPEGLS * j,uword * line);

JPEGLSCC	BOOL JPEGLS_DecodeLine8b (JPEGLS * j,ubyte ** pLine);
JPEGLSCC	BOOL JPEGLS_DecodeLine16b(JPEGLS * j,uword ** pLine);
						// the decode functions give you a pointer to a line which is
						//	valid only until any other JPEGLS function is called!

/****
 notes on the CodeLine functions:
	1. if interleave != PLANE then the line data must be pixel-interleaved : RGBRGB..
	2. you must call codeLine for all of the scan lines to make it terminate the scan
		before you can restart with a new StartCode() call
	3. the 16b data are data-valid uwords!  That is, they are not endian-standardized!
		if you want to write them out directly, you must manage any necessary endian
		corrections! (we treat them in the endianness of your machine)
	4. for encode : using the line from GetEncodeLine is slightly faster, but beware:
		A. if you ever use it, you must always use it
		B. you must get a new line for every scan line! the pointer is *not* persistent!
****/

JPEGLSCC	BOOL JPEGLS_EncodeFromCB (JPEGLS * j,JPEGLS_ReadBufCallback  rcb,void *rcbContext);
JPEGLSCC	BOOL JPEGLS_DecodeToCB   (JPEGLS * j,JPEGLS_WriteBufCallback wcb,void *wcbContext);
						// from a DLL, you should use these instead of CodeLine, for speed
						// does 8b or 16b as appropriate

JPEGLSCC
	const char * JPEGLS_GetInterleaveDescription(JPEGLS_Interleave interleave);
JPEGLSCC
	const char * JPEGLS_GetColorXFormDescription(JPEGLS_ColorXForm xf);
JPEGLSCC
	const char * JPEGLS_GetColorSpaceDescription(JPEGLS_ColorSpace s);

//---------------------------------------------

}
#endif // JPEGLS_H

