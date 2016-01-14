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
 *  enhance.c
 *
 *      Gamma TRC (tone reproduction curve) mapping
 *           PIX     *pixGammaTRC()     *** see warning ***   
 *           PIX     *pixGammaTRCMasked()     *** see warning ***   
 *           NUMA    *numaGammaTRC()
 *
 *      Contrast enhancement
 *           PIX     *pixContrastTRC()   *** see warning ***
 *           PIX     *pixContrastTRCMasked()   *** see warning ***
 *           NUMA    *numaContrastTRC()
 *
 *      Generic TRC mapper
 *           PIX     *pixTRCMap()       *** see warning ***
 *
 *      Unsharp-masking
 *           PIX     *pixUnsharpMask()
 *           PIX     *pixUnsharpMaskColor()
 *           PIX     *pixUnsharpMaskGray()
 *
 *      Edge by bandpass
 *           PIX     *pixHalfEdgeByBandpass()
 *
 *  *** Warning: these functions make an implicit assumption about
 *               RGB component ordering; namely R is MSB
 *
 *      Gamma correction and contrast enhancement apply a
 *      simple mapping function to each pixel (or, for color
 *      images, to each sample (i.e., r,g,b) of the pixel).
 *       - Gamma correction either lightens the image or darkens
 *         it, depending on whether the gamma factor is greater
 *         or less than 1.0, respectively.
 *       - Contrast enhancement darkens the pixels that are already
 *         darker than the middle of the dynamic range (128)
 *         and lightens pixels that are lighter than 128.
 *
 *      Unsharp masking is a more complicated enhancement.
 *      A "high frequency" image, generated by subtracting
 *      the smoothed ("low frequency") part of the image from
 *      itself, has all the energy at the edges.  This "edge image"
 *      has 0 average value.  A fraction of the edge image is
 *      then added to the original, enhancing the differences
 *      between pixel values at edges.  Because we represent
 *      images as l_uint8 arrays, we preserve dynamic range and
 *      handle negative values by doing all the arithmetic on
 *      shifted l_uint16 arrays; the l_uint8 values are recovered
 *      at the end.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "allheaders.h"

    /* scales contrast enhancement factor to have a useful range
     * between 0.0 and 1.0 */
static const l_float32  ENHANCE_SCALE_FACTOR = 5.;


/*-------------------------------------------------------------*
 *         Gamma TRC (tone reproduction curve) mapping         *
 *-------------------------------------------------------------*/
/*!
 *  pixGammaTRC()
 *
 *      Input:  pixd (<optional> null or equal to pixs)
 *              pixs (8 or 32 bpp; or 2, 4 or 8 bpp with colormap)
 *              gamma (gamma correction; must be > 0.0)
 *              minval  (input value that gives 0 for output; can be < 0)
 *              maxval  (input value that gives 255 for output; can be > 255)
 *      Return: pixd always
 *
 *  Notes:
 *      (1) pixd must either be null or equal to pixs.
 *          Set pixd == pixs to get in-place operation;
 *          set pixd == null to get new image;
 *      (2) If pixs is colormapped, the colormap is transformed,
 *          either in-place or in a copy of pixs.
 *      (3) We use a gamma mapping between minval and maxval.
 *      (4) If gamma < 1.0, the image will appear darker;
 *          if gamma > 1.0, the image will appear lighter;
 *          if gamma == 1.0, return a clone
 *      (5) For color images that are not colormapped, the mapping
 *          is applied to each component.
 *          *** Warning: implicit assumption about RGB component ordering ***
 *      (6) minval and maxval are not restricted to the interval [0, 255].
 *          If minval < 0, an input value of 0 is mapped to a
 *          nonzero output.  This will turn black to gray.
 *          If maxval > 255, an input value of 255 is mapped to
 *          an output value less than 255.  This will turn
 *          white (e.g., in the background) to gray.
 *      (7) Increasing minval darkens the image.
 *      (8) Decreasing maxval bleaches the image.
 *      (9) Simultaneously increasing minval and decreasing maxval
 *          will darken the image and make the colors more intense;
 *          e.g., minval = 50, maxval = 200.
 *      (10) See numaGammaTRC() for further examples of use.
 */
PIX *
pixGammaTRC(PIX       *pixd,
            PIX       *pixs,
	    l_float32  gamma,
	    l_int32    minval,
	    l_int32    maxval)
{
l_int32   d;
NUMA     *nag;
PIXCMAP  *cmap;

    PROCNAME("pixGammaTRC");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixd && (pixd != pixs))
	return (PIX *)ERROR_PTR("pixd not null or pixs", procName, pixd);
    if (gamma <= 0.0) {
	L_WARNING("gamma must be > 0.0; setting to 1.0", procName);
        gamma = 1.0;
    }
    if (minval >= maxval)
	return (PIX *)ERROR_PTR("minval not < maxval", procName, pixd);

    cmap = pixGetColormap(pixs);
    d = pixGetDepth(pixs);
    if (!cmap && d != 8 && d != 32)
	return (PIX *)ERROR_PTR("depth not 8 or 32 bpp", procName, pixd);

    if (!pixd)  /* start with a copy if not in-place */
        pixd = pixCopy(NULL, pixs);

    if (cmap) {
        pixcmapGammaTRC(pixGetColormap(pixd), gamma, minval, maxval);
	return pixd;
    }

        /* pixd is 8 or 32 bpp */
    if ((nag = numaGammaTRC(gamma, minval, maxval)) == NULL)
	return (PIX *)ERROR_PTR("nag not made", procName, pixd);
    pixTRCMap(pixd, NULL, nag);
    numaDestroy(&nag);

    return pixd;
}


/*!
 *  pixGammaTRCMasked()
 *
 *      Input:  pixd (<optional> null or equal to pixs)
 *              pixs (8 or 32 bpp; not colormapped)
 *              pixm (<optional> null or 1 bpp)
 *              gamma (gamma correction; must be > 0.0)
 *              minval  (input value that gives 0 for output; can be < 0)
 *              maxval  (input value that gives 255 for output; can be > 255)
 *      Return: pixd always
 *
 *  Notes:
 *      (1) Same as pixGammaTRC() except mapping is optionally over
 *          a subset of pixels described by pixm.
 *          *** Warning: implicit assumption about RGB component ordering ***
 *      (2) Masking does not work for colormapped images.
 *      (3) See pixGammaTRC() for details on how to use the parameters.
 */
PIX *
pixGammaTRCMasked(PIX       *pixd,
                  PIX       *pixs,
                  PIX       *pixm,
                  l_float32  gamma,
                  l_int32    minval,
                  l_int32    maxval)
{
l_int32   d;
NUMA     *nag;

    PROCNAME("pixGammaTRCMasked");

    if (!pixm)
        return pixGammaTRC(pixd, pixs, gamma, minval, maxval);

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetColormap(pixs))
	return (PIX *)ERROR_PTR("invalid: pixs has a colormap", procName, pixd);
    if (pixd && (pixd != pixs))
	return (PIX *)ERROR_PTR("pixd not null or pixs", procName, pixd);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
	return (PIX *)ERROR_PTR("depth not 8 or 32 bpp", procName, pixd);
    if (minval >= maxval)
	return (PIX *)ERROR_PTR("minval not < maxval", procName, pixd);

    if (gamma <= 0.0) {
	L_WARNING("gamma must be > 0.0; setting to 1.0", procName);
        gamma = 1.0;
    }

    if (!pixd)  /* start with a copy if not in-place */
        pixd = pixCopy(NULL, pixs);

    if ((nag = numaGammaTRC(gamma, minval, maxval)) == NULL)
	return (PIX *)ERROR_PTR("nag not made", procName, pixd);
    pixTRCMap(pixd, pixm, nag);
    numaDestroy(&nag);

    return pixd;
}


/*!
 *  numaGammaTRC()
 *
 *      Input:  gamma   (gamma factor; must be > 0.0)
 *              minval  (input value that gives 0 for output)
 *              maxval  (input value that gives 255 for output)
 *      Return: na, or null on error
 *
 *  Notes:
 *      (1) The map is returned as a numa; values are clipped to [0, 255].
 *      (2) To force all intensities into a range within fraction delta
 *          of white, use: minval = -256 * (1 - delta) / delta
 *                         maxval = 255
 *      (3) To force all intensities into a range within fraction delta
 *          of black, use: minval = 0
 *                         maxval = 256 * (1 - delta) / delta
 */
NUMA *
numaGammaTRC(l_float32  gamma,
             l_int32    minval,
	     l_int32    maxval)
{
l_int32    i, val;
l_float64  x, invgamma;
NUMA      *na;

    PROCNAME("numaGammaTRC");

    if (minval >= maxval)
	return (NUMA *)ERROR_PTR("minval not < maxval", procName, NULL);
    if (gamma <= 0.0) {
	L_WARNING("gamma must be > 0.0; setting to 1.0", procName);
        gamma = 1.0;
    }

    invgamma = 1. / gamma;
    na = numaCreate(256);
    for (i = 0; i < minval; i++)
        numaAddNumber(na, 0);
    for (i = minval; i <= maxval; i++) {
        if (i < 0) continue;
        if (i > 255) continue;
	x = (l_float64)(i - minval) / (l_float64)(maxval - minval);
	val = (l_int32)(255. * pow(x, invgamma) + 0.5);
	val = L_MAX(val, 0);
	val = L_MIN(val, 255);
	numaAddNumber(na, val);
    }
    for (i = maxval + 1; i < 256; i++)
        numaAddNumber(na, 255);

    return na;
}


/*-------------------------------------------------------------*
 *                      Contrast enhancement                   *
 *-------------------------------------------------------------*/
/*!
 *  pixContrastTRC()
 *
 *      Input:  pixd (<optional> null or equal to pixs)
 *              pixs (8 or 32 bpp; or 2, 4 or 8 bpp with colormap)
 *              factor  (0.0 is no enhancement)
 *      Return: pixd always
 *
 *  Notes:
 *      (1) pixd must either be null or equal to pixs.
 *          Set pixd == pixs to get in-place operation;
 *          set pixd == null to get new image;
 *      (2) If pixs is colormapped, the colormap is transformed,
 *          either in-place or in a copy of pixs.
 *      (3) Contrast is enhanced by mapping each color component
 *          using an atan function with maximum slope at 127.
 *          Pixels below 127 are lowered in intensity and pixels
 *          above 127 are increased.
 *      (4) The useful range for the contrast factor is scaled to
 *          be in (0.0 to 1.0), but larger values can also be used.
 *          0.0 corresponds to no enhancement.
 *      (5) For color images that are not colormapped, the mapping
 *          is applied to each component.
 *          *** Warning: implicit assumption about RGB component ordering ***
 */
PIX *
pixContrastTRC(PIX       *pixd,
               PIX       *pixs,
	       l_float32  factor)
{
l_int32   d;
NUMA     *nac;
PIXCMAP  *cmap;

    PROCNAME("pixContrastTRC");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixd && (pixd != pixs))
	return (PIX *)ERROR_PTR("pixd not null or pixs", procName, pixd);
    if (factor < 0.0) {
        L_WARNING("factor must be >= 0.0; using 0.0", procName);
	return pixClone(pixs);
    }

    cmap = pixGetColormap(pixs);
    d = pixGetDepth(pixs);
    if (!cmap && d != 8 && d != 32)
	return (PIX *)ERROR_PTR("depth not 8 or 32 bpp", procName, pixd);

    if (!pixd)  /* start with a copy if not in-place */
	pixd = pixCopy(NULL, pixs);

    if (cmap) {
	pixcmapContrastTRC(pixGetColormap(pixd), factor);
	return pixd;
    }

        /* pixd is 8 or 32 bpp */
    if ((nac = numaContrastTRC(factor)) == NULL)
	return (PIX *)ERROR_PTR("nac not made", procName, pixd);
    pixTRCMap(pixd, NULL, nac);
    numaDestroy(&nac);

    return pixd;
}


/*!
 *  pixContrastTRCMasked()
 *
 *      Input:  pixd (<optional> null or equal to pixs)
 *              pixs (8 or 32 bpp; or 2, 4 or 8 bpp with colormap)
 *              pixm (<optional> null or 1 bpp)
 *              factor  (0.0 is no enhancement)
 *      Return: pixd always
 *
 *  Notes:
 *      (1) Same as pixContrastTRC() except mapping is optionally over
 *          a subset of pixels described by pixm.
 *          *** Warning: implicit assumption about RGB component ordering ***
 *      (2) Masking does not work for colormapped images.
 *      (3) See pixContrastTRC() for details on how to use the parameters.
 */
PIX *
pixContrastTRCMasked(PIX       *pixd,
                     PIX       *pixs,
                     PIX       *pixm,
                     l_float32  factor)
{
l_int32   d;
NUMA     *nac;

    PROCNAME("pixContrastTRCMasked");

    if (!pixm)
        return pixContrastTRC(pixd, pixs, factor);

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, pixd);
    if (pixGetColormap(pixs))
	return (PIX *)ERROR_PTR("invalid: pixs has a colormap", procName, pixd);
    if (pixd && (pixd != pixs))
	return (PIX *)ERROR_PTR("pixd not null or pixs", procName, pixd);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
	return (PIX *)ERROR_PTR("depth not 8 or 32 bpp", procName, pixd);

    if (factor < 0.0) {
        L_WARNING("factor must be >= 0.0; using 0.0", procName);
	return pixClone(pixs);
    }

    if (!pixd)  /* start with a copy if not in-place */
	pixd = pixCopy(NULL, pixs);

    if ((nac = numaContrastTRC(factor)) == NULL)
	return (PIX *)ERROR_PTR("nac not made", procName, pixd);
    pixTRCMap(pixd, pixm, nac);
    numaDestroy(&nac);

    return pixd;
}


/*!
 *  numaContrastTRC()
 *
 *      Input:  factor (generally between 0.0 (no enhancement)
 *              and 1.0, but can be larger than 1.0)
 *      Return: na, or null on error
 *
 *  Notes:
 *      (1) The mapping is monotonic increasing, where 0 is mapped
 *          to 0 and 255 is mapped to 255. 
 *      (2) As 'factor' is increased from 0.0 (where the mapping is linear),
 *          the map gets closer to its limit as a step function that
 *          jumps from 0 to 255 at the center (input value = 127).
 */
NUMA *
numaContrastTRC(l_float32  factor)
{
l_int32    i, val;
l_float64  x, ymax, ymin, dely, scale;
NUMA      *na;

    PROCNAME("numaContrastTRC");

    if (factor < 0.0) {
        L_WARNING("factor must be >= 0.0; using 0.0; no enhancement", procName);
	return numaMakeSequence(0, 1, 256);  /* linear map */
    }

    scale = ENHANCE_SCALE_FACTOR;
    ymax = atan((l_float64)(1.0 * factor * scale));
    ymin = atan((l_float64)(-127. * factor * scale / 128.));
    dely = ymax - ymin;
    na = numaCreate(256);
    for (i = 0; i < 256; i++) {
	x = (l_float64)i;
	val = (l_int32)((255. / dely) *
	     (-ymin + atan((l_float64)(factor * scale * (x - 127.) / 128.))) +
		 0.5);
	numaAddNumber(na, val);
    }

    return na;
}


/*-------------------------------------------------------------*
 *                       Generic TRC mapping                   *
 *-------------------------------------------------------------*/
/*!
 *  pixTRCMap()
 *
 *      Input:  pixs (8 grayscale or 32 bpp rgb; not colormapped)
 *              pixm (<optional> 1 bpp mask)
 *              na (mapping array)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) This operation is in-place on pixs.
 *      (2) For 32 bpp, this applies the same map to each of the r,g,b
 *          components.
 *      (3) The mapping array is of size 256, and it maps the input
 *          index into values in the range [0, 255].
 *      (4) If defined, the optional 1 bpp mask pixm has its origin
 *          aligned with pixs, and the map function is applied only
 *          to pixels in pixs under the fg of pixm.
 *
 *  *** Warning: implicit assumption about RGB component ordering ***
 */
l_int32
pixTRCMap(PIX   *pixs,
	  PIX   *pixm,
          NUMA  *na)
{
l_int32    w, h, d, wm, hm, wpl, wplm, i, j, ival, sval8, dval8;
l_int32   *tab;
l_uint32   sval32, dval32;
l_uint32  *data, *datam, *line, *linem;

    PROCNAME("pixTRCMap");

    if (!pixs)
	return ERROR_INT("pixs not defined", procName, 1);
    if (pixGetColormap(pixs))
	return ERROR_INT("pixs is colormapped", procName, 1);
    if (!na)
	return ERROR_INT("na not defined", procName, 1);
    if (numaGetCount(na) != 256)
	return ERROR_INT("na not of size 256", procName, 1);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
	return ERROR_INT("pixs not 8 or 32 bpp", procName, 1);
    if (pixm) {
        if (pixGetDepth(pixm) != 1)
	    return ERROR_INT("pixm not 1 bpp", procName, 1);
    }

        /* Get an integer lut from the numa */
    if ((tab = (l_int32 *)CALLOC(256, sizeof(l_int32))) == NULL)
	return ERROR_INT("tab not made", procName, 1);
    for (i = 0; i < 256; i++) {
	numaGetIValue(na, i, &ival);
        tab[i] = ival;
    }

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    if (!pixm) {
	if (d == 8) {
	    for (i = 0; i < h; i++) {
		line = data + i * wpl;
		for (j = 0; j < w; j++) {
		    sval8 = GET_DATA_BYTE(line, j);
		    dval8 = tab[sval8];
		    SET_DATA_BYTE(line, j, dval8);
		}
	    }
	}
	else {  /* d == 32 */
	    for (i = 0; i < h; i++) {
		line = data + i * wpl;
		for (j = 0; j < w; j++) {
		    sval32 = *(line + j);
		    dval32 = tab[(sval32 >> 24)] << 24 |
			     tab[(sval32 >> 16) & 0xff] << 16 |
			     tab[(sval32 >> 8) & 0xff] << 8;
		    *(line + j) = dval32;
		}
	    }
	}
    }
    else {
        datam = pixGetData(pixm);
	wplm = pixGetWpl(pixm);
	wm = pixGetWidth(pixm);
	hm = pixGetHeight(pixm);
	if (d == 8) {
	    for (i = 0; i < h; i++) {
		if (i >= hm)
		    break;
		line = data + i * wpl;
		linem = datam + i * wplm;
		for (j = 0; j < w; j++) {
		    if (j >= wm)
		        break;
                    if (GET_DATA_BIT(linem, j) == 0)
		        continue;
		    sval8 = GET_DATA_BYTE(line, j);
		    dval8 = tab[sval8];
		    SET_DATA_BYTE(line, j, dval8);
		}
	    }
	}
	else {  /* d == 32 */
	    for (i = 0; i < h; i++) {
		if (i >= hm)
		    break;
		line = data + i * wpl;
		linem = datam + i * wplm;
		for (j = 0; j < w; j++) {
		    if (j >= wm)
		        break;
                    if (GET_DATA_BIT(linem, j) == 0)
		        continue;
		    sval32 = *(line + j);
		    dval32 = tab[(sval32 >> 24)] << 24 |
			     tab[(sval32 >> 16) & 0xff] << 16 |
			     tab[(sval32 >> 8) & 0xff] << 8;
		    *(line + j) = dval32;
		}
	    }
	}
    }

    FREE((void *)tab);
    return 0;
}



/*-------------------------------------------------------------*
 *                        Unsharp masking                      *
 *-------------------------------------------------------------*/
/*!
 *  pixUnsharpMask()
 *
 *      Input:  pix (8 or 32 bpp; or 2, 4 or 8 bpp with colormap)
 *              smooth  ("half-width" of smoothing filter)
 *              fract  (fraction of edge added back into image)
 *      Return: pixd, or null on error
 *
 *  Note: (1) We use symmetric smoothing filters of odd dimension,
 *            typically use 3, 5, 7, etc.  The smooth parameter
 *            for these is (size - 1)/2; i.e., 1, 2, 3, etc.
 *        (2) The fract parameter is typically taken in the
 *            range:  0.2 < fract < 0.7
 */
PIX *
pixUnsharpMask(PIX       *pix,
	       l_int32    smooth,
               l_float32  fract)
{
l_int32  d;
PIX     *pixs, *pixd;

    PROCNAME("pixUnsharpMask");

    if (!pix)
	return (PIX *)ERROR_PTR("pix not defined", procName, NULL);
    if (fract <= 0.0) {
	L_WARNING("no fraction added back in", procName);
	return pixClone(pix);
    }

        /* Remove colormap if necessary */
    d = pixGetDepth(pix);
    if ((d == 2 || d == 4 || d == 8) && pixGetColormap(pix)) {
	L_WARNING("pix has colormap; removing", procName);
	pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
	d = pixGetDepth(pixs);
    }
    else
	pixs = pixClone(pix);

    if (d != 8 && d != 32) {
	pixDestroy(&pixs);
	return (PIX *)ERROR_PTR("depth not 8 or 32 bpp", procName, NULL);
    }

    if (d == 8)
	pixd = pixUnsharpMaskGray(pixs, smooth, fract);
    else  /* d == 32 */
	pixd = pixUnsharpMaskColor(pixs, smooth, fract);
    pixDestroy(&pixs);
    
    return pixd;
}


/*!
 *  pixUnsharpMaskColor()
 *
 *      Input:  pixs (32 bpp; 24 bpp RGB color)
 *              smooth  ("half-width" of smoothing filter)
 *              fract  (fraction of edge added back into image)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) We use symmetric smoothing filters of odd dimension,
 *          typically use 3, 5, 7, etc.  The smooth parameter
 *          for these is (size - 1)/2; i.e., 1, 2, 3, etc.
 *      (2) The fract parameter is typically taken in the range:
 *           0.2 < fract < 0.7
 */
PIX *
pixUnsharpMaskColor(PIX       *pixs,
		    l_int32    smooth,
                    l_float32  fract)
{
PIX  *pixRed, *pixGreen, *pixBlue;
PIX  *pixRedSharp, *pixGreenSharp, *pixBlueSharp;
PIX  *pixd;

    PROCNAME("pixUnsharpMaskColor");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 32)
	return (PIX *)ERROR_PTR("pixs not 32 bpp", procName, NULL);

    if (fract <= 0.0) {
	L_WARNING("no fraction added back in", procName);
	return pixClone(pixs);
    }

    pixRed = pixGetRGBComponent(pixs, COLOR_RED);
    pixRedSharp = pixUnsharpMaskGray(pixRed, smooth, fract);
    pixDestroy(&pixRed);
    pixGreen = pixGetRGBComponent(pixs, COLOR_GREEN);
    pixGreenSharp = pixUnsharpMaskGray(pixGreen, smooth, fract);
    pixDestroy(&pixGreen);
    pixBlue = pixGetRGBComponent(pixs, COLOR_BLUE);
    pixBlueSharp = pixUnsharpMaskGray(pixBlue, smooth, fract);
    pixDestroy(&pixBlue);

    if ((pixd = pixCreateRGBImage(pixRedSharp, pixGreenSharp, pixBlueSharp))
            == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);

    pixDestroy(&pixRedSharp);
    pixDestroy(&pixGreenSharp);
    pixDestroy(&pixBlueSharp);

    return pixd;
}


/*!
 *  pixUnsharpMaskGray()
 *
 *      Input:  pixs (8 bpp)
 *              smooth  ("half-width" of smoothing filter)
 *              fract  (fraction of edge added back into image)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) We use symmetric smoothing filters of odd dimension,
 *          typically use 3, 5, 7, etc.  The smooth parameter
 *          for these is (size - 1)/2; i.e., 1, 2, 3, etc.
 *      (2) The fract parameter is typically taken in the range:
 *          0.2 < fract < 0.7
 */
PIX *
pixUnsharpMaskGray(PIX       *pixs,
		   l_int32    smooth,
                   l_float32  fract)
{
l_int32  w, h;
PIX     *pixc, *pixt, *pixd;

    PROCNAME("pixUnsharpMaskGray");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 8)
	return (PIX *)ERROR_PTR("pixs not 8 bpp", procName, NULL);

    if (fract <= 0.0) {
	L_WARNING("no fraction added back in", procName);
	return pixClone(pixs);
    }

    if ((pixc = pixBlockconvGray(pixs, NULL, smooth, smooth)) == NULL)
	return (PIX *)ERROR_PTR("pixc not made", procName, NULL);

	/* steps:
	 *    (1) edge image is pixs - pixc  (this is highpass part)
	 *    (2) multiply edge image by fract
	 *    (3) add fraction of edge to pixs
	 */
    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if ((pixt = pixInitAccumulate(w, h, 0x10000000)) == NULL)
	return (PIX *)ERROR_PTR("pixt not made", procName, NULL);
    pixAccumulate(pixt, pixs, ARITH_ADD);
    pixAccumulate(pixt, pixc, ARITH_SUBTRACT);
    pixMultConstAccumulate(pixt, fract, 0x10000000);
    pixAccumulate(pixt, pixs, ARITH_ADD);
    if ((pixd = pixFinalAccumulate(pixt, 0x10000000, 8)) == NULL)
	return (PIX *)ERROR_PTR("pixd not made", procName, NULL);

    pixDestroy(&pixc);
    pixDestroy(&pixt);

    return pixd;
}


/*-------------------------------------------------------------*
 *                    Half-edge by bandpass                    *
 *-------------------------------------------------------------*/
/*!
 *  pixHalfEdgeByBandpass()
 *
 *      Input:  pixs (8 bpp gray or 32 bpp rgb)
 *              sm1h, sm1v ("half-widths" of smoothing filter sm1)
 *              sm2h, sm2v ("half-widths" of smoothing filter sm2)
 *                      (require sm2 != sm1)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (1) We use symmetric smoothing filters of odd dimension,
 *          typically use 3, 5, 7, etc.  The smoothing parameters
 *          for these are 1, 2, 3, etc.  The filter size is related
 *          to the smoothing parameter by
 *               size = 2 * smoothing + 1
 *      (2) Because we take the difference of two lowpass filters,
 *          this is actually a bandpass filter.
 *      (3) We allow both filters to be anisotropic.
 *      (4) Consider either the h or v component of the 2 filters.
 *          Depending on whether sm1 > sm2 or sm2 > sm1, we get
 *          different halves of the smoothed gradients (or "edges").
 *          This difference of smoothed signals looks more like
 *          a second derivative of a transition, which we rectify
 *          by not allowing the signal to go below zero.  If sm1 < sm2,
 *          the sm2 transition is broader, so the difference between
 *          sm1 and sm2 signals is positive on the upper half of
 *          the transition.  Likewise, if sm1 > sm2, the sm1 - sm2
 *          signal difference is positive on the lower half of
 *          the transition.
 */
PIX  *
pixHalfEdgeByBandpass(PIX     *pixs,
	              l_int32  sm1h,
	              l_int32  sm1v,
	              l_int32  sm2h,
                      l_int32  sm2v)
{
l_int32  d;
PIX     *pixg, *pixacc, *pixc1, *pixc2;

    PROCNAME("pixHalfEdgeByBandpass");

    if (!pixs)
	return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (sm1h == sm2h && sm1v == sm2v)
	return (PIX *)ERROR_PTR("sm2 = sm1", procName, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
	return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", procName, NULL);
    if (d == 32)
        pixg = pixConvertRGBToLuminance(pixs);
    else   /* d == 8 */
        pixg = pixClone(pixs);

        /* Make a convolution accumulator and use it twice */
    if ((pixacc = pixBlockconvAccum(pixg)) == NULL)
	return (PIX *)ERROR_PTR("pixacc not made", procName, NULL);
    if ((pixc1 = pixBlockconvGray(pixg, pixacc, sm1h, sm1v)) == NULL)
	return (PIX *)ERROR_PTR("pixc1 not made", procName, NULL);
    if ((pixc2 = pixBlockconvGray(pixg, pixacc, sm2h, sm2v)) == NULL)
	return (PIX *)ERROR_PTR("pixc2 not made", procName, NULL);
    pixDestroy(&pixacc);

	/* Compute the half-edge using pixc1 - pixc2.  */
    pixSubtractGray(pixc1, pixc1, pixc2);

    pixDestroy(&pixg);
    pixDestroy(&pixc2);

    return pixc1;
}