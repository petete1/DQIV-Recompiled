#include "ov028/MaterielMenu_SHOP/MaterielMenu_SHOP.hpp"
#include "main/menu/TownMenu_MESSAGE.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/PlayerStatus.hpp"
#include "main/status/UseItem.hpp"
#include "ov016/MenuTemplate/MenuTemplate_materiel.hpp"
#include "ov016/UnkMaterielMenuDraw/MaterielMenuScreens.hpp"
#include "main/menu/MenuTemplate_Common.hpp"
#ifdef DQIV_NATIVE
#include "main/dss/Pad.hpp"
#include "main/menu/MenuDataCommon.hpp"
#include "ov015/btl/BattleMenuNative.hpp"
#include <nitro/pad.h>
namespace {
// The Items window under the shop's Sell command.
void beginSellWindow()
{
    int command[3];
    menu::MenuDataCommon::getShopCommand(command, false);
    beginItemsWindow();
    setItemsWindowTitle(command[1]);
}
}
#endif

static inline int getItem(status::HaveItem* haveItem, int index)
{
    return haveItem->getItem(index);
}

THUMB void MaterielMenu_SHOP_SELL_ITEM::menuSetup()
{
    status::g_Party.setPlayerMode();
    menuItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    menuItem3_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH, menu::MenuItem::CURSORTYPE_NONE);
    menuItem2_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_CANCEL, menu::MenuItem::CURSORTYPE_NONE);
    activeChara_ = MaterielMenuPlayerControl::getSingleton()->activeChara_;
    pageStart_ = 0;
    int i = 0;
    status::HaveItem& haveItem = status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_.haveItem_;
    while (haveItem.isEquipment(i) == true) {
        i++;
        if (i > haveItem.getCount()) {
            break;
        }
    }
    itemIndex_ = i;
    if (i == haveItem.getCount()) {
        itemIndex_ = 0;
    }
    int itemIndex = itemIndex_;
    MaterielMenuPlayerControl::getSingleton()->activeItem_ = itemIndex;
    MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = 0;
    navigator_.setupBase();
}

THUMB void MaterielMenu_SHOP_SELL_ITEM::menuExecute()
{
    status::HaveItem& haveItem = status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_.haveItem_;
    int itemCount;
    if (pageStart_ == 0 && haveItem.getCount() > 6) {
        itemCount = 6;
    } else if (pageStart_ == 1) {
        itemCount = haveItem.getCount() - 6;
    } else {
        itemCount = haveItem.getCount();
    }
    MenuTemplate_Common::COMMON_ITEM_ICON32_2x3(&menuItem_, itemCount, itemIndex_);
    MenuTemplate_materiel::MATERIEL_CANCEL(&menuItem2_);
    if (haveItem.getCount() > 6) {
        MenuTemplate_materiel::shopPlayerArrow(&menuItem3_);
    }
}

THUMB void MaterielMenu_SHOP_SELL_ITEM::menuDraw()
{
    if (g_TownMenuMessage.isOpen()) {
        drawShopScreen();
        return;
    }
#ifdef DQIV_NATIVE
    if (classicBattleMenus()) {
        beginSellWindow();
        storeItemsWhose(activeChara_);
        beginFieldMenu(pc::BattleMenuKind::items, 1);
        addSellItems(activeChara_, false, pageStart_, itemIndex_);
        endItemsWindow(pc::BattleItemsFocus::list, &status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_);
        return;
    }
#endif
    drawShopSellItemScreen();
    menuItem3_.drawActive();
    menuItem_.drawActive();
}

THUMB void MaterielMenu_SHOP_SELL_ITEM::menuUpdate()
{
    if (g_TownMenuMessage.isOpen()) {
        if ((unsigned int)(g_TownMenuMessage.stat_ - 1) <= 1) {
            g_TownMenuMessage.close();
        }
        return;
    }
    if (MenuUpdate_Assist::isCancel(menuItem2_)) {
        close();
        gMaterielMenu_SHOP_WHO_SELL.open();
    }
    navigator_.setup(2, 3, status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_.haveItem_.getCount());
#ifdef DQIV_NATIVE
    // Classic: one column; Left / Right turn the page.
    if (classicBattleMenus()) {
        const int pages = (status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_.haveItem_.getCount() + 5) / 6;
        if (pages > 1 && (dss::g_Pad.edge() & (PAD_KEY_LEFT | PAD_KEY_RIGHT))) {
            pageStart_ = (pageStart_ + 1) % pages;
            navigator_.setPageNo(pageStart_);
            itemIndex_ = menuItem_.active_ = 0;
            MaterielMenuPlayerControl::getSingleton()->activeItem_ = 0;
            MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart_;
            redraw_ = 1;
            return;
        }
    }
    int result = classicBattleMenus() ? battleMenuSelect(menuItem_, navigator_) : MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#else
    int result = MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#endif
    if (result != 0) {
        if (result == 2) {
            int itemID = getItem(&status::g_Party.getPlayerStatus(activeChara_)->haveStatusInfo_.haveItem_, itemIndex_ + pageStart_ * 6);
            MaterielMenu_SHOP_MANAGER::getSingleton()->setSellQuantity(1);
            close();
            gMaterielMenu_SHOP_VALUE.sellType_ = status::UseItem::getSellType(itemID);
            gMaterielMenu_SHOP_VALUE.open();
            return;
        }
        itemIndex_ = menuItem_.active_;
        pageStart_ = navigator_.getPageNo();
        int itemIndex = itemIndex_;
        MaterielMenuPlayerControl::getSingleton()->activeItem_ = itemIndex;
        int pageStart = pageStart_;
        MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart;
        redraw_ = 1;
        return;
    }
    int active = menuItem_.active_;
    if (MenuUpdate_Assist::isPageFlip(menuItem3_, navigator_, active)) {
        itemIndex_ = menuItem_.active_ = active;
        pageStart_ = navigator_.getPageNo();
        int itemIndex = itemIndex_;
        MaterielMenuPlayerControl::getSingleton()->activeItem_ = itemIndex;
        int pageStart = pageStart_;
        MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart;
        redraw_ = 1;
    }
}
