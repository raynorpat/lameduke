/* The LameDuke side of build.c's bstub.h hooks. */

#include "bstub.h"
#include "editor.h"

void editinput(void);

void ExtInit(void)
{
    platform_init();
    initengine();
    loadlookup();
}
void ExtLoadNames(char names[][17]) { (void)names; }
void ExtUnInit(void) {}
void ExtLoadMap(const char *mapname) { (void)mapname; }
void ExtSaveMap(const char *mapname) { (void)mapname; }
const char *ExtGetSectorCaption(short sectnum) { (void)sectnum; return ""; }
const char *ExtGetWallCaption(short wallnum) { (void)wallnum; return ""; }
const char *ExtGetSpriteCaption(short spritenum) { (void)spritenum; return ""; }
void ExtShowSectorData(short sectnum) { (void)sectnum; }
void ExtShowWallData(short wallnum) { (void)wallnum; }
void ExtShowSpriteData(short spritenum) { (void)spritenum; }
void ExtEditSectorData(short sectnum) { (void)sectnum; }
void ExtEditWallData(short wallnum) { (void)wallnum; }
void ExtEditSpriteData(short spritenum) { (void)spritenum; }
void ExtPreCheckKeys(void) {}
void ExtAnalyzeSprites(void) {}
/* Once a frame in both modes; build.c's 3D keys are editinput. */
void ExtCheckKeys(void)
{
    if (!ydim16)
        editinput();
}
