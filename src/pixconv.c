/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -  This software is distributed in the hope that it will be
 -  useful, but with NO WARRANTY OF ANY KIND.
 -  No author or distributor accepts responsibility to anyone for the
 -  consequences of using this software, or for whether it serves any
 -  particular purpose or works at all, unless he or she says so in
 -  writing.  Everyone is granted permission to copy, modify and
 -  redistribute this source code, for commercial or non-commercial
 -  purposes, with the following restrictions: (1) the origin of this
 -  source code must not be misrepresented; (2) modified versions must
 -  be plainly marked as such; and (3) this notice may not be removed
 -  or altered from any source or modified source distribution.
 *====================================================================*/

/*
 *  pixconv.c
 *
 *      These functions convert between images of different types
 *      without scaling.
 *
 *      Conversion from 8 bpp grayscale to 1, 2, 4 and 8 bpp
 *           PIX        *pixThreshold8()
 *
 *      Conversion from colormap to full color or grayscale
 *           PIX        *pixRemoveColormap()
 *
 *      Conversion from RGB color to grayscale
 *           PIX        *pixConvertRGBToGray()
 *           PIX        *pixConvertRGBToGrayFast()
 *           PIX        *pixConvertRGBToLuminance()
 *
 *      Conversion from grayscale to colormap
 *           PIX        *pixConvertGrayToColormap()  -- 2, 4, 8 bpp
 *           PIX        *pixConvertGrayToColormap8()  -- 8 bpp only
 *
 *      Conversion from RGB color to colormap (exact)
 *           PIX        *pixConvertRGBToColormap()
 *
 *      Conversion from 16 bpp to 8 bpp
 *           PIX        *pixConvert16To8()
 *
 *      Conversion from grayscale to false color
 *           PIX        *pixConvertGrayToFalseColor()
 *
 *      Unpacking conversion from 1 bpp to 2 bpp
 *           PIX        *pixConvert1To2Cmap()
 *           PIX        *pixConvert1To2()
 *
 *      Unpacking conversion from 1 bpp to 4 bpp
 *           PIX        *pixConvert1To4Cmap()
 *           PIX        *pixConvert1To4()
 *
 *      Unpacking conversion from 1 bpp to 8, 16 and 32 bpp
 *           PIX        *pixUnpackBinary()
 *           PIX        *pixConvert1To16()
 *           PIX        *pixConvert1To32()
 *
 *      Unpacking conversion from 1, 2 and 4 bpp to 8 bpp
 *           PIX        *pixConvertTo8()
 *           PIX        *pixConvert1To8()
 *           PIX        *pixConvert2To8()
 *           PIX        *pixConvert4To8()
 *
 *      Unpacking conversion to 32 bpp (RGB)
 *           PIX        *pixConvertTo32()
 *           PIX        *pixConvert8To32()
 *
 *      Conversion for printing in PostScript
 *           PIX        *pixConvertForPSWrap()
 *
 *      Colorspace conversion between RGB and HSV
 *           PIX        *pixConvertRGBToHSV()
 *           PIX        *pixConvertHSVToRGB()
 *           l_int32     convertRGBToHSV()
 *           l_int32     convertHSVToRGB()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "allheaders.h"

    /* these numbers are ad-hoc, but at least they add up
       to 1. Unlike, for example, the weighting factor for
       conversion of RGB to luminance, or more specifically
       to Y in the YUV colorspace. Those numbers come
       from the International Telecommunications Union, via ITU-R
       (and formerly ITU CCIR 601). */
static const l_float32  L_RED_WEIGHT =   0.3;
static const l_float32  L_GREEN_WEIGHT = 0.5;
static const l_float32  L_BLUE_WEIGHT =  0.2;

    /* fallback requested colors for pixConvertRGBToColormap() */
static const l_int32  SAFE_VALUE_FOR_REQUESTED_COLORS = 220;


#ifndef  NO_CONSOLE_IO
#define DEBUG_CONVERT_TO_COLORMAP  0
#endif   /* ~NO_CONSOLE_IO */


/*-------------------------------------------------------------*
 *     Conversion from 8 bpp grayscale to 1, 2 4 and 8 bpp     *
 *-------------------------------------------------------------*/
/*!
 *  pixThreshold8()
 *
 *      Input:  pix (8 bpp grayscale)
 *              d (destination depth: 1, 2, 4 or 8)
 *              nlevels (number of levels to be used for colormap)
 *              cmapflag (1 if makes colormap; 0 otherwise)
 *      Return: pixd (thresholded with standard dest thresholds),
 *              or null on error
 *
 *  Notes:
 *      (1) This uses, by default, equally spaced "target" values
 *          that depend on the number of levels, with thresholds
 *          halfway between.  For N levels, with separation (N-1)/255,
 *          there are N-1 fixed thresholds.
 *      (2) For 1 bpp destination, the number of levels can only be 2
 *          and if a cmap is made, black is (0,0,0) and white
 *          is (255,255,255), which is opposite to the convention
 *          without a colormap.
 *      (3) For 1, 2 and 4 bpp, the nlevels arg is used if a colormap
 *          is made; otherwise, we take the most significant bits
 *          from the src that will fit in the dest.
 *      (4) For 8 bpp, the input pixs is quantized to nlevels.  The
 *          dest quantized with that mapping, either through a colormap
 *          table or directly with 8 bit values.
 *      (5) Typically you should not use make a colormap for 1 bpp dest.
 *      (6) This is not dithering.  Each pixel is treated independently.
 */
PIX *
pixThreshold8(PIX     *pixs,
              l_int32  d,
	      l_int32  nlevels,
	      l_int32  cmapflag)
{
PIX       *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixThreshold8");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 8)
	return (PIX *)ERROR_PTR("pixs not 8 bpp", procName, NULL);
    if (cmapflag && nlevels < 2)
	return (PIX *)ERROR_PTR("nlevels must be at least 2", procName, NULL);

    switch (d) {
    case 1:
	pixd = pixThresholdToBinary(pixs, 128);
	if (cmapflag) {
	    cmap = pixcmapCreate(d);
	    pixcmapAddColor(cmap, 0, 0, 0);
	    pixcmapAddColor(cmap, 255, 255, 255);
	    pixSetColormap(pixd, cmap);
	}
        break;
    case 2:
        pixd = pixThresholdTo2bpp(pixs, nlevels, cmapflag);
        break;
    case 4:
        pixd = pixThresholdTo4bpp(pixs, nlevels, cmapflag);
        break;
    case 8:
        pixd = pixThresholdOn8bpp(pixs, nlevels, cmapflag);
        break;
    default:
	return (PIX *)ERROR_PTR("d must be in {1,2,4,8}", procName, NULL);
    }

    if (!pixd)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    return pixd;
}


/*-------------------------------------------------------------*
 *               Conversion from colormapped pix               *
 *-------------------------------------------------------------*/
/*!
 *  pixRemoveColormap()
 *
 *      Input:  pixs (any)
 *              type (REMOVE_CMAP_TO_BINARY,
 *                    REMOVE_CMAP_TO_GRAYSCALE,
 *                    REMOVE_CMAP_TO_FULL_COLOR,
 *                    REMOVE_CMAP_BASED_ON_SRC)
 *      Return: new pix, or null on error
 *
 *  Notes:
 *      (1) If there is no colormap, a clone is returned.
 *      (2) Otherwise, the input pixs is restricted to 1, 2, 4 or 8 bpp.
 *      (3) Use REMOVE_CMAP_TO_BINARY only on 1 bpp pix.
 *      (4) For grayscale conversion, use a weighted average
 *          of RGB values, and always return an 8 bpp pix, regardless
 *          of whether the input pixs depth is 2, 4 or 8 bpp.
 */
PIX *
pixRemoveColormap(PIX     *pixs,
                  l_int32  type)
{
l_int32    sval, rval, gval, bval;
l_int32    i, j, w, h, d, wpls, wpld;
l_int32    colorfound;
l_int32   *rmap, *gmap, *bmap;
l_uint32  *datas, *lines, *datad, *lined;
PIXCMAP   *cmap;
PIX       *pixd;

    PROCNAME("pixRemoveColormap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if ((cmap = pixGetColormap(pixs)) == NULL)
	return pixClone(pixs);

    if (type != REMOVE_CMAP_TO_BINARY &&
        type != REMOVE_CMAP_TO_GRAYSCALE &&
        type != REMOVE_CMAP_TO_FULL_COLOR &&
        type != REMOVE_CMAP_BASED_ON_SRC) {
        L_WARNING("Invalid type; converting based on src", procName);
        type = REMOVE_CMAP_BASED_ON_SRC;
    }

    d = pixGetDepth(pixs);
    if (d != 1 && d != 2 && d != 4 && d != 8)
	return (PIX *)ERROR_PTR("pixs must be {1,2,4,8} bpp", procName, NULL);

    if (pixcmapToArrays(cmap, &rmap, &gmap, &bmap))
	return (PIX *)ERROR_PTR("colormap arrays not made", procName, NULL);

    if (d != 1 && type == REMOVE_CMAP_TO_BINARY) {
        L_WARNING("not 1 bpp; can't remove cmap to binary", procName);
        type = REMOVE_CMAP_BASED_ON_SRC;
    }

    if (type == REMOVE_CMAP_BASED_ON_SRC) {
	    /* select output type depending on colormap */
        pixcmapHasColor(cmap, &colorfound);
        if (!colorfound) {
            if (d == 1)
                type = REMOVE_CMAP_TO_BINARY;
            else
                type = REMOVE_CMAP_TO_GRAYSCALE;
        }
        else
            type = REMOVE_CMAP_TO_FULL_COLOR;
    }

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (type == REMOVE_CMAP_TO_BINARY) {
	if ((pixd = pixCopy(NULL, pixs)) == NULL)
	    return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
        pixcmapGetColor(cmap, 0, &rval, &gval, &bval);
        if (rval == 0)  /* photometrically inverted from standard */
            pixInvert(pixd, pixd);
        pixDestroyColormap(pixd);
    }
    else if (type == REMOVE_CMAP_TO_GRAYSCALE) {
	if ((pixd = pixCreate(w, h, 8)) == NULL)
	    return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
        pixCopyResolution(pixd, pixs);
	datad = pixGetData(pixd);
	wpld = pixGetWpl(pixd);
	for (i = 0; i < h; i++) {
	    lines = datas + i * wpls;
	    lined = datad + i * wpld;
	    for (j = 0; j < w; j++) {
		switch (d)   /* depth test above; no default permitted */
		{
		case 8:
		    sval = GET_DATA_BYTE(lines, j);
		    break;
		case 4:
		    sval = GET_DATA_QBIT(lines, j);
		    break;
		case 2:
		    sval = GET_DATA_DIBIT(lines, j);
		    break;
                case 1:
                    sval = GET_DATA_BIT(lines, j);
                    break;
                default:
		    return NULL;
		}
		gval = (rmap[sval] + 2 * gmap[sval] + bmap[sval]) / 4;
		SET_DATA_BYTE(lined, j, gval);
	    }
	}
    }
    else {  /* type == REMOVE_CMAP_TO_FULL_COLOR */
	if ((pixd = pixCreate(w, h, 32)) == NULL)
	    return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
        pixCopyResolution(pixd, pixs);
	datad = pixGetData(pixd);
	wpld = pixGetWpl(pixd);
	for (i = 0; i < h; i++) {
	    lines = datas + i * wpls;
	    lined = datad + i * wpld;
	    for (j = 0; j < w; j++) {
		if (d == 8)
		    sval = GET_DATA_BYTE(lines, j);
		else if (d == 4)
		    sval = GET_DATA_QBIT(lines, j);
		else if (d == 2)
		    sval = GET_DATA_DIBIT(lines, j);
                else if (d == 1)
                    sval = GET_DATA_BIT(lines, j);
                else
                    return NULL;

		SET_DATA_BYTE(lined + j, COLOR_RED, rmap[sval]);
		SET_DATA_BYTE(lined + j, COLOR_GREEN, gmap[sval]);
		SET_DATA_BYTE(lined + j, COLOR_BLUE, bmap[sval]);
	    }
	}
    }

    FREE((void *)rmap);
    FREE((void *)gmap);
    FREE((void *)bmap);
    return pixd;
}



/*-------------------------------------------------------------*
 *            Conversion from RGB color to grayscale           *
 *-------------------------------------------------------------*/
/*!
 *  pixConvertRGBToLuminance()
 *
 *      Input:  pix (32 bpp RGB)
 *      Return: 8 bpp pix, or null on error
 *
 *  Note: we use a standard luminance conversion
 */
PIX *
pixConvertRGBToLuminance(PIX *pixs)
{
  return pixConvertRGBToGray(pixs, 0.0, 0.0, 0.0);
}


/*!
 *  pixConvertRGBToGray()
 *
 *      Input:  pix (32 bpp RGB)
 *              rwt, gwt, bwt  (these should add to 1.0,
 *                              or use 0.0 for default)
 *      Return: 8 bpp pix, or null on error
 *
 *  Note: we use a weighted average of the RGB values
 */
PIX *
pixConvertRGBToGray(PIX       *pixs,
		    l_float32  rwt,
		    l_float32  gwt,
		    l_float32  bwt)
{
l_uint8    redbyte, greenbyte, bluebyte, graybyte;
l_int32    i, j, w, h, wpls, wpld;
l_uint32  *datas, *words, *datad, *lined;
PIX       *pixd;

    PROCNAME("pixConvertRGBToGray");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 32)
	return (PIX *)ERROR_PTR("pixs not 32 bpp", procName, NULL);

    if (rwt == 0.0 && gwt == 0.0 && bwt == 0.0) {
        rwt = L_RED_WEIGHT;
        gwt = L_GREEN_WEIGHT;
        bwt = L_BLUE_WEIGHT;
    }

        /* If the sum of weights is much above 1.0, you can get
	 * overflow in the gray value. */
    if (L_ABS(rwt + gwt + bwt - 1.0) > 0.0001)
	return (PIX *)ERROR_PTR("weights don't add to 1.0", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    if ((pixd = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < h; i++) {
	words = datas + i * wpls;
	lined = datad + i * wpld;
	for (j = 0; j < w; j++, words++) {
	    redbyte = GET_DATA_BYTE(words, COLOR_RED);
	    greenbyte = GET_DATA_BYTE(words, COLOR_GREEN);
	    bluebyte = GET_DATA_BYTE(words, COLOR_BLUE);
	    graybyte = (l_uint8)(rwt * redbyte +
		               gwt * greenbyte +
			       bwt * bluebyte + 0.5);
	    SET_DATA_BYTE(lined, j, graybyte);
	}
    }

    return pixd;
}


/*!
 *  pixConvertRGBToGrayFast()
 *
 *      Input:  pix (32 bpp RGB)
 *      Return: 8 bpp pix, or null on error
 *
 *  Notes:
 *      (1) This function should be used if speed of conversion
 *          is paramount, and the green channel can be used as
 *          a fair representative of the RGB intensity.  It is
 *          about 8x faster than pixConvertRGBToGray().
 *      (2) The standard color byte order (RGBA) is assumed.
 *      (3) If you want to combine RGB to gray conversion with
 *          subsampling, use pixScaleRGBToGrayFast() instead.
 *
 *  *** Warning: implicit assumption about RGB component ordering ***
 */
PIX *
pixConvertRGBToGrayFast(PIX  *pixs)
{
l_uint8    graybyte;
l_int32    i, j, w, h, wpls, wpld;
l_uint32  *datas, *words, *datad, *lined;
PIX       *pixd;

    PROCNAME("pixConvertRGBToGrayFast");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 32)
	return (PIX *)ERROR_PTR("pixs not 32 bpp", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    if ((pixd = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    
    for (i = 0; i < h; i++) {
	words = datas + i * wpls;
	lined = datad + i * wpld;
	for (j = 0; j < w; j++, words++) {
            graybyte = ((*words) >> 16) & 0xff;
	    SET_DATA_BYTE(lined, j, graybyte);
	}
    }

    return pixd;
}


/*---------------------------------------------------------------------------*
 *                  Conversion from grayscale to colormap                    *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertGrayToColormap()
 *
 *      Input:  pixs (2, 4 or 8 bpp grayscale)
 *      Return: pixd (2, 4 or 8 bpp with colormap), or null on error
 *
 *  Notes:
 *      (1) Returns a copy if pixs already has a colormap.
 *      (2) For 8 bpp src, this is a lossless transformation.
 *      (3) For 2 and 4 bpp src, this generates a colormap that
 *          assumes full coverage of the gray space: 4 levels for
 *          d = 2 and 16 levels for d = 4.  The standard target
 *          levels are used.
 */
PIX *
pixConvertGrayToColormap(PIX  *pixs)
{
l_int32    w, h, d, i, j, wpls, wplt;
l_int32    val2, val4, val8;
l_uint32  *lines, *linet, *datas, *datat;
PIX       *pixt, *pixd;

    PROCNAME("pixConvertGrayToColormap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
	return (PIX *)ERROR_PTR("pixs not 2, 4 or 8 bpp", procName, NULL);

    if (pixGetColormap(pixs)) {
        L_WARNING("pixs already has a colormap", procName);
        return pixCopy(NULL, pixs);
    }
    if (d == 8)  /* lossless conversion */
        return pixConvertGrayToColormap8(pixs, 2);

        /* Expand to an 8 bpp grayscale image, using equally spaced
	 * target values that cover the full 8 bpp range. */
    w = pixGetWidth(pixs); 
    h = pixGetHeight(pixs); 
    if ((pixt = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixt not made", procName, NULL);
    datas = pixGetData(pixs);
    datat = pixGetData(pixt);
    wpls = pixGetWpl(pixs);
    wplt = pixGetWpl(pixt);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        linet = datat + i * wplt;
	for (j = 0; j < w; j++) {
	    switch (d)
	    {
	    case 2:
		val2 = GET_DATA_DIBIT(lines, j);
	        val8 = 255 * val2 / 3;
		break;
	    case 4:
		val4 = GET_DATA_QBIT(lines, j);
	        val8 = 255 * val4 / 15;
		break;
            default:
	        return (PIX *)ERROR_PTR("invalid depth", procName, NULL);
	    }
	    SET_DATA_BYTE(linet, j, val8);
	}
    }

        /* Lossless conversion to the original depth, with added colormap */
    pixd = pixConvertGrayToColormap8(pixt, d);

    pixDestroy(&pixt);
    return pixd;
}
        

/*!
 *  pixConvertGrayToColormap8()
 *
 *      Input:  pixs (8 bpp grayscale)
 *              mindepth (of pixd; valid values are 2, 4 and 8)
 *      Return: pixd (2, 4 or 8 bpp with colormap), or null on error
 *
 *  Notes:
 *      (1) Returns a copy if pixs already has a colormap.
 *      (2) This is a lossless transformation.  We compute the
 *          number of different gray values in pixs, and construct
 *          a colormap that has exactly these values.
 *      (3) 'mindepth' is the minimum depth of pixd.  If mindepth == 8,
 *          pixd will always be 8 bpp.  Let the number of different
 *          gray values in pixs be ngray.  If mindepth == 4, we attempt
 *          to save pixd as a 4 bpp image, but if ngray > 16,
 *          pixd must be 8 bpp.  Likewise, if mindepth == 2,
 *          the depth of pixd will be 2 if ngray <= 4 and 4 if ngray > 4
 *          but <= 16.
 */
PIX *
pixConvertGrayToColormap8(PIX     *pixs,
                          l_int32  mindepth)
{
l_int32    ncolors, w, h, depth, i, j, wpls, wpld;
l_int32    index, num, val, newval;
l_int32    array[256];
l_uint32  *lines, *lined, *datas, *datad;
NUMA      *na;
PIX       *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixConvertGrayToColormap8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 8)	
	return (PIX *)ERROR_PTR("pixs not 8 bpp", procName, NULL);
    if (mindepth != 2 && mindepth != 4 && mindepth != 8) {
        L_WARNING("invalid value of mindepth; setting to 8", procName);
        mindepth = 8;
    }

    if (pixGetColormap(pixs)) {
        L_WARNING("pixs already has a colormap", procName);
        return pixCopy(NULL, pixs);
    }

    na = pixGrayHistogram(pixs);
    ncolors = numaGetCount(na);
    if (mindepth == 8 || ncolors > 16)
        depth = 8;
    else if (mindepth == 4 || ncolors > 4)
        depth = 4;
    else
        depth = 2;

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    pixd = pixCreate(w, h, depth);
    cmap = pixcmapCreate(depth);
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);

    index = 0;
    for (i = 0; i < 256; i++) {
        numaGetIValue(na, i, &num);
        if (num > 0) {
            pixcmapAddColor(cmap, i, i, i);
            array[i] = index;
            index++;
        }
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lines, j);
            newval = array[val];
            SET_DATA_BYTE(lined, j, newval);
        }
    }

    numaDestroy(&na);
    return pixd;
}



/*---------------------------------------------------------------------------*
 *               Conversion from RGB color to colormap (exact)
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertRGBToColormap()
 *
 *      Input:  pixs (32 bpp rgb)
 *              level (of octcube indexing, for histogram: 1, 2, 3, 4, 5, 6)
 *             &nerrors (number of improperly categorized pixels)
 *      Return: pixd (2, 4 or 8 bpp with colormap), or null on error
 *
 *  Notes:
 *      (1) This is appropriate for a color image, such as one made
 *          orthographically, that has a small number of colors. 
 *      (2) If there are more than 256 colors, we fall back to using
 *          pixOctreeColorQuant() with dithering, to get the best 
 *          result possible.
 *      (3) Calling with level = 4 or above should not get more than
 *          one color in each cube.  
 *      (4) The number of pixels whose color was not exactly 
 *          reproduced because more than 1 pixel of a given color
 *          was in the same octcube is returned in &nerrors.
 *          On error, the returned value is UNDEF.
 *      (5) These images are conveniently compressed losslessly with png.
 */     
PIX *
pixConvertRGBToColormap(PIX      *pixs,
                        l_int32   level,
	                l_int32  *pnerrors)
{
l_int32    w, h, wpls, wpld, i, j, nerrors;
l_int32    ncubes, ncolors, depth, cindex, val, oval;
l_int32    rval, gval, bval;
l_int32   *octarray;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *lines, *lined, *datas, *datad, *ppixel;
l_uint32  *colorarray;
NUMA      *na;
PIX       *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixConvertRGBToColormap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 32)	
	return (PIX *)ERROR_PTR("pixs not 32 bpp", procName, NULL);
    if (!pnerrors)
	return (PIX *)ERROR_PTR("&nerrors not defined", procName, NULL);
    if (level < 1 || level > 6)
	return (PIX *)ERROR_PTR("level not in {1 ... 6}", procName, NULL);
    *pnerrors = UNDEF;  /* warning or error condition if not reset */


        /* Get the histogram and count the number of occupied cubes.
	 * We don't yet know if this is the number of actual colors,
	 * but if it's not, we will make some approximation to pixel
	 * colors, because we only allow one color for all pixels in
	 * the same octcube. */
    na = pixOctcubeHistogram(pixs, level);
    ncubes = numaGetCount(na);
    ncolors = 0;
    for (i = 0; i < ncubes; i++) {
        numaGetIValue(na, i, &val);
	if (val > 0)
	    ncolors++;
    }

        /* If there are too many colors, fall back to pixOctreeColorQuant()
	 * using dithering.  This is the best we can do. */
    if (ncolors > 256) {
	L_WARNING("too many colors; using pixOctreeColorQuant()", procName);
	numaDestroy(&na);
	return pixOctreeColorQuant(pixs, SAFE_VALUE_FOR_REQUESTED_COLORS, 1);
    }

#if  DEBUG_CONVERT_TO_COLORMAP
    fprintf(stderr, "ncubes = %d, ncolors = %d\n", ncubes, ncolors);
#endif  /* DEBUG_CONVERT_TO_COLORMAP */

    if (makeRGBToIndexTables(&rtab, &gtab, &btab, level))
	return (PIX *)ERROR_PTR("tables not made", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    if (ncolors <= 4)
        depth = 2;
    else if (ncolors <= 16)
        depth = 4;
    else  /* ncolors <= 256 */
        depth = 8;
	
    if ((pixd = pixCreate(w, h, depth)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* octarray will give a ptr from the octcube to the colorarray */
    if ((octarray = (l_int32 *)CALLOC(ncubes, sizeof(l_int32))) == NULL)
	return (PIX *)ERROR_PTR("octarray not made", procName, NULL);
    if ((colorarray = (l_uint32 *)CALLOC(ncolors + 1, sizeof(l_uint32)))
            == NULL)
	return (PIX *)ERROR_PTR("colorarray not made", procName, NULL);

    cindex = 1;  /* start with 1 */ 
    nerrors = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < w; j++) {
	    ppixel = lines + j;
            rval = GET_DATA_BYTE(ppixel, COLOR_RED);
	    gval = GET_DATA_BYTE(ppixel, COLOR_GREEN);
	    bval = GET_DATA_BYTE(ppixel, COLOR_BLUE);
	    octindex = rtab[rval] | gtab[gval] | btab[bval];
	    oval = octarray[octindex];
	    if (oval == 0) {
	        octarray[octindex] = cindex;
		colorarray[cindex] = *ppixel;
		setPixelLow(lined, j, depth, cindex - 1);
		cindex++;
	    }
	    else {  /* already have seen this color; is it unique? */
		setPixelLow(lined, j, depth, oval - 1);
	        if (colorarray[oval] != *ppixel)
		    nerrors++;
	    }
	}
    }
    *pnerrors = nerrors;

#if  DEBUG_CONVERT_TO_COLORMAP
    for (i = 0; i < ncolors; i++)
        fprintf(stderr, "color[%d] = %x\n", i, colorarray[i + 1]);
#endif  /* DEBUG_CONVERT_TO_COLORMAP */

        /* make the colormap */
    cmap = pixcmapCreate(depth);
    for (i = 0; i < ncolors; i++) {
	ppixel = colorarray + i + 1;
	rval = GET_DATA_BYTE(ppixel, COLOR_RED);
	gval = GET_DATA_BYTE(ppixel, COLOR_GREEN);
	bval = GET_DATA_BYTE(ppixel, COLOR_BLUE);
	pixcmapAddColor(cmap, rval, gval, bval);
    }
    pixSetColormap(pixd, cmap);

    numaDestroy(&na);
    FREE((void *)octarray);
    FREE((void *)colorarray);
    FREE((void *)rtab);
    FREE((void *)gtab);
    FREE((void *)btab);

    return pixd;
}


/*---------------------------------------------------------------------------*
 *                    Conversion from 16 bpp to 8 bpp                        *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvert16To8()
 *     
 *      Input:  pixs (16 bpp)
 *              whichbyte (1 for MSB, 0 for LSB)
 *      Return: pixd (8 bpp), or null on error
 *
 *  For each dest pixel, use either the MSB or LSB of each src pixel.
 */
PIX *
pixConvert16To8(PIX     *pixs,
                l_int32  whichbyte)
{
l_uint16   dsword;
l_int32    w, h, wpls, wpld, i, j;
l_uint32   sword;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    PROCNAME("pixConvert16To8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 16)
	return (PIX *)ERROR_PTR("pixs not 16 bpp", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* convert 2 pixels at a time */
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	if (whichbyte == 0) {  /* LSB */
	    for (j = 0; j < wpls; j++) {
		sword = *(lines + j);
		dsword = ((sword >> 8) & 0xff00) | (sword & 0xff);
		SET_DATA_TWO_BYTES(lined, j, dsword);
	    }
        }
	else {  /* MSB */
	    for (j = 0; j < wpls; j++) {
		sword = *(lines + j);
		dsword = ((sword >> 16) & 0xff00) | ((sword >> 8) & 0xff);
		SET_DATA_TWO_BYTES(lined, j, dsword);
	    }
        }
    }

    return pixd;
}
    


/*---------------------------------------------------------------------------*
 *                Conversion from grayscale to false color
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertGrayToFalseColor()
 *
 *      Input:  pixs (8 or 16 bpp grayscale)
 *              gamma factor (0.0 or 1.0 for default; > 1.0 for brighter;
 *                            2.0 is quite nice)
 *      Return: pixd (8 bpp with colormap), or null on error
 *
 *  Note:
 *      - For 8 bpp input, this simply adds a colormap to the input image.
 *      - For 16 bpp input, it first converts to 8 bpp and then
 *        adds the colormap.
 *      - The colormap is modeled after the Matlab "jet" configuration.
 */     
PIX *
pixConvertGrayToFalseColor(PIX       *pixs,
                           l_float32  gamma)
{
l_int32    d, i, rval, bval, gval;
l_int32   *curve;
l_float64  invgamma, x;
PIX       *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixConvertGrayToFalseColor");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 16)	
	return (PIX *)ERROR_PTR("pixs not 8 or 16 bpp", procName, NULL);

    if (d == 16)
        pixd = pixConvert16To8(pixs, 1);
    else {  /* d == 8 */
        if (pixGetColormap(pixs))
            pixd = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
        else
            pixd = pixCopy(NULL, pixs);
    }
    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    if ((cmap = pixcmapCreate(8)) == NULL)
        return (PIX *)ERROR_PTR("cmap not made", procName, NULL);
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);

        /* generate curve for transition part of color map */
    if ((curve = (l_int32 *)CALLOC(64, sizeof(l_int32)))== NULL)
	return (PIX *)ERROR_PTR("curve not made", procName, NULL);
    if (gamma == 0.0) gamma = 1.0;
    invgamma = 1. / gamma;
    for (i = 0; i < 64; i++) {
        x = (l_float64)i / 64.;
	curve[i] = (l_int32)(255. * pow(x, invgamma) + 0.5);
    }

    for (i = 0; i < 256; i++) {
        if (i < 32) {
	    rval = 0;
	    gval = 0;
	    bval = curve[i + 32];
	}
	else if (i < 96) {   /* 32 - 95 */
	    rval = 0;
	    gval = curve[i - 32];
	    bval = 255;
	}
	else if (i < 160) {  /* 96 - 159 */
	    rval = curve[i - 96];
	    gval = 255;
	    bval = curve[159 - i];
	}
	else if (i < 224) {  /* 160 - 223 */
	    rval = 255;
	    gval = curve[223 - i];
	    bval = 0;
	}
	else {  /* 224 - 255 */
	    rval = curve[287 - i];
	    gval = 0;
	    bval = 0;
	}
	pixcmapAddColor(cmap, rval, gval, bval);
    }

    FREE((void *)curve);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *            Unpacking conversion from 1 bpp to 8, 16 and 32 bpp            *
 *---------------------------------------------------------------------------*/
/*!
 *  pixUnpackBinary()
 *
 *      Input:  pixs (1 bpp)
 *              depth (of destination: 8, 16 or 32 bpp)
 *              invert (0:  binary 0 --> grayscale 0
 *                          binary 1 --> grayscale 0xff...
 *                      1:  binary 0 --> grayscale 0xff...
 *                          binary 1 --> grayscale 0)
 *      Return: pixd (8, 16 or 32 bpp), or null on error
 *
 *  Note: This function calls special cases of pixConvert1To*(),
 *        for 8, 16 and 32 bpp destinations.
 */     
PIX *
pixUnpackBinary(PIX     *pixs,
                l_int32  depth,
                l_int32  invert)
{
PIX       *pixd;

    PROCNAME("pixUnpackBinary");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, NULL);
    if (depth != 8 && depth != 16 && depth != 32)
	return (PIX *)ERROR_PTR("depth not 8, 16 or 32 bpp", procName, NULL);

    if (depth == 8) {
        if (invert == 0)
	    pixd = pixConvert1To8(NULL, pixs, 0, 255);
	else  /* invert bits */
	    pixd = pixConvert1To8(NULL, pixs, 255, 0);
    }
    else if (depth == 16) {
        if (invert == 0)
	    pixd = pixConvert1To16(NULL, pixs, 0, 0xffff);
	else  /* invert bits */
	    pixd = pixConvert1To16(NULL, pixs, 0xffff, 0);
    }
    else {
        if (invert == 0)
	    pixd = pixConvert1To32(NULL, pixs, 0, 0xffffffff);
	else  /* invert bits */
	    pixd = pixConvert1To32(NULL, pixs, 0xffffffff, 0);
    }

    return pixd;
}


/*!
 *  pixConvert1To16()
 *
 *      Input:  pixd (<optional> 16 bpp, can be null)
 *              pixs (1 bpp)
 *              val0 (16 bit value to be used for 0s in pixs)
 *              val1 (16 bit value to be used for 1s in pixs)
 *      Return: pixd (16 bpp)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 */     
PIX *
pixConvert1To16(PIX      *pixd,
                PIX      *pixs,
                l_uint16  val0,
	        l_uint16  val1)
{
l_int32    w, h, i, j, dibit, ndibits, wpls, wpld;
l_uint16   val[2];
l_uint32   index;
l_uint32  *tab, *datas, *datad, *lines, *lined;

    PROCNAME("pixConvert1To16");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (pixd) {
        if (w != pixGetWidth(pixd) || h != pixGetHeight(pixd))
	    return (PIX *)ERROR_PTR("pix sizes unequal", procName, pixd);
        if (pixGetDepth(pixd) != 16)
	    return (PIX *)ERROR_PTR("pixd not 16 bpp", procName, pixd);
    }
    else {
        if ((pixd = pixCreate(w, h, 16)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    }
    pixCopyResolution(pixd, pixs);

        /* use a table to convert 2 src bits at a time */
    if ((tab = (l_uint32 *)CALLOC(4, sizeof(l_uint32))) == NULL) 
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    val[0] = val0;
    val[1] = val1;
    for (index = 0; index < 4; index++) {
	tab[index] = (val[(index >> 1) & 1] << 16) | val[index & 1];
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    ndibits = (w + 1) / 2;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < ndibits; j++) {
	    dibit = GET_DATA_DIBIT(lines, j);
	    lined[j] = tab[dibit];
	}
    }

    FREE((void *)tab);
    return pixd;
}
 

/*!
 *  pixConvert1To32()
 *
 *      Input:  pixd (<optional> 32 bpp, can be null)
 *              pixs (1 bpp)
 *              val0 (32 bit value to be used for 0s in pixs)
 *              val1 (32 bit value to be used for 1s in pixs)
 *      Return: pixd (32 bpp)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 */     
PIX *
pixConvert1To32(PIX      *pixd,
                PIX      *pixs,
                l_uint32  val0,
	        l_uint32  val1)
{
l_int32    w, h, i, j, wpls, wpld, bit;
l_uint32   val[2];
l_uint32  *datas, *datad, *lines, *lined;

    PROCNAME("pixConvert1To32");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (pixd) {
        if (w != pixGetWidth(pixd) || h != pixGetHeight(pixd))
	    return (PIX *)ERROR_PTR("pix sizes unequal", procName, pixd);
        if (pixGetDepth(pixd) != 32)
	    return (PIX *)ERROR_PTR("pixd not 32 bpp", procName, pixd);
    }
    else {
        if ((pixd = pixCreate(w, h, 32)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    }
    pixCopyResolution(pixd, pixs);

    val[0] = val0;
    val[1] = val1;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j <w; j++) {
	    bit = GET_DATA_BIT(lines, j);
	    lined[j] = val[bit];
	}
    }

    return pixd;
}
 

/*---------------------------------------------------------------------------*
 *                    Conversion from 1 bpp to 2 bpp                         *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvert1To2Cmap()
 *
 *      Input:  pixs (1 bpp)
 *      Return: pixd (2 bpp, cmapped)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 *      (3) Input 0 is mapped to (255, 255, 255); 1 is mapped to (0, 0, 0)
 */     
PIX *
pixConvert1To2Cmap(PIX  *pixs)
{
PIX      *pixd;
PIXCMAP  *cmap;

    PROCNAME("pixConvert1To2Cmap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, pixd);

    if ((pixd = pixConvert1To2(NULL, pixs, 0, 1)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, pixd);
    cmap = pixcmapCreate(2);
    pixcmapAddColor(cmap, 255, 255, 255);
    pixcmapAddColor(cmap, 0, 0, 0);
    pixSetColormap(pixd, cmap);

    return pixd;
}


/*!
 *  pixConvert1To2()
 *
 *      Input:  pixd (<optional> 2 bpp, can be null)
 *              pixs (1 bpp)
 *              val0 (2 bit value to be used for 0s in pixs)
 *              val1 (2 bit value to be used for 1s in pixs)
 *      Return: pixd (2 bpp)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 *      (3) A simple unpacking might use val0 = 0 and val1 = 3.
 *      (4) If you want a colormapped pixd, use pixConvert1To2Cmap().
 */     
PIX *
pixConvert1To2(PIX     *pixd,
               PIX     *pixs,
               l_int32  val0,
	       l_int32  val1)
{
l_int32    w, h, i, j, byteval, nbytes, wpls, wpld;
l_uint8    val[2];
l_uint32   index;
l_uint16  *tab;
l_uint32  *datas, *datad, *lines, *lined;

    PROCNAME("pixConvert1To2");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, pixd);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (pixd) {
        if (w != pixGetWidth(pixd) || h != pixGetHeight(pixd))
	    return (PIX *)ERROR_PTR("pix sizes unequal", procName, pixd);
        if (pixGetDepth(pixd) != 2)
	    return (PIX *)ERROR_PTR("pixd not 2 bpp", procName, pixd);
    }
    else {
        if ((pixd = pixCreate(w, h, 2)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    }
    pixCopyResolution(pixd, pixs);

        /* Use a table to convert 8 src bits to 16 dest bits */
    if ((tab = (l_uint16 *)CALLOC(256, sizeof(l_uint16))) == NULL) 
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    val[0] = val0;
    val[1] = val1;
    for (index = 0; index < 256; index++) {
	tab[index] = (val[(index >> 7) & 1] << 14) |
	             (val[(index >> 6) & 1] << 12) |
	             (val[(index >> 5) & 1] << 10) |
	             (val[(index >> 4) & 1] << 8) |
	             (val[(index >> 3) & 1] << 6) |
	             (val[(index >> 2) & 1] << 4) |
	             (val[(index >> 1) & 1] << 2) | val[index & 1];
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    nbytes = (w + 7) / 8;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < nbytes; j++) {
	    byteval = GET_DATA_BYTE(lines, j);
	    SET_DATA_TWO_BYTES(lined, j, tab[byteval]);
	}
    }

    FREE((void *)tab);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *                    Conversion from 1 bpp to 4 bpp                         *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvert1To4Cmap()
 *
 *      Input:  pixs (1 bpp)
 *      Return: pixd (4 bpp, cmapped)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 *      (3) Input 0 is mapped to (255, 255, 255); 1 is mapped to (0, 0, 0)
 */     
PIX *
pixConvert1To4Cmap(PIX  *pixs)
{
PIX      *pixd;
PIXCMAP  *cmap;

    PROCNAME("pixConvert1To4Cmap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, pixd);

    if ((pixd = pixConvert1To4(NULL, pixs, 0, 1)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, pixd);
    cmap = pixcmapCreate(4);
    pixcmapAddColor(cmap, 255, 255, 255);
    pixcmapAddColor(cmap, 0, 0, 0);
    pixSetColormap(pixd, cmap);

    return pixd;
}


/*!
 *  pixConvert1To4()
 *
 *      Input:  pixd (<optional> 4 bpp, can be null)
 *              pixs (1 bpp)
 *              val0 (4 bit value to be used for 0s in pixs)
 *              val1 (4 bit value to be used for 1s in pixs)
 *      Return: pixd (4 bpp)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 *      (3) A simple unpacking might use val0 = 0 and val1 = 15, or v.v.
 *      (4) If you want a colormapped pixd, use pixConvert1To4Cmap().
 */     
PIX *
pixConvert1To4(PIX     *pixd,
               PIX     *pixs,
               l_int32  val0,
	       l_int32  val1)
{
l_int32    w, h, i, j, byteval, nbytes, wpls, wpld;
l_uint8    val[2];
l_uint32   index;
l_uint32  *tab, *datas, *datad, *lines, *lined;

    PROCNAME("pixConvert1To4");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, pixd);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (pixd) {
        if (w != pixGetWidth(pixd) || h != pixGetHeight(pixd))
	    return (PIX *)ERROR_PTR("pix sizes unequal", procName, pixd);
        if (pixGetDepth(pixd) != 4)
	    return (PIX *)ERROR_PTR("pixd not 4 bpp", procName, pixd);
    }
    else {
        if ((pixd = pixCreate(w, h, 4)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    }
    pixCopyResolution(pixd, pixs);

        /* Use a table to convert 8 src bits to 32 bit dest word */
    if ((tab = (l_uint32 *)CALLOC(256, sizeof(l_uint32))) == NULL) 
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    val[0] = val0;
    val[1] = val1;
    for (index = 0; index < 256; index++) {
	tab[index] = (val[(index >> 7) & 1] << 28) |
	             (val[(index >> 6) & 1] << 24) |
	             (val[(index >> 5) & 1] << 20) |
	             (val[(index >> 4) & 1] << 16) |
	             (val[(index >> 3) & 1] << 12) |
	             (val[(index >> 2) & 1] << 8) |
	             (val[(index >> 1) & 1] << 4) | val[index & 1];
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    nbytes = (w + 7) / 8;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < nbytes; j++) {
	    byteval = GET_DATA_BYTE(lines, j);
	    lined[j] = tab[byteval];
	}
    }

    FREE((void *)tab);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *               Conversion from 1, 2 and 4 bpp to 8 bpp                     *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertTo8()
 *
 *      Input:  pixs (1, 2, 4 or 16 bpp)
 *      Return: pixd (8 bpp), or null on error
 *
 *  Notes:
 *      - Top-level function, with simple default values for unpacking.
 *      - pixd never has a colormap.
 *      - 1 bpp:  val0 = 255, val1 = 0
 *      - 2 bpp:  replication: val0 = 0, val1 = 0x55, val2 = 0xaa, val3 = 255
 *      - 4 bpp:  always uses replication
 *      - 16 bpp:  use MSB
 */     
PIX *
pixConvertTo8(PIX  *pixs)
{
l_int32    d;

    PROCNAME("pixConvertTo8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);

    d = pixGetDepth(pixs);
    if (d == 1)
        return pixConvert1To8(NULL, pixs, 255, 0);
    else if (d == 2)
        return pixConvert2To8(pixs, 0, 85, 170, 255, FALSE);
    else if (d == 4)
        return pixConvert4To8(pixs, FALSE);
    else if (d == 16)
        return pixConvert16To8(pixs, 1);
    else 
	return (PIX *)ERROR_PTR("depth not 1, 2, 4 or 16 bpp", procName, NULL);
}


/*!
 *  pixConvert1To8()
 *
 *      Input:  pixd (<optional> 8 bpp, can be null)
 *              pixs (1 bpp)
 *              val0 (8 bit value to be used for 0s in pixs)
 *              val1 (8 bit value to be used for 1s in pixs)
 *      Return: pixd (8 bpp)
 *
 *  Notes:
 *      (1) If pixd is null, a new pix is made.
 *      (2) If pixd is not null, it must be of equal width and height
 *          as pixs.  It is always returned.
 *      (3) A simple unpacking might use val0 = 0 and val1 = 255, or v.v.
 *      (4) In a typical application where one wants to use a colormap
 *          with the dest, you can use val0 = 0, val1 = 1 to make a
 *          non-cmapped 8 bpp pix, and then make a colormap and set 0
 *          and 1 to the desired colors.  Here is an example:
 *             pixd = pixConvert1To8(NULL, pixs, 0, 1);
 *             cmap = pixcreate(8);
 *             pixcmapAddColor(cmap, 255, 255, 255);
 *             pixcmapAddColor(cmap, 0, 0, 0);
 *             pixSetColormap(pixd, cmap);
 */     
PIX *
pixConvert1To8(PIX     *pixd,
               PIX     *pixs,
               l_uint8  val0,
	       l_uint8  val1)
{
l_int32    w, h, i, j, qbit, nqbits, wpls, wpld;
l_uint8    val[2];
l_uint32   index;
l_uint32  *tab, *datas, *datad, *lines, *lined;

    PROCNAME("pixConvert1To8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetDepth(pixs) != 1)
	return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, pixd);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (pixd) {
        if (w != pixGetWidth(pixd) || h != pixGetHeight(pixd))
	    return (PIX *)ERROR_PTR("pix sizes unequal", procName, pixd);
        if (pixGetDepth(pixd) != 8)
	    return (PIX *)ERROR_PTR("pixd not 8 bpp", procName, pixd);
    }
    else {
        if ((pixd = pixCreate(w, h, 8)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    }
    pixCopyResolution(pixd, pixs);

        /* use a table to convert 4 src bits at a time */
    if ((tab = (l_uint32 *)CALLOC(16, sizeof(l_uint32))) == NULL) 
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    val[0] = val0;
    val[1] = val1;
    for (index = 0; index < 16; index++) {
	tab[index] = (val[(index >> 3) & 1] << 24) |
	             (val[(index >> 2) & 1] << 16) |
	             (val[(index >> 1) & 1] << 8) | val[index & 1];
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    nqbits = (w + 3) / 4;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < nqbits; j++) {
	    qbit = GET_DATA_QBIT(lines, j);
	    lined[j] = tab[qbit];
	}
    }

    FREE((void *)tab);
    return pixd;
}
 

/*!
 *  pixConvert2To8()
 *
 *      Input:  pixs (2 bpp)
 *              val0 (8 bit value to be used for 00 in pixs)
 *              val1 (8 bit value to be used for 01 in pixs)
 *              val2 (8 bit value to be used for 10 in pixs)
 *              val3 (8 bit value to be used for 11 in pixs)
 *              cmapflag (TRUE if pixd is to have a colormap; FALSE otherwise)
 *      Return: pixd (8 bpp), or null on error
 *
 *  Notes:
 *      - A simple unpacking might use val0 = 0,
 *        val1 = 85 (0x55), val2 = 170 (0xaa), val3 = 255.
 *      - If cmapflag is TRUE:
 *          - The 8 bpp image is made with a colormap.
 *          - If pixs has a colormap, the input values are ignored and
 *            the 8 bpp image is made using the colormap
 *          - If pixs does not have a colormap, the input values are
 *            used to build the colormap.
 *      - If cmapflag is FALSE:
 *          - The 8 bpp image is made without a colormap.
 *          - If pixs has a colormap, the input values are ignored,
 *            the colormap is removed, and the values stored in the 8 bpp
 *            image are from the colormap.
 *          - If pixs does not have a colormap, the input values are
 *            used to populate the 8 bpp image.
 */     
PIX *
pixConvert2To8(PIX     *pixs,
               l_uint8  val0,
	       l_uint8  val1,
	       l_uint8  val2,
	       l_uint8  val3,
               l_int32  cmapflag)
{
l_int32    w, h, i, j, nbytes, wpls, wpld, dibit, ncolor;
l_int32    rval, gval, bval, byte;
l_uint8    val[4];
l_uint32   index;
l_uint32  *tab, *datas, *datad, *lines, *lined;
PIX       *pixd;
PIXCMAP   *cmaps, *cmapd;

    PROCNAME("pixConvert2To8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 2)
	return (PIX *)ERROR_PTR("pixs not 2 bpp", procName, NULL);

    cmaps = pixGetColormap(pixs);
    if (cmaps && cmapflag == FALSE)
        return pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if (cmapflag == TRUE) {  /* pixd will have a colormap */
        cmapd = pixcmapCreate(8);  /* 8 bpp standard cmap */
        if (cmaps) {  /* use the existing colormap from pixs */
            ncolor = pixcmapGetCount(cmaps);
            for (i = 0; i < ncolor; i++) {
                pixcmapGetColor(cmaps, i, &rval, &gval, &bval);
                pixcmapAddColor(cmapd, rval, gval, bval);
            }
        }
        else {  /* make a colormap from the input values */
            pixcmapAddColor(cmapd, val0, val0, val0);
            pixcmapAddColor(cmapd, val1, val1, val1);
            pixcmapAddColor(cmapd, val2, val2, val2);
            pixcmapAddColor(cmapd, val3, val3, val3);
        }
        pixSetColormap(pixd, cmapd);
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                dibit = GET_DATA_DIBIT(lines, j);
                SET_DATA_BYTE(lined, j, dibit);
            }
        }
        return pixd;
    }

        /* Last case: no colormap in either pixs or pixd.
         * Use input values and build a table to convert 1 src byte
         * (4 src pixels) at a time */
    if ((tab = (l_uint32 *)CALLOC(256, sizeof(l_uint32))) == NULL) 
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    val[0] = val0;
    val[1] = val1;
    val[2] = val2;
    val[3] = val3;
    for (index = 0; index < 256; index++) {
	tab[index] = (val[(index >> 6) & 3] << 24) |
	             (val[(index >> 4) & 3] << 16) |
	             (val[(index >> 2) & 3] << 8) | val[index & 3];
    }

    nbytes = (w + 3) / 4;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < nbytes; j++) {
	    byte = GET_DATA_BYTE(lines, j);
	    lined[j] = tab[byte];
	}
    }

    FREE((void *)tab);
    return pixd;
}
 

/*!
 *  pixConvert4To8()
 *
 *      Input:  pixs (4 bpp)
 *              cmapflag (TRUE if pixd is to have a colormap; FALSE otherwise)
 *      Return: pixd (8 bpp), or null on error
 *
 *  Notes:
 *      - If cmapflag is TRUE:
 *          - pixd is made with a colormap.
 *          - If pixs has a colormap, it is copied and the colormap
 *            index values are placed in pixd.
 *          - If pixs does not have a colormap, a colormap with linear
 *            trc is built and the pixel values in pixs are placed in
 *            pixd as colormap index values.
 *      - If cmapflag is FALSE:
 *          - pixd is made without a colormap.
 *          - If pixs has a colormap, it is removed and the values stored
 *            in pixd are from the colormap (converted to gray).
 *          - If pixs does not have a colormap, the pixel values in pixs
 *            are used, with shift replication, to populate pixd.
 */     
PIX *
pixConvert4To8(PIX     *pixs,
               l_int32  cmapflag)
{
l_int32    w, h, i, j, wpls, wpld, ncolor;
l_int32    rval, gval, bval, byte, qbit;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;
PIXCMAP   *cmaps, *cmapd;

    PROCNAME("pixConvert4To8");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 4)
	return (PIX *)ERROR_PTR("pixs not 4 bpp", procName, NULL);

    cmaps = pixGetColormap(pixs);
    if (cmaps && cmapflag == FALSE)
        return pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if (cmapflag == TRUE) {  /* pixd will have a colormap */
        cmapd = pixcmapCreate(8);
        if (cmaps) {  /* use the existing colormap from pixs */
            ncolor = pixcmapGetCount(cmaps);
            for (i = 0; i < ncolor; i++) {
                pixcmapGetColor(cmaps, i, &rval, &gval, &bval);
                pixcmapAddColor(cmapd, rval, gval, bval);
            }
        }
        else {  /* make a colormap with a linear trc */
            for (i = 0; i < 16; i++) 
                pixcmapAddColor(cmapd, 17 * i, 17 * i, 17 * i);
        }
        pixSetColormap(pixd, cmapd);
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
    	    for (j = 0; j < w; j++) {
	        qbit = GET_DATA_QBIT(lines, j);
		SET_DATA_BYTE(lined, j, qbit);
            }
	}
	return pixd;
    }

        /* Last case: no colormap in either pixs or pixd.
         * Replicate the qbit value into 8 bits. */
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
	for (j = 0; j < w; j++) {
	    qbit = GET_DATA_QBIT(lines, j);
	    byte = (qbit << 4) | qbit;
	    SET_DATA_BYTE(lined, j, byte);
	}
    }
    return pixd;
}
 

/*---------------------------------------------------------------------------*
 *            Conversion from 1, 2, 4, 8, and 16 bpp to 32 bpp               *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertTo32()
 *
 *      Input:  pixs (1, 2, 4, 8, 16 or 32 bpp)
 *      Return: pixd (32 bpp), or null on error
 *
 *  Usage: Top-level function, with simple default values for unpacking.
 *      1 bpp:  val0 = 255, val1 = 0
 *              and then replication into R, G and B components
 *      2 bpp:  if colormapped, use the colormap values; otherwise,
 *              use val0 = 0, val1 = 0x55, val2 = 0xaa, val3 = 255
 *              and replicate gray into R, G and B components
 *      4 bpp:  if colormapped, use the colormap values; otherwise,
 *              replicate 2 nybs into a byte, and then into R,G,B components
 *      8 bpp:  if colormapped, use the colormap values; otherwise,
 *              replicate gray values into R, G and B components
 *      16 bpp:  replicate MSB into R, G and B components
 *      32 bpp:  makes a copy
 */     
PIX *
pixConvertTo32(PIX  *pixs)
{
l_int32  d;
PIX     *pixt, *pixd;

    PROCNAME("pixConvertTo32");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);

    d = pixGetDepth(pixs);
    if (d == 1)
        return pixConvert1To32(NULL, pixs, 0xffffffff, 0);
    else if (d == 2) {
        pixt = pixConvert2To8(pixs, 0, 85, 170, 255, TRUE);
        pixd = pixConvert8To32(pixt);
	pixDestroy(&pixt);
	return pixd;
    }
    else if (d == 4) {
        pixt = pixConvert4To8(pixs, TRUE);
        pixd = pixConvert8To32(pixt);
	pixDestroy(&pixt);
	return pixd;
    }
    else if (d == 8)
        return pixConvert8To32(pixs);
    else if (d == 16) {
        pixt = pixConvert16To8(pixs, 1);
        pixd = pixConvert8To32(pixt);
	pixDestroy(&pixt);
	return pixd;
    }
    else if (d == 32)
        return pixCopy(NULL, pixs);
    else
	return (PIX *)ERROR_PTR("depth not 1, 2, 4, 8, 16, 32 bpp",
	                        procName, NULL);
}


/*!
 *  pixConvert8To32()
 *
 *      Input:  pix (8 bpp)
 *      Return: 32 bpp rgb pix, or null on error
 *
 *  Note: If there is no colormap, replicates the gray value
 *        into the 3 MSB of the dest pixel.
 */
PIX *
pixConvert8To32(PIX  *pixs)
{
l_int32    i, j, w, h, wpls, wpld, val;
l_uint32  *datas, *datad, *lines, *lined;
l_uint32  *tab;
PIX       *pixd;

    PROCNAME("pixConvert8To32");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 8)
	return (PIX *)ERROR_PTR("pixs not 8 bpp", procName, NULL);

    if (pixGetColormap(pixs))
        return pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);

        /* else, no colormap */
    if ((tab = (l_uint32 *)CALLOC(256, sizeof(l_uint32))) == NULL)
	return (PIX *)ERROR_PTR("tab not made", procName, NULL);
    for (i = 0; i < 256; i++)
      tab[i] = (i << 24) | (i << 16) | (i << 8);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(w, h, 32)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    pixCopyResolution(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    
    for (i = 0; i < h; i++) {
	lines = datas + i * wpls;
	lined = datad + i * wpld;
	for (j = 0; j < w; j++) {
	    val = GET_DATA_BYTE(lines, j);
	    lined[j] = tab[val];
	}
    }

    FREE((void *)tab);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *                     Conversion for printing in PostScript                 *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertForPSWrap()
 *
 *      Input:  pixs (1, 2, 4, 8, 16, 32 bpp)
 *      Return: pixd (1, 8, or 32 bpp), or null on error
 *
 *  Notes:
 *      (1) For wrapping in PostScript, we convert pixs to
 *          1 bpp, 8 bpp (gray) and 32 bpp (RGB color).
 *      (2) Colormaps are removed.  For pixs with colormaps, the
 *          images are converted to either 8 bpp gray or 32 bpp
 *          RGB, depending on whether the colormap has color content.
 *      (3) Images without colormaps, that are not 1 bpp or 32 bpp,
 *          are converted to 8 bpp gray.
 */     
PIX *
pixConvertForPSWrap(PIX  *pixs)
{
l_int32   d;
PIX      *pixd;
PIXCMAP  *cmap;

    PROCNAME("pixConvertForPSWrap");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);

    cmap = pixGetColormap(pixs);
    d = pixGetDepth(pixs);
    switch (d)
    {
    case 1:
    case 32:
        pixd = pixClone(pixs);
	break;
    case 2:
	if (cmap)
            pixd = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
	else
	    pixd = pixConvert2To8(pixs, 0, 0x55, 0xaa, 0xff, FALSE);
	break;
    case 4:
	if (cmap)
            pixd = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
	else
	    pixd = pixConvert4To8(pixs, FALSE);
	break;
    case 8:
        pixd = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
	break;
    case 16:
	pixd = pixConvert16To8(pixs, 1);
	break;
    default:
        fprintf(stderr, "depth not in {1, 2, 4, 8, 16, 32}");
	return NULL;
   }

   return pixd;
}


/*---------------------------------------------------------------------------*
 *                  Colorspace conversion between RGB and HSB                *
 *---------------------------------------------------------------------------*/
/*!
 *  pixConvertRGBToHSV()
 *
 *      Input:  pixd (can be NULL; if not NULL, must == pixs)
 *              pixs
 *      Return: pixd always
 *
 *  Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The definition of our HSV space is given in convertRGBToHSV().
 *      (3) The h, s and v values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 */
PIX *
pixConvertRGBToHSV(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, hval, sval, vval;
l_uint32   pixel;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    PROCNAME("pixConvertRGBToHSV");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixd && pixd != pixs)
	return (PIX *)ERROR_PTR("pixd defined and not inplace", procName, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
	return (PIX *)ERROR_PTR("not cmapped or rgb", procName, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertRGBToHSV(cmap);
        return pixd;
    }

        /* Convert RGB image */
    w = pixGetWidth(pixd);
    h = pixGetHeight(pixd);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            pixel = line[j];
            rval = GET_DATA_BYTE(&pixel, COLOR_RED);
            gval = GET_DATA_BYTE(&pixel, COLOR_GREEN);
            bval = GET_DATA_BYTE(&pixel, COLOR_BLUE);
            convertRGBToHSV(rval, gval, bval, &hval, &sval, &vval);
            line[j] = (hval << 24) | (sval << 16) | (vval << 8);
        }
    }

    return pixd;
}


/*!
 *  pixConvertHSVToRGB()
 *
 *      Input:  pixd (can be NULL; if not NULL, must == pixs)
 *              pixs
 *      Return: pixd always
 *
 *  Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The user takes responsibility for making sure that pixs is
 *          in our HSV space.  The definition of our HSV space is given
 *          in convertRGBToHSV().
 *      (3) The h, s and v values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 */
PIX *
pixConvertHSVToRGB(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, hval, sval, vval;
l_uint32   pixel;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    PROCNAME("pixConvertHSVToRGB");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixd && pixd != pixs)
	return (PIX *)ERROR_PTR("pixd defined and not inplace", procName, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
	return (PIX *)ERROR_PTR("not cmapped or hsv", procName, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertHSVToRGB(cmap);
        return pixd;
    }

        /* Convert HSV image */
    w = pixGetWidth(pixd);
    h = pixGetHeight(pixd);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            pixel = line[j];
            hval = pixel >> 24;
            sval = (pixel >> 16) & 0xff;
            vval = (pixel >> 8) & 0xff;
            convertHSVToRGB(hval, sval, vval, &rval, &gval, &bval);
            SET_DATA_BYTE(line + j, COLOR_RED, rval);
            SET_DATA_BYTE(line + j, COLOR_GREEN, gval);
            SET_DATA_BYTE(line + j, COLOR_BLUE, bval);
        }
    }

    return pixd;
}


/*!
 *  convertRGBToHSV()
 *
 *      Input:  rval, gval, bval (RGB input)
 *              &hval, &sval, &vval (<return> HSV values)
 *      Return: 0 if OK, 1 on error
 *
 *  Notes:
 *      (1) The range of returned values is:
 *            h [0 ... 240]
 *            s [0 ... 255]
 *            v [0 ... 255]
 *      (2) If r = g = b, the pixel is gray (s = 0), and we define h = 0.
 *      (3) h wraps around, so that h = 0 and h = 240 are equivalent
 *          in hue space.
 *      (4) h has the following correspondence to color:
 *            h = 0         magenta
 *            h = 40        red
 *            h = 80        yellow
 *            h = 120       green
 *            h = 160       cyan
 *            h = 200       blue
 *            h = 240       magenta
 */     
l_int32
convertRGBToHSV(l_int32   rval,
                l_int32   gval,
                l_int32   bval,
                l_int32  *phval,
                l_int32  *psval,
                l_int32  *pvval)
{
l_int32    minrg, maxrg, min, max, delta;
l_float32  h;

    PROCNAME("convertRGBToHSV");

    if (!phval || !psval || !pvval)
	return ERROR_INT("&hval, &sval, &vval not all defined", procName, 1);

    minrg = L_MIN(rval, gval);
    min = L_MIN(minrg, bval);
    maxrg = L_MAX(rval, gval);
    max = L_MAX(maxrg, bval);
    delta = max - min;

    *pvval = max;
    if (delta == 0) {   /* gray; no chroma */
        *phval = 0;
        *psval = 0;
    }
    else {
        *psval = (l_int32)(255. * (l_float32)delta / (l_float32)max + 0.5);
        if (rval == max)  /* between magenta and yellow */
            h = (l_float32)(gval - bval) / (l_float32)delta; 
        else if (gval == max)  /* between yellow and cyan */
            h = 2. + (l_float32)(bval - rval) / (l_float32)delta; 
        else  /* between cyan and magenta */
            h = 4. + (l_float32)(rval - gval) / (l_float32)delta;
        h *= 40.0;
        if (h < 0.0)
          h += 240.0;
        *phval = (l_int32)(h + 0.5);
    }

    return 0;
}


/*!
 *  convertHSVToRGB()
 *
 *      Input:  hval, sval, vval
 *              &rval, &gval, &bval (<return> RGB values)
 *      Return: 0 if OK, 1 on error
 *
 *  Notes:
 *      (1) See convertRGBToHSV() for valid input range of HSV values
 *          and their interpretation in color space.
 */     
l_int32
convertHSVToRGB(l_int32   hval,
                l_int32   sval,
                l_int32   vval,
                l_int32  *prval,
                l_int32  *pgval,
                l_int32  *pbval)
{
l_int32   i, x, y, z;
l_float32 h, f, s;

    PROCNAME("convertHSVToRGB");

    if (!prval || !pgval || !pbval)
	return ERROR_INT("&rval, &gval, &bval not all defined", procName, 1);

    if (sval == 0) {  /* gray */
        *prval = vval;
        *pgval = vval;
        *pbval = vval;
    }
    else {
        if (hval < 0 || hval > 240)
            return ERROR_INT("invalid hval", procName, 1);
        if (hval == 240)
            hval = 0;
        h = (l_float32)hval / 40.;
        i = (l_int32)h;
        f = h - i;
        s = (l_float32)sval / 255.;
        x = (l_int32)(vval * (1. - s) + 0.5);
        y = (l_int32)(vval * (1. - s * f) + 0.5);
        z = (l_int32)(vval * (1. - s * (1. - f)) + 0.5);
        switch (i)
        {
        case 0:
            *prval = vval;
            *pgval = z;
            *pbval = x;
            break;
        case 1:
            *prval = y;
            *pgval = vval;
            *pbval = x;
            break;
        case 2:
            *prval = x;
            *pgval = vval;
            *pbval = z;
            break;
        case 3:
            *prval = x;
            *pgval = y;
            *pbval = vval;
            break;
        case 4:
            *prval = z;
            *pgval = x;
            *pbval = vval;
            break;
        case 5:
            *prval = vval;
            *pgval = x;
            *pbval = y;
            break;
        default:  /* none possible */
            return 1;
        }
    }
  
    return 0;
}