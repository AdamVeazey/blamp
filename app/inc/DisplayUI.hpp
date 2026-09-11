#pragma once

#include <cstdint>
#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include "InputSource.hpp"

class DisplayUI {
public:
    enum class MenuState {
        Home,               // Knob turns volume, click toggles play/pause, long-press enters source selection
        SubmenuSelect,      // Knob scrolls submenus, click selects another menu, long-press goes back
            SourceSelect,   // knob scrolls InputSource, click selects a source, long-press goes back
            BTPairing,      // click to cancel
    };
    enum class Submenu {
        InputSourceSelection,
        BluetoothPairing,
    };
private:
    static const device* display;
    static constexpr uint32_t BLANK_TIMEOUT_MS = 10'000;
    uint32_t lastActivityTime = 0;

    // Home Screen
    lv_obj_t* screenHome = nullptr;
    // Home widgets
    lv_obj_t* labelHomeVolume = nullptr;
    lv_obj_t* labelHomePlaying = nullptr;
    lv_obj_t* labelHomeSource = nullptr;

    // Submenu Screen
    lv_obj_t* screenSubmenu = nullptr;
    // Submenu widgets
    lv_obj_t* listSubmenu = nullptr;
    lv_obj_t* btnSubmenuSource = nullptr;
    lv_obj_t* btnSubmenuBT = nullptr;

    // Input Source Slect Screen
    lv_obj_t* screenSourceSelect = nullptr;
    // Source Select widgets
    lv_obj_t* rollerSource = nullptr;

    // Bluetooth Pairing Screen
    lv_obj_t* screenBtPairing = nullptr;

    lv_group_t* groupEncoder = nullptr;
private:
    static void checkInactivityCb_C( lv_timer_t* timer ) { static_cast<DisplayUI*>(timer->user_data)->checkInactivityCb(timer); }
    void checkInactivityCb( lv_timer_t* timer );
    void applyOledStyle( lv_obj_t* obj );
    void applyMonochromeFocusStyle( lv_obj_t* btn );
    void applyMonochromeRollerStyle( lv_obj_t* roller );
    uint16_t inputSourceToRollerIndex( InputSource source );
    InputSource rollerIndexToInputSource( uint16_t index );
public:
    void init();
    void showHomeScreen( bool isPlayingAudio, uint8_t volume, InputSource source );
    void showSubmenuSelect();
    void showSourceSelect( InputSource source );
    void showBTPairing();

    void updateHomeAudioPlaying( bool isPlayingAudio );
    void updateHomeVolume( int32_t volume );
    void updateHomeInputSource( InputSource source );

    void scrollMenuDown();
    void scrollMenuUp();

    Submenu getSubmenuSelected();
    InputSource getInputSourceSelected();

    void resetInactivityTimer();
};