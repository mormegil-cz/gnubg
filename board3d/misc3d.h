extern float getDiceSize(const renderdata* prd);
extern void SetupFlag(void);
extern void setupDicePaths(const BoardData* bd, Path dicePaths[2], float diceMovingPos[2][3], DiceRotation diceRotation[2]);
extern void waveFlag(float wag);
extern GdkGLConfig *getGlConfig(void);

/* Helper functions in misc3d */
void cylinder(float radius, float height, unsigned int accuracy, const Texture* texture);
void circle(float radius, float height, unsigned int accuracy);
void circleRev(float radius, float height, unsigned int accuracy);
void circleTex(float radius, float height, unsigned int accuracy, const Texture* texture);
void circleRevTex(float radius, float height, unsigned int accuracy, const Texture* texture);
void circleOutlineOutward(float radius, float height, unsigned int accuracy);
void circleOutline(float radius, float height, unsigned int accuracy);
void drawBox(int boxType, float x, float y, float z, float w, float h, float d, const Texture* texture);
void drawCube(float size);
void drawRect(float x, float y, float z, float w, float h, const Texture* texture);
void drawSplitRect(float x, float y, float z, float w, float h, const Texture* texture);
void drawChequeredRect(float x, float y, float z, float w, float h, int across, int down, const Texture* texture);
void QuarterCylinder(float radius, float len, unsigned int accuracy, const Texture* texture);
void QuarterCylinderSplayed(float radius, float len, unsigned int accuracy, const Texture* texture);
void QuarterCylinderSplayedRev(float radius, float len, unsigned int accuracy, const Texture* texture);
void drawCornerEigth(float ** const *boardPoints, float radius, unsigned int accuracy);
void calculateEigthPoints(float ****boardPoints, float radius, unsigned int accuracy);
void drawBoardTop(const BoardData *bd, BoardData3d *bd3d, const renderdata *prd);
void drawBasePreRender(const BoardData *bd, BoardData3d *bd3d, const renderdata *prd);

/* Other functions */
void initPath(Path* p, const float start[3]);
void addPathSegment(Path* p, PathType type, const float point[3]);
void initDT(diceTest* dt, int x, int y, int z);
