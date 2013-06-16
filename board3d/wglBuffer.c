
#include "config.h"
#include "inc3d.h"

typedef HANDLE(WINAPI * fCreateRegion) (HDC dc, int layer, unsigned int type);
typedef BOOL(WINAPI * fSaveRegion) (HANDLE region, int x, int y, int width, int height);
typedef BOOL(WINAPI * fRestoreRegion) (HANDLE region, int x, int y, int width, int height, int xSrc, int ySrc);
typedef void (WINAPI * fDeleteRegion) (HANDLE region);

static fCreateRegion wglCreateBufferRegionARB;
static fSaveRegion wglSaveBufferRegionARB;
static fRestoreRegion wglRestoreBufferRegionARB;
static fDeleteRegion wglDeleteBufferRegionARB;

int
wglBufferInitialize()
{
    if (!extensionSupported("WGL_ARB_buffer_region"))
        return FALSE;

    /* Look up the 4 functions */
    wglCreateBufferRegionARB = (fCreateRegion) wglGetProcAddress("wglCreateBufferRegionARB");
    wglSaveBufferRegionARB = (fSaveRegion) wglGetProcAddress("wglSaveBufferRegionARB");
    wglRestoreBufferRegionARB = (fRestoreRegion) wglGetProcAddress("wglRestoreBufferRegionARB");
    wglDeleteBufferRegionARB = (fDeleteRegion) wglGetProcAddress("wglDeleteBufferRegionARB");

    return wglCreateBufferRegionARB && wglDeleteBufferRegionARB && wglSaveBufferRegionARB && wglRestoreBufferRegionARB;
}

#if 0                           /* Remove this until option added to control this */

HANDLE
CreateBufferRegion(unsigned int buffers)
{
    /* Create the buffer region. */
    HANDLE FBRegion = wglCreateBufferRegionARB(wglGetCurrentDC(), 0, buffers);

    if (FBRegion == 0)
        puts("wglCreateBufferRegionARB Failed");

    return FBRegion;
}
#endif
void
SaveBufferRegion(HANDLE region, int x, int y, int width, int height)
{
    if (wglSaveBufferRegionARB(region, x, y, width, height) == FALSE)
        puts("wglSaveBufferRegionARB Failed");
}

void
RestoreBufferRegion(HANDLE region, int x, int y, int width, int height)
{
    if (wglRestoreBufferRegionARB(region, x, y, width, height, 0, 0) == FALSE)
        puts("wglSaveBufferRegionARB Failed");
}

/* Should be called on exit ? 
 * void DeleteBufferRegion(HANDLE region)
 * {
 * wglDeleteBufferRegionARB(region);
 * }
 */
