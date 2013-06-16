
/*#define WGL_FRONT_COLOR_BUFFER_BIT_ARB 1 */
#define WGL_BACK_COLOR_BUFFER_BIT_ARB 2
#define WGL_DEPTH_BUFFER_BIT_ARB 4
/*#define WGL_STENCIL_BUFFER_BIT_ARB 8 */

int wglBufferInitialize(void);
HANDLE CreateBufferRegion(unsigned int buffers);
void SaveBufferRegion(HANDLE region, int x, int y, int width, int height);
void RestoreBufferRegion(HANDLE region, int x, int y, int width, int height);
/*void DeleteBufferRegion(HANDLE region); */
