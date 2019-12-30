// Copyright (c) HEWLETT-PACKARD
// License details can be found in: HP_ LICENSE.txt

#pragma once

#include <cstdint>
#include <cstddef>

namespace hp {

//---------------------------------------------
// the (opaque) JPEGLS module

struct JPEGLS;

//---------------------------------------------
// base types

enum class BOOL
{
    FALSE = 0,
    TRUE = 1
};


typedef void (*JPEGLS_MessageCallback)(void* context, const char* message);

typedef uint32_t (*JPEGLS_ReadBufCallback)(void* context, std::byte* buffer, uint32_t len);
typedef BOOL (*JPEGLS_WriteBufCallback)(void* context, const std::byte* buffer, uint32_t len);

//---------------------------------------------
// the Info structures

constexpr auto MAX_COMPONENTS = 8;
constexpr auto MAX_SCANS = 4;

enum class JPEGLS_Interleave
{
    plane = 0,
    none = plane,
    line = 1,
    pixel = 2,
};

enum class JPEGLS_ColorSpace
{
    NONE = 0, // means don't transmit to decoder

    GRAY,
    PALETTIZED,
    RGB,
    YUV,
    HSV,
    HSB,
    HSL,
    LAB,
    CMYK,
};

enum class JPEGLS_ColorXForm
{
    NONE = 0, // means don't transmit to decoder

    HP1,
    HP2,
    HP3,
    RGB_AS_YUV_LOSSY,

    MATRIX, // if you set this at encode time, you must
            // specify a colorMatrixForward & colorMatrixInverse
            // color inverse matrix must have diagonal ones!
};

// the colorspaces are *not* in the JPEG-LS standard
// most of these are just information communication
// from the encoder to the decoder
// the _AS_ values tell my code to do an xform at enc & dec

struct JPEGLS_ScanInfo final
{
    JPEGLS_Interleave interleave;

    uint32_t loss;
    uint32_t shift;
    uint32_t thresholds[3];
    uint32_t resetCount;

    JPEGLS_ColorSpace colorSpace;

    // optional colorXform stuff:
    JPEGLS_ColorXForm colorXForm; // the matrices only exist if xform == MATRIX
    int colorMatrixForward[MAX_COMPONENTS][MAX_COMPONENTS];

    // the forward matrix is a real matrix, left-shifted 16
    // (encoder only)
    int colorMatrixInverse[MAX_COMPONENTS][MAX_COMPONENTS];
    BOOL colorMatrixDC[MAX_COMPONENTS];   // ACDC flag (eg. DC means you've changed the average to zero)
    uint32_t colorMatrixBits[MAX_COMPONENTS]; // Value = Matrix[row][col] >> Bits
                                          // DC and Bits are per-row

    // stuff that's repeated from the main info :
    uint32_t alphabet;
    uint32_t components;
    uint32_t componentId[MAX_COMPONENTS];

    // the JPEGLS spec allows for multiple tables, but we just allow one :

    BOOL hasTable;
    uint32_t tableSize, tableEntryWidth; // this stuff only exists if hasTable is true
    uint32_t mapTable[256];
};

struct JPEGLS_Info final
{
    uint32_t width;
    uint32_t height;
    uint32_t alphabet;
    uint32_t components;

    BOOL doRestart;
    uint32_t restartInterval;

    uint32_t samplingX[MAX_COMPONENTS];
    uint32_t samplingY[MAX_COMPONENTS];
    uint32_t componentId[MAX_COMPONENTS]; // we read & write these, but do nothing with them

    uint32_t scanCount;
    JPEGLS_ScanInfo scan[MAX_SCANS];
};


//---------------------------------------------
// the library functions

extern "C" {

__declspec(dllimport) JPEGLS* JPEGLS_Create(void);

__declspec(dllimport) void JPEGLS_Destroy(JPEGLS* j);

__declspec(dllimport) BOOL JPEGLS_CheckOk(JPEGLS* j);

__declspec(dllimport) void JPEGLS_SetMessageCallback(JPEGLS* j, JPEGLS_MessageCallback mcb, void* messageContext);

__declspec(dllimport) BOOL JPEGLS_GetInfo(const JPEGLS* j, JPEGLS_Info* jinfo);
// can only GetInfo after StartDecode

__declspec(dllimport) void JPEGLS_GetDefaultInfo(JPEGLS_Info* jinfo);

__declspec(dllimport) BOOL JPEGLS_IsStreamJLS(JPEGLS_ReadBufCallback rcb, void* readContext);
// test to see if a file is JLS file
// note : we may read & not seek back! you must do any seek-backing you want

__declspec(dllimport) BOOL JPEGLS_StartDecode(JPEGLS* j, JPEGLS_ReadBufCallback rcb, void* readContext);
__declspec(dllimport) BOOL JPEGLS_StartEncode(JPEGLS* j, JPEGLS_WriteBufCallback wcb, void* writeContext, const JPEGLS_Info* jinfo);

__declspec(dllimport) void* JPEGLS_GetEncodeLine(JPEGLS* j);
// gives you a line to work into and then pass to EncodeLine()
// you must re-call this after each EncodeLine

__declspec(dllimport) BOOL JPEGLS_EncodeLine8b(JPEGLS* j, std::byte* line);
__declspec(dllimport) BOOL JPEGLS_EncodeLine16b(JPEGLS* j, uint16_t* line);

__declspec(dllimport) BOOL JPEGLS_DecodeLine8b(JPEGLS* j, std::byte** pLine);
__declspec(dllimport) BOOL JPEGLS_DecodeLine16b(JPEGLS* j, uint16_t** pLine);
// the decode functions give you a pointer to a line which is
// valid only until any other JPEGLS function is called!

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

__declspec(dllimport) BOOL JPEGLS_EncodeFromCB(JPEGLS* j, JPEGLS_ReadBufCallback rcb, void* rcbContext);
__declspec(dllimport) BOOL JPEGLS_DecodeToCB(JPEGLS* j, JPEGLS_WriteBufCallback wcb, void* wcbContext);
// from a DLL, you should use these instead of CodeLine, for speed
// does 8b or 16b as appropriate

__declspec(dllimport) const char* JPEGLS_GetInterleaveDescription(JPEGLS_Interleave interleave);
__declspec(dllimport) const char* JPEGLS_GetColorXFormDescription(JPEGLS_ColorXForm xf);
__declspec(dllimport) const char* JPEGLS_GetColorSpaceDescription(JPEGLS_ColorSpace s);
}
} // namespace hp
