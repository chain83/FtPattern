// Ft1DHv1.03
#define PLUGIN "Ft1DH Forward FOURIER Transform"
#define AUTHOR "RONC"
#define VER     "1.03<>"

//********************************************************************************************************
//
//********************************************************************************************************
// this file should be same as one for forward transform but with these interchanged
// both contain same code but one executes forward and the other inverse
#define FORWARD_Transform  true   //FORWARD Ft1DH.8bf
#define INVERSE_Transform  false  //INVERSE iFt1DH.8bf
//********************************************************************************************************
//********************************************************************************************************

/*
 Ron Chambers, rechmbrs@gmail.com and Francesco Pierfederici, fpierfed@gmail.com  Ft3DF_iFt3DF
 
 Photoshop CC 2018 / VisualStudio 2017 Community / XCode 9.4.1
 
 November 2017 to August 2018
 */
//********************************************************************************************************
//********************************************************************************************************
//********************************************************************************************************

#ifdef _WIN32
#include <windows.h>
#include "..\Ft_lib\fftw3.h"
#include "C:\adobe_photoshop_sdk_cc_2017_win\pluginsdk\photoshopapi\photoshop\PIFilter.h"
#include "..\Ft_lib\Ft_lib.h"
#elif __APPLE__
#include "../Ft_lib/fftw3.h"
#include "../Ft_lib/Ft_lib.h"
#include <PIFilter.h>
#include <PIUtilities.h>
#include <DialogUtilities.h>
#endif
#include <math.h>
#include <string>
#include <thread>

#include "Ft1DH_resource.h"


#define c_re(c) ((c)[0])
#define c_im(c) ((c)[1])

#define filterSelectorAbout      0
#define filterSelectorParameters 1
#define filterSelectorPrepare     2
#define filterSelectorStart      3
#define filterSelectorContinue     4
#define filterSelectorFinish     5

#define userCanceledErr            (-128)
#define memoryCancelErr            (-64)

#define CONCAT2(a, b) a b
#define CONCAT(a, b) CONCAT2(a, b)
#define VERSION CONCAT(VER, __DATE__)

#define u_zLen 1024
wchar_t* uString = new wchar_t[u_zLen];
wchar_t* vString = new wchar_t[u_zLen];
wchar_t* wString = new wchar_t[u_zLen];
char xString[u_zLen];
char yString[u_zLen];
char zString[u_zLen];

int iERR = 0;

#ifdef __APPLE__
SPBasicSuite * sSPBasic = NULL;
#endif

void SetRectMy(Rect *rect, short top, short left, short right, short bottom)
{
    rect->top = top;
    rect->left = left;
    rect->right = right;
    rect->bottom = bottom;
}

#ifdef _WIN32
HANDLE hDllInstance = NULL;

// Entry point (dll initialization)
// Executed once when PhotoShop scans the "Plugins" folder

BOOL APIENTRY DLLEntry(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
        hDllInstance = hInstance;        // Process descriptor
    return 1;
}
#endif

//
// These are the dialog window parameters
//
short ErrorCode = 0;

bool Forward = FORWARD_Transform;//FORWARD Ft1DH.8bf
bool Inverse = INVERSE_Transform;//INVERSE iFt1DH.8bf

// Entry point, executed when user starting this plugin
// Executed several times (3 times minimum) with different "selector" values

#ifdef _WIN32
extern "C" __declspec(dllexport) void ENTRYPOINT(
                                                 const short        selector,  // TYPE of action required
                                                 FilterRecord    *fPB,       // Pointer to the information about image
                                                 intptr_t        *data,       // Never use this one, keep everything in registry
                                                 short           *result)   // Error code should be assigned to this variable  before return
{
    HWND HWnd = NULL;
    PlatformData *platform;
    
    platform = (PlatformData *)(fPB->platformData);
    
    if(platform != NULL)HWnd = (HWND)platform->hwnd;            // Descriptor of parent window
    
#elif __APPLE__
    DLLExport MACPASCAL void PluginMain(const int16        selector,  // TYPE of action required
                                        FilterRecordPtr    fPB,       // Pointer to the information about image
                                        intptr_t        *data,       // Never use this one, keep everything in registry
                                        int16            *result)   // Error code should be assigned to this variable  before return
    {
#endif
        int xSizeF = fPB->imageSize.h;      //Row Size           517     518    519    520        width true
        int xSize = fPB->imageSize.h - 2; //                          519    520    521    522         true + 2
        int xSizeH = (xSize / 2) + 1;            //                          259   260    260    261        (width / 2) + 1
        int xSizeHx2 = xSizeH * 2;              //                          518    520    520    522       ((width / 2) + 1) * 2
        int ySize = fPB->imageSize.v; //Col Size
        int zSize = 3;
        
        double dxSize = (double)(xSize);
        
        int bitDepth = fPB->depth;    //Bit Depth/Channel
        int nPlanes = fPB->planes;    //Number planes
        int nChans = nPlanes - 1;     //Number channels
        
        double PI = (double)3.141592653589793238462102433832795;
        double Rad2Deg = (double)180.0000000 / PI;
        
        if(selector == filterSelectorStart)        // Request pointers to data arrays
        {
            SetRectMy(&fPB->inRect, 0, 0, xSizeF, ySize);   // Input buffer (Whole)
            fPB->inLoPlane = 0;                               // First requested plane (RED)
            fPB->inHiPlane = (short)nChans;       // Last requested plane (Blue or transparency)
            
            SetRectMy(&fPB->outRect, 0, 0, xSizeF, ySize);  // Output buffer (Whole)
            fPB->outLoPlane = 0;
            fPB->outHiPlane = (short)nChans;
            
            SetRectMy(&fPB->maskRect, 0, 0, 0, 0);
        }
        
        if(selector == filterSelectorContinue)
        {
            //********************************************************************************************************
            //    }
            //  if(selector == filterSelectorContinue)
            //  {
            //********************************************************************************************************
            // Data initialization
            //********************************************************************************************************
            unsigned char* cIn = (unsigned char*)fPB->inData;
            unsigned short* sIn = (unsigned short*)fPB->inData;
            float* fIn = (float*)fPB->inData;
            long iRC = fPB->inRowBytes;
            long iRS = fPB->inRowBytes / 2;
            long iRF = fPB->inRowBytes / 4;
            
            unsigned char* cOut = (unsigned char*)fPB->outData;
            unsigned short* sOut = (unsigned short*)fPB->outData;
            float* fOut = (float*)fPB->outData;
            long oRC = fPB->outRowBytes;
            long oRS = fPB->outRowBytes / 2;
            long oRF = fPB->outRowBytes / 4;
            
            double dAmp, dPhz;
            double dRe, dIm;
            double dAmpMx, dAmpMxD2;
            double degMax = 360.0;
            double dLG;
            double dRnd;
            double dRound = 0.5;
            
            long iAmpMx;
            int i, j, k;
            
            fftw_plan p;
            fftw_complex *complex_io = (fftw_complex *)data;
            complex_io = (fftw_complex *)fftw_malloc((xSize + 4) * (ySize + 4) * (zSize + 1) * sizeof(fftw_complex) + 64);
            double *double_io;
            double_io = (double *)fftw_malloc((xSize + 4) * (ySize + 4) * (zSize + 1) * sizeof(double) + 64);
            
            // Init FFTW
            unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
            
            fftw_init_threads();
            fftw_plan_with_nthreads(concurentThreadsSupported);
            
            //******************************************************************************************************
            // 8 bit images
            //******************************************************************************************************
            if(bitDepth == 8)
            {
                //****************************************************
                // 8 bit Forward
                //****************************************************
                if(Forward == true)
                {
                    iAmpMx = 255;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    
                    p = fftw_plan_dft_r2c_1d(xSize, double_io, complex_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSize; i++)
                            {
                                double_io[i] = (double)cIn[iRC * j + nPlanes * i + k];
                            }
                            //****************************************************
                            // 8 bit Forward   compute forward transform / spectra
                            //****************************************************
                            fftw_execute(p);
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)c_re(complex_io[i]);
                                dIm = (double)c_im(complex_io[i]);
                                dAmp = (double)sqrt(dRe * dRe + dIm * dIm);
                                if(dAmp > 0.0)
                                {
                                    dAmp = (double)log10(dAmp) / dLG;
                                    dPhz = (double)atan2(dIm, dRe) * Rad2Deg * (dAmpMx / degMax) + dAmpMxD2;
                                }
                                dRnd = absRoundLimit(dAmp, dAmpMx, dRound);
                                cOut[oRC * j + nPlanes * i + k] = (unsigned char)dRnd;
                                
                                dRnd = absRoundLimit(dPhz, dAmpMx, dRound);
                                cOut[oRC * j + nPlanes * (i + xSizeH) + k] = (unsigned char)dRnd;
                            }
                            if(xSizeHx2 != xSizeF)
                            {
                                for(i = xSizeHx2; i < xSizeF + 1; i++)
                                {
                                    cOut[oRC * j + nPlanes * i + k] = (unsigned char)0.0;
                                }
                                //dRnd = 0.0;
                                //cOut[oRC * j + nPlanes * (i + xSizeF) + k] = (unsigned char)dRnd;
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
                //****************************************************
                // 8 bit Inverse
                //****************************************************
                if(Inverse == true)
                {
                    iAmpMx = 255;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    p = fftw_plan_dft_c2r_1d(xSize, complex_io, double_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)cIn[iRC * j + nPlanes * i + k];
                                if(dRe > 0.0)
                                {
                                    dIm = (double)cIn[iRC * j + nPlanes * (i + xSizeH) + k];
                                    
                                    dAmp = (double)pow(10, dRe * dLG);
                                    dPhz = (dIm - dAmpMxD2) / (Rad2Deg * (dAmpMx / degMax));
                                }
                                c_re(complex_io[i]) = dAmp * cos(dPhz);
                                c_im(complex_io[i]) = dAmp * sin(dPhz);
                            }
                            //****************************************************
                            // 8 bit Inverse    compute inverse transform
                            //****************************************************
                            fftw_execute(p);
                            for(i = 0; i < xSize; i++)
                            {
                                dRe = double_io[i] / dxSize;
                                dRnd = absRoundLimit(dRe, dAmpMx, dRound);
                                cOut[oRC * j + nPlanes * i + k] = (unsigned char)dRnd;
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
            }
            //********************************************************
            // 16 bit images
            //********************************************************
            if(bitDepth == 16)
            {
                //****************************************************
                // 16 bit Forward
                //****************************************************
                if(Forward == true)
                {
                    iAmpMx = 32768;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    p = fftw_plan_dft_r2c_1d(xSize, double_io, complex_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSize; i++)
                            {
                                double_io[i] = (double)sIn[iRS * j + nPlanes * i + k];
                            }
                            //****************************************************
                            // 16 bit Forward     compute forward transform / spectra
                            //****************************************************
                            fftw_execute(p);
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)c_re(complex_io[i]);
                                dIm = (double)c_im(complex_io[i]);
                                dAmp = (double)sqrt(dRe * dRe + dIm * dIm);
                                if(dAmp > 0.0)
                                {
                                    dAmp = (double)log10(dAmp) / dLG;
                                    dPhz = (double)atan2(dIm, dRe) * Rad2Deg * (dAmpMx / degMax) + dAmpMxD2;
                                }
                                dRnd = absRoundLimit(dAmp, dAmpMx, dRound);
                                sOut[oRS * j + nPlanes * i + k] = (unsigned short)dRnd;
                                
                                dRnd = absRoundLimit(dPhz, dAmpMx, dRound);
                                sOut[oRS * j + nPlanes * (i + xSizeH) + k] = (unsigned short)dRnd;
                            }
                            if(xSizeHx2 != xSizeF)
                            {
                                for(i = xSizeHx2; i < xSizeF + 1; i++)
                                {
                                    sOut[oRS * j + nPlanes * i + k] = (unsigned short)0.0;
                                }
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
                //****************************************************
                // 16 bit Inverse
                //****************************************************
                if(Inverse == true)
                {
                    iAmpMx = 32768;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    p = fftw_plan_dft_c2r_1d(xSize, complex_io, double_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)sIn[iRS * j + nPlanes * i + k];
                                if(dRe > 0.0)
                                {
                                    dIm = (double)sIn[iRS * j + nPlanes * (i + xSizeH) + k];
                                    
                                    dAmp = (double)pow(10, dRe * dLG);
                                    dPhz = (dIm - dAmpMxD2) / (Rad2Deg * (dAmpMx / degMax));
                                }
                                c_re(complex_io[i]) = dAmp * cos(dPhz);
                                c_im(complex_io[i]) = dAmp * sin(dPhz);
                            }
                            //****************************************************
                            // 16 bit Inverse    compute inverse transform
                            //****************************************************
                            fftw_execute(p);
                            for(i = 0; i < xSize; i++)
                            {
                                dRe = double_io[i] / dxSize;
                                dRnd = absRoundLimit(dRe, dAmpMx, dRound);
                                sOut[oRS * j + nPlanes * i + k] = (unsigned short)dRnd;
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
            }
            //********************************************************************************************************
            // 32 bit images
            //********************************************************************************************************
            if(bitDepth == 32)
            {
                //****************************************************
                // 32 bit Forward
                //****************************************************
                if(Forward == true)
                {
                    iAmpMx = 1;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    p = fftw_plan_dft_r2c_1d(xSize, double_io, complex_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSize; i++)
                            {
                                double_io[i] = (double)fIn[iRF * j + nPlanes * i + k];
                                //    dSum = (double)fIn[iRF * j + nPlanes * i + k] + 0.00001 ;
                                //    double_io[i] = dSum;
                                //double_io[i] = (double)pow(dSum, dUnGamma) * (double)dUnExpose;
                            }
                            //****************************************************
                            // 32 bit Forward     compute forward transform / spectra
                            //****************************************************
                            fftw_execute(p);
                            //****************************************************
                            // range of numbers is - to + dAmpMx
                            // Photoshop only likes 0 and positive numbers
                            //  cIn, sIn, fIn, cOut, sOut, and fOut arrays
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)c_re(complex_io[i]);
                                dIm = (double)c_im(complex_io[i]);
                                dAmp = (double)sqrt(dRe * dRe + dIm * dIm);
                                if(dAmp > 0.0)
                                {
                                    dAmp = (double)log10(dAmp) / dLG;
                                    dPhz = (double)atan2(dIm, dRe) * Rad2Deg * (dAmpMx / degMax) + dAmpMxD2;
                                }
                                fOut[oRF * j + nPlanes * i + k] = (float)dabs(dAmp);
                                fOut[oRF * j + nPlanes * (i + xSizeH) + k] = (float)dabs(dPhz);
                            }
                            if(xSizeHx2 != xSizeF)
                            {
                                for(i = xSizeHx2; i < xSizeF + 1; i++)
                                {
                                    fOut[oRF * j + nPlanes * i + k] = (float)0.0;
                                }
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
                //****************************************************
                // 32 bit Inverse
                //****************************************************
                if(Inverse == true)
                {
                    iAmpMx = 1;
                    dAmpMx = (double)iAmpMx;
                    dAmpMxD2 = dAmpMx / (double)2.0;
                    dLG = log10(dAmpMx * dxSize) / dAmpMx;
                    p = fftw_plan_dft_c2r_1d(xSize, complex_io, double_io, FFTW_ESTIMATE);
                    for(k = 0; k < zSize; k++)
                    {
                        for(j = 0; j < ySize; j++)
                        {
                            for(i = 0; i < xSizeH; i++)
                            {
                                dAmp = 0.0;
                                dPhz = 0.0;
                                dRe = (double)fIn[iRF * j + nPlanes * i + k];
                                if(dRe > 0.0)
                                {
                                    dIm = (double)fIn[iRF * j + nPlanes * (i + xSizeH) + k];
                                    dAmp = (double)pow(10, dRe * dLG);
                                    dPhz = (dIm - dAmpMxD2) / (Rad2Deg * (dAmpMx / degMax));
                                }
                                c_re(complex_io[i]) = dAmp * cos(dPhz);
                                c_im(complex_io[i]) = dAmp * sin(dPhz);
                            }
                            //****************************************************
                            // 32 bit Inverse    compute inverse transform
                            //****************************************************
                            fftw_execute(p);
                            for(i = 0; i < xSize; i++)
                            {
                                dRe = dabs(double_io[i] / dxSize);
                                //dRe = (double)pow((dRe * (double)dExpose), (double)dGamma);
                                fOut[oRF * j + nPlanes * i + k] = (float)dRe;
                            }
                        }
                    }
                    fftw_destroy_plan(p);
                }
            }
            //********************************************************************************************************
            // Clean up for errors or good run
            //********************************************************************************************************
            fftw_free(complex_io);
            fftw_free(double_io);
            // The end of processing
            SetRectMy(&fPB->inRect, 0, 0, 0, 0); // Do not forget to clear data request (infinite loop)
            SetRectMy(&fPB->outRect, 0, 0, 0, 0);
            SetRectMy(&fPB->maskRect, 0, 0, 0, 0);
        } //if(selector == filterSelectorContinue)
        *result = 0;    // No Errors
        return;
    }
    //********************************************************************************************************
    // End
    //********************************************************************************************************
