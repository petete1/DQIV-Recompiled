#include "ov028/MaterielMenu_SHOP/MaterielMenu_SHOP.hpp"
#include "main/menu/TownMenu_MESSAGE.hpp"
#include "main/status/PartyStatus.hpp"
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

THUMB void MaterielMenu_SHOP_SELL_SACK::menuSetup()
{
    status::g_Party.setPlayerMode();
    menuItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    menuItem3_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH, menu::MenuItem::CURSORTYPE_NONE);
    menuItem2_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_CANCEL, menu::MenuItem::CURSORTYPE_NONE);
    navigator_.setupBase();
    itemIndex_ = 0;
    pageStart_ = 0;
    MaterielMenuPlayerControl::getSingleton()->activeItem_ = 0;
    MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = 0;
    if (status::g_Party.haveItemSack_.getCount() > 6) {
        itemCount_ = 6;
    } else {
        itemCount_ = status::g_Party.haveItemSack_.getCount();
    }
}

THUMB void MaterielMenu_SHOP_SELL_SACK::menuExecute()
{
    if (status::g_Party.haveItemSack_.getCount() > 6) {
        MenuTemplate_materiel::shopSackArrow(&menuItem3_, menuItem3_.active_);
    }
    MenuTemplate_Common::COMMON_ITEM_ICON32_2x3(&menuItem_, itemCount_, itemIndex_);
    MenuTemplate_materiel::MATERIEL_CANCEL(&menuItem2_);
}

THUMB void MaterielMenu_SHOP_SELL_SACK::menuDraw()
{
    if (g_TownMenuMessage.isOpen()) {
        drawShopScreen();
        return;
    }
#ifdef DQIV_NATIVE
    if (classicBattleMenus()) {
        beginSellWindow();
        storeItemsWhose(status::g_Party.getCount());
        beginFieldMenu(pc::BattleMenuKind::items, 1);
        addSellItems(0, true, pageStart_, itemIndex_);
        endItemsWindow(pc::BattleItemsFocus::list, NULL);
        return;
    }
#endif
    drawShopSellSackScreen();
    menuItem_.drawActive();
}

THUMB void MaterielMenu_SHOP_SELL_SACK::menuUpdate()
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
    navigator_.setup(2, 3, status::g_Party.haveItemSack_.getCount());
#ifdef DQIV_NATIVE
    // Classic: one column; Left / Right turn the page.
    if (classicBattleMenus()) {
        const int pages = (status::g_Party.haveItemSack_.getCount() + 5) / 6;
        const int edge = dss::g_Pad.edge();
        if (pages > 1 && (edge & (PAD_KEY_LEFT | PAD_KEY_RIGHT))) {
            pageStart_ = (pageStart_ + (edge & PAD_KEY_RIGHT ? 1 : pages - 1)) % pages;
            navigator_.setPageNo(pageStart_);
            itemIndex_ = menuItem_.active_ = 0;
            MaterielMenuPlayerControl::getSingleton()->activeItem_ = 0;
            MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart_;
            changeItem();
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
            selectItem();
        }
        pageStart_ = navigator_.getPageNo();
        int itemIndex = menuItem_.active_;
        itemIndex_ = itemIndex;
        MaterielMenuPlayerControl::getSingleton()->activeItem_ = itemIndex;
        int pageStart = pageStart_;
        MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart;
        changeItem();
        redraw_ = 1;
    }
    int active = menuItem_.active_;
    if (MenuUpdate_Assist::isPageFlip(menuItem3_, navigator_, active)) {
        itemIndex_ = menuItem_.active_ = active;
        pageStart_ = navigator_.getPageNo();
        int itemIndex = itemIndex_;
        MaterielMenuPlayerControl::getSingleton()->activeItem_ = itemIndex;
        int pageStart = pageStart_;
        MaterielMenuPlayerControl::getSingleton()->activeItemPage_ = pageStart;
        changeItem();
        redraw_ = 1;
    }
}

THUMB void MaterielMenu_SHOP_SELL_SACK::selectItem()
{
    int itemID = status::g_Party.haveItemSack_.getItem(itemIndex_ + pageStart_ * 6);
    close();
    if (status::UseItem::getSellType(itemID) == 1) {
        gMaterielMenu_SHOP_VALUE.sellType_ = 1;
        gMaterielMenu_SHOP_VALUE.open();
        return;
    }
    if (status::g_Party.haveItemSack_.getItemCount(itemIndex_ + pageStart_ * 6) == 1) {
        MaterielMenu_SHOP_MANAGER::getSingleton()->setSellQuantity(1);
        gMaterielMenu_SHOP_VALUE.sellType_ = status::UseItem::getSellType(itemID);
        gMaterielMenu_SHOP_VALUE.open();
        return;
    }
    gMaterielMenu_SHOP_SELL_QUANTITY.open();
}

THUMB void MaterielMenu_SHOP_SELL_SACK::changeItem()
{
    if (status::g_Party.haveItemSack_.getCount() < 6) {
        itemCount_ = status::g_Party.haveItemSack_.getCount();
    } else if ((pageStart_ + 1) * 6 > status::g_Party.haveItemSack_.getCount()) {
        itemCount_ = status::g_Party.haveItemSack_.getCount() - pageStart_ * 6;
    } else {
        itemCount_ = 6;
    }
    if (itemIndex_ > itemCount_ - 1) {
        itemIndex_ = itemCount_ - 1;
    }
}
