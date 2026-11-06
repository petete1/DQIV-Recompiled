#include "main/menu/MenuDisplays.hpp"
#include "main/dss/Display.hpp"
#include "main/menu/MenuSystem.hpp"
#ifdef DQIV_NATIVE
#include "main/menu/MenuLayers.hpp"
#endif

MenuDisplays g_MenuDisplays;

THUMB void setupMenuDisplays()
{
    g_MenuDisplays.message_.setup(-1);
    g_MenuDisplays.yesNo_.setup(0x1d4);
    g_MenuDisplays.frames_.setup(0);
    g_MenuDisplays.face_.setup(0);
    g_MenuDisplays.icon_.setup(0);
    g_MenuDisplays.sprite_.setup(0);
    g_MenuDisplays.dummy_.setup(0);
    g_MenuDisplays.texts_[0].setup(0);
    g_MenuDisplays.texts_[1].setup(0x40);
    g_MenuDisplays.texts_[2].setup(0x80);
    g_MenuDisplays.texts_[3].setup(0xc0);
    g_MenuDisplays.windowFrame1_.setup(0);
    g_MenuDisplays.windowFrame2_.setup(0);
    g_MenuDisplays.arrow_.setup(0);
    g_MenuDisplays.hopping_.setup(0);
}

THUMB void disableMenuDisplays()
{
    g_MenuDisplays.message_.setEnable(0);
    g_MenuDisplays.yesNo_.setEnable(0);
    g_MenuDisplays.frames_.setEnable(0);
    g_MenuDisplays.face_.setEnable(0);
    g_MenuDisplays.icon_.setEnable(0);
    g_MenuDisplays.sprite_.setEnable(0);
    g_MenuDisplays.dummy_.setEnable(0);
    g_MenuDisplays.texts_[0].setEnable(0);
    g_MenuDisplays.texts_[1].setEnable(0);
    g_MenuDisplays.texts_[2].setEnable(0);
    g_MenuDisplays.texts_[3].setEnable(0);
    g_MenuDisplays.windowFrame1_.setEnable(0);
    g_MenuDisplays.windowFrame2_.setEnable(0);
    g_MenuDisplays.arrow_.setEnable(0);
    g_MenuDisplays.hopping_.setEnable(0);
}

// The text displays are the message window's lines and name, or the battle's
// monster plates: overlay menus draw the first with their window (on its
// layer), the plates stay on the DS with the battle scene.
static OamBuffer* textMain(int index, OamBuffer* overlay, OamBuffer* scene)
{
#ifdef DQIV_NATIVE
    const MessageWindow* window = &g_MenuDisplays.message_;
    MenuTextDisplay* text = &g_MenuDisplays.texts_[index];
    if (window->enable_ && window->state_ != 0) {
        if (window->namePlate_ == text) {
            return overlay;
        }
        for (int i = 0; i < 4; i++) {
            if (window->lines_[i] == text) {
                return overlay;
            }
        }
    }
#endif
    return scene;
}

THUMB void updateMenuDisplays()
{
    OamBuffer* main = getOamBufferAtY(0);
    OamBuffer* sub = getOamBufferAtY(192);
    // The battle scene's plates and damage numbers stay on the DS (the message window goes with its layer: textMain).
    OamBuffer* sceneMain = main;
    OamBuffer* sceneSub = sub;
#ifdef DQIV_NATIVE
    // Overlay menus: the sprites of both screens, every frame, for the overlay only.
    if (overlayMenusOn()) {
        main = overlayMenuOam(0);
        sub = overlayMenuOam(1);
    }
#endif
    g_MenuDisplays.message_.update(main, sub);
    g_MenuDisplays.yesNo_.update(main, sub);
    g_MenuDisplays.face_.update(main, sub);
    g_MenuDisplays.icon_.update(main, sub);
    g_MenuDisplays.sprite_.update(main, sub);
    g_MenuDisplays.windowFrame1_.update(main, sub);
    g_MenuDisplays.windowFrame2_.update(main, sub);
    g_MenuDisplays.arrow_.update(main, sub);
    g_MenuDisplays.dummy_.update(main, sub);
    g_MenuDisplays.texts_[0].update(textMain(0, main, sceneMain), textMain(0, sub, sceneSub));
    g_MenuDisplays.texts_[1].update(textMain(1, main, sceneMain), textMain(1, sub, sceneSub));
    g_MenuDisplays.texts_[2].update(textMain(2, main, sceneMain), textMain(2, sub, sceneSub));
    g_MenuDisplays.texts_[3].update(textMain(3, main, sceneMain), textMain(3, sub, sceneSub));
    g_MenuDisplays.frames_.update(main, sub);
    g_MenuDisplays.hopping_.update(sceneMain, sceneSub);
}

THUMB void executeMenuDisplays()
{
    OamBuffer* main = getOamBufferAtY(0);
    OamBuffer* sub = getOamBufferAtY(192);
    // The battle scene's plates and damage numbers stay on the DS (the message window goes with its layer: textMain).
    OamBuffer* sceneMain = main;
    OamBuffer* sceneSub = sub;
#ifdef DQIV_NATIVE
    if (overlayMenusOn()) {
        main = overlayMenuOam(0);
        sub = overlayMenuOam(1);
    }
#endif
    g_MenuDisplays.message_.execute(main, sub);
    g_MenuDisplays.yesNo_.execute(main, sub);
    g_MenuDisplays.face_.execute(main, sub);
    g_MenuDisplays.icon_.execute(main, sub);
    g_MenuDisplays.sprite_.execute(main, sub);
    g_MenuDisplays.windowFrame1_.execute(main, sub);
    g_MenuDisplays.windowFrame2_.execute(main, sub);
    g_MenuDisplays.arrow_.execute(main, sub);
    g_MenuDisplays.dummy_.execute(main, sub);
    g_MenuDisplays.texts_[0].execute(textMain(0, main, sceneMain), textMain(0, sub, sceneSub));
    g_MenuDisplays.texts_[1].execute(textMain(1, main, sceneMain), textMain(1, sub, sceneSub));
    g_MenuDisplays.texts_[2].execute(textMain(2, main, sceneMain), textMain(2, sub, sceneSub));
    g_MenuDisplays.texts_[3].execute(textMain(3, main, sceneMain), textMain(3, sub, sceneSub));
    g_MenuDisplays.frames_.execute(main, sub);
    g_MenuDisplays.hopping_.execute(sceneMain, sceneSub);
}

THUMB void drawMenuDisplays()
{
    OamBuffer* main = getOamBufferAtY(0);
    OamBuffer* sub = getOamBufferAtY(192);
    // The battle scene's plates and damage numbers stay on the DS (the message window goes with its layer: textMain).
    OamBuffer* sceneMain = main;
    OamBuffer* sceneSub = sub;
#ifdef DQIV_NATIVE
    if (overlayMenusOn()) {
        main = overlayMenuOam(0);
        sub = overlayMenuOam(1);
    }
#endif
    g_MenuDisplays.face_.draw(main, sub);
    g_MenuDisplays.icon_.draw(main, sub);
    g_MenuDisplays.sprite_.draw(main, sub);
    g_MenuDisplays.dummy_.draw(main, sub);
    g_MenuDisplays.message_.draw(main, sub);
    g_MenuDisplays.yesNo_.draw(main, sub);
    g_MenuDisplays.windowFrame1_.draw(main, sub);
    g_MenuDisplays.windowFrame2_.draw(main, sub);
    g_MenuDisplays.texts_[0].draw(textMain(0, main, sceneMain), textMain(0, sub, sceneSub));
    g_MenuDisplays.texts_[1].draw(textMain(1, main, sceneMain), textMain(1, sub, sceneSub));
    g_MenuDisplays.texts_[2].draw(textMain(2, main, sceneMain), textMain(2, sub, sceneSub));
    g_MenuDisplays.texts_[3].draw(textMain(3, main, sceneMain), textMain(3, sub, sceneSub));
    g_MenuDisplays.yesNo_.unkfunc_02052a24(main, sub);
    g_MenuDisplays.frames_.draw(main, sub);
    g_MenuDisplays.arrow_.draw(main, sub);
    g_MenuDisplays.hopping_.draw(sceneMain, sceneSub);
    moveMenuDisplays();
    disableMenuDisplays();
}

THUMB void moveMenuDisplays()
{
    g_MenuDisplays.face_.updateMove();
    g_MenuDisplays.icon_.updateMove();
    g_MenuDisplays.sprite_.updateMove();
    g_MenuDisplays.dummy_.updateMove();
    g_MenuDisplays.message_.updateMove();
    g_MenuDisplays.yesNo_.updateMove();
    g_MenuDisplays.windowFrame1_.updateMove();
    g_MenuDisplays.windowFrame2_.updateMove();
    g_MenuDisplays.texts_[0].updateMove();
    g_MenuDisplays.texts_[1].updateMove();
    g_MenuDisplays.texts_[2].updateMove();
    g_MenuDisplays.texts_[3].updateMove();
    g_MenuDisplays.frames_.updateMove();
    g_MenuDisplays.arrow_.updateMove();
    g_MenuDisplays.hopping_.updateMove();
}

THUMB void resetMenuDisplays()
{
    g_MenuDisplays.face_.reset();
    g_MenuDisplays.icon_.reset();
    g_MenuDisplays.sprite_.reset();
    g_MenuDisplays.dummy_.reset();
    g_MenuDisplays.frames_.reset();
    g_MenuDisplays.texts_[0].reset();
    g_MenuDisplays.texts_[1].reset();
    g_MenuDisplays.texts_[2].reset();
    g_MenuDisplays.texts_[3].reset();
    g_MenuDisplays.windowFrame1_.reset();
    g_MenuDisplays.windowFrame2_.reset();
    g_MenuDisplays.arrow_.reset();
    g_MenuDisplays.hopping_.reset();
}
