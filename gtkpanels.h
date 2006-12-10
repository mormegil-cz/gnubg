#ifndef _GTKPANELS_H_
#define _GTKPANELS_H_

/* position of windows: main window, game list, and annotation */

typedef enum _gnubgwindow {
  WINDOW_MAIN = 0,
  WINDOW_GAME,
  WINDOW_ANALYSIS,
  WINDOW_ANNOTATION,
  WINDOW_HINT,
  WINDOW_MESSAGE,
  WINDOW_COMMAND,
  WINDOW_THEORY,
  NUM_WINDOWS
} gnubgwindow;

typedef struct _windowgeometry {
  int nWidth, nHeight;
  int nPosX, nPosY, max;
} windowgeometry;

extern void SaveWindowSettings(FILE* pf);
extern void HidePanel(gnubgwindow window);
extern void getWindowGeometry(gnubgwindow window);
extern int PanelShowing(gnubgwindow window);
extern void ClosePanels();

extern int GetPanelSize();
extern void SetPanelWidth(int size);

#endif
