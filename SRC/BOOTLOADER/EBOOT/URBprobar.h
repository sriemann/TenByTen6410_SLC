/**
 * A mini gui implementation (copied some code from VGUI)
 * functions:
 * 1. draw line (horz or vert only)
 * 2. draw rectangle, fill rectangle,
 * 3. draw bitmap ( Windows bmp format)
 * 4. draw text ( note, ascii only and the text font was gererated by
 * vgui's FontGen 1.0 tool)
 *
 * Usage:
 *  first maybe you want to change this line to fit your rgb format
	#define mv_RGB_FORMAT mv_RGB_FORMAT_565
 *  in the source code firstly you need to prepare all HW related jobs,
 *  such as turn on panel backlight/init LCDC
 *  after that the first function you need to invoked is mv_initPrimary.
 *  You need to set the correct member in mv_surface argument.
 *  After that you can call all functions
 *
 *  Here is a example:
 *
 *
	mv_surface s;
        s.width = 800;
        s.height = 480;
	s.lineBytes = 800 * 2;
		// if the panel is 16bit
	s.startAddr = 0x7C000000;
	// the frame buffer address

	mv_initPrimary(&s);

        mv_Rect r;
	r.left = 0;
	r.right = s.width;
	r.top = 0;
	r.bottom = s.height;
	//fill whole screen with red color
	mv_fillRect(&r, mv_RGB2Color(255, 0, 0));
		
	//draw some text on the screen (build-in font size is 12x24)
	mv_textOut(0, 0, "Hello,world!", mv_RGB2Color(0, 0, 0));
	mv_textOut(0, 25, "Hello, mini VGUI!", mv_RGB2Color(0, 0, 0));
 *
 */
#ifndef MINIVGUI_H_INCLUDED
#define MINIVGUI_H_INCLUDED

#ifdef  __cplusplus
extern "C" {
#endif

#define mv_RGB_FORMAT_555  555
#define mv_RGB_FORMAT_565  565
#define mv_RGB_FORMAT_888  888
#define mv_RGB_FORMAT_666  666

//change this line if you are using other rgb format
#define mv_RGB_FORMAT mv_RGB_FORMAT_565

#if ( mv_RGB_FORMAT == mv_RGB_FORMAT_565)
	typedef unsigned short	mv_color;
	#define mv_RGB2Color(r, g, b)  (mv_color)(((r) >> 3) << 11 | ((g) >> 2) << 5 | ((b) >> 3))

#elif ( mv_RGB_FORMAT == mv_RGB_FORMAT_555)
	typedef unsigned short	mv_color;
	#define mv_RGB2Color(r, g, b)  (mv_color)(((r) >> 3) << 10 | ((g) >> 3) << 5 | ((b) >> 3) & 0x7fff)

#elif ( mv_RGB_FORMAT == mv_RGB_FORMAT_888)
	typedef unsigned int mv_color;
	#define mv_RGB2Color(r, g, b)	(mv_color)(((r) << 16 | (g) << 8 | (b)) & 0x00FFFFFF)

#elif ( mv_RGB_FORMAT == mv_RGB_FORMAT_666)
	typedef unsigned int mv_color;
	#define mv_RGB2Color(r, g, b)  (mv_color)(((r) >> 2) << 12 | ((g) >> 2) << 6 | ((b) >> 2))

#else
	#error "unknown rgb format."
#endif


typedef struct
{
    int     width;
    int     height;
    int     lineBytes;
	char *	startAddr;
}mv_surface;

typedef struct
{
    int	     left;
    int      top;
    int      right;
    int      bottom;
}mv_Rect;

/**
 * init minivgui primary screen. this is the first function need be called
 */
void mv_initPrimary(const mv_surface * s);

// bmpData is the start address of a Win32 bmp file(support 32/24/16/8bit format)
// usually it is loaded from nand flash
// img->startAddr must be a valid memory address and can hold the image data
// return 0 if ok.
int mv_loadBmp(const void* bmpData, mv_surface* img);

void mv_putPixel(int x, int y, mv_color color);
void mv_drawLine(int x1, int y1, int x2, int y2, mv_color color);
void mv_drawRect(const mv_Rect* rect, mv_color color);
void mv_fillRect(const mv_Rect* rect, mv_color color);

/**
 *
 * @param x start x
 * @param y start y
 * @param bmp the bitmap object
 * @param bmpRect the draw part rect(if NULL draw whole bmp)
 * @param transColor (set the transparent color, if NULL draw bmp directly)
 */
void mv_drawBitmap(int x, int y, const mv_surface* bmp, const mv_Rect * bmpRect,
                        const mv_color * transColor);

/**
 * draw text on the screen (Note: only acsii supported)
 */
void mv_textOut(int x, int y, const char * string, mv_color textColor);



#ifdef  __cplusplus
}
#endif

#endif // MINIVGUI_H_INCLUDED


