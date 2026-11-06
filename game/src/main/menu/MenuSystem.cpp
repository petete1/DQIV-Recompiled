#include "main/menu/MenuSystem.hpp"
#ifdef DQIV_NATIVE
#include "main/menu/MenuLayers.hpp"
#endif
#include "main/menu/MenuIconData.hpp"
#include "main/menu/MenuManager.hpp"

ARM void loadMenuSystemFiles()
{
    loadMenuIconFiles();
}

ARM void initMenuSystem()
{
    initMenuMsgSystem();
    MenuManager::setup();
}

ARM void startMenuSystem()
{
    setupMenuDisplays();
    initMenuBg();
}

ARM void endMenuSystem()
{
}

ARM void updateMenuSystem()
{
    updateMenuDisplays();
    updateMenuBg();
}

ARM void executeMenuSystem()
{
    MenuManager::execute();
    executeMenuDisplays();
    executeMenuBg();
}

ARM void drawMenuSystem()
{
    drawMenuDisplays();
    transferMenuBg();
#ifdef DQIV_NATIVE
    publishMenuLayers();
#endif
}
