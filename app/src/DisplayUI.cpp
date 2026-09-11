#include "DisplayUI.hpp"

const device* DisplayUI::display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

// LVGL timer callback running every second to check for timeout
void DisplayUI::
checkInactivityCb( lv_timer_t * timer ) {
    if( lastActivityTime > 0 && (lv_tick_get() - lastActivityTime > BLANK_TIMEOUT_MS) ) {
        display_blanking_on( display );
        lastActivityTime = 0; // Mark as blanked
    }
}

void DisplayUI::
applyOledStyle( lv_obj_t* obj ) {
    lv_obj_set_style_bg_color( obj, lv_color_white(), 0 );
    lv_obj_set_style_bg_opa( obj, LV_OPA_COVER, 0 );
}

void DisplayUI::
applyMonochromeFocusStyle( lv_obj_t* btn ) {
    // Normal (Unfocused) state: Off-background, On-text
    lv_obj_set_style_bg_color( btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa( btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_text_color( btn, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT );

    // Focused state (When selected by encoder): Inverted colors!
    lv_obj_set_style_bg_color( btn, lv_color_black(), LV_PART_MAIN | LV_STATE_FOCUSED );
    lv_obj_set_style_bg_opa( btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED );
    lv_obj_set_style_text_color( btn, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED );

    // Remove default thin focus outline that distorts text on 64px displays
    lv_obj_set_style_outline_width( btn, 0, LV_STATE_FOCUSED );
}

void DisplayUI::
applyMonochromeRollerStyle( lv_obj_t* roller ) {
    // Unselected items (top and bottom rows)
    lv_obj_set_style_bg_color(roller, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_MAIN);

    // Selected center item: Inverted background & text
    lv_obj_set_style_bg_color(roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);

    // Remove default border frame around roller
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
}

uint16_t DisplayUI::
inputSourceToRollerIndex( InputSource source ) {
    switch( source ) {
    case InputSource::AUX_IN:           return 0;
    case InputSource::RCA_DIGITAL_COAX: return 1;
    case InputSource::OPTICAL:          return 2;
    case InputSource::HDMI_ARC:         return 3;
    case InputSource::USB:              return 4;
    case InputSource::BLUETOOTH:        return 5;
    default:                            return 0;
    }
}

InputSource DisplayUI::
rollerIndexToInputSource( uint16_t index ) {
    switch( index ) {
    case 0:     return InputSource::AUX_IN;
    case 1:     return InputSource::RCA_DIGITAL_COAX;
    case 2:     return InputSource::OPTICAL;
    case 3:     return InputSource::HDMI_ARC;
    case 4:     return InputSource::USB;
    case 5:     return InputSource::BLUETOOTH;
    default:    return InputSource::UNKNOWN;
    }
}

void DisplayUI::
init() {
    if( device_is_ready( display ) ) {
        display_blanking_off( display );
    }

    lastActivityTime = lv_tick_get();

    // Create an LVGL background timer to monitor inactivity
    lv_timer_create( checkInactivityCb_C, 1000, this );

    screenHome = lv_obj_create( nullptr );
    screenSubmenu = lv_obj_create( nullptr );
    screenSourceSelect = lv_obj_create( nullptr );
    screenBtPairing = lv_obj_create( nullptr );
    applyOledStyle( screenHome );
    applyOledStyle( screenSubmenu );
    applyOledStyle( screenSourceSelect );
    applyOledStyle( screenBtPairing );

    groupEncoder = lv_group_create();
}

void DisplayUI::
showHomeScreen( bool isPlayingAudio, uint8_t volume, InputSource source ) {
    if( !labelHomeVolume ) {
        labelHomeVolume = lv_label_create( screenHome );
        lv_obj_set_width( labelHomeVolume, 128 );
        lv_obj_set_style_text_color( labelHomeVolume, lv_color_black(), 0 );
        lv_obj_set_style_text_align( labelHomeVolume, LV_TEXT_ALIGN_CENTER, 0 );
        lv_obj_align( labelHomeVolume, LV_ALIGN_TOP_MID, 0, 4 );
    }
    updateHomeVolume( volume );

    if( !labelHomePlaying ) {
        labelHomePlaying = lv_label_create( screenHome );
        lv_obj_set_width( labelHomePlaying, 128 );
        lv_obj_set_style_text_color( labelHomePlaying, lv_color_black(), 0 );
        lv_obj_set_style_text_align( labelHomePlaying, LV_TEXT_ALIGN_CENTER, 0 );
        lv_obj_align( labelHomePlaying, LV_ALIGN_TOP_MID, 0, 20 );
    }
    updateHomeAudioPlaying( isPlayingAudio );

    if( !labelHomeSource ) {
        labelHomeSource = lv_label_create( screenHome );
        lv_obj_set_width( labelHomeSource, 128 );
        lv_obj_set_style_text_color( labelHomeSource, lv_color_black(), 0 );
        lv_obj_set_style_text_align( labelHomeSource, LV_TEXT_ALIGN_CENTER, 0 );
        lv_obj_align( labelHomeSource, LV_ALIGN_TOP_MID, 0, 36 );
    }
    updateHomeInputSource( source );

    lv_scr_load( screenHome );
}

void DisplayUI::
showSubmenuSelect() {
    // Create an LVGL list container
    if( !listSubmenu ) {
        listSubmenu = lv_list_create( screenSubmenu );
        lv_obj_set_size( listSubmenu, 128, 64 );
        lv_obj_align( listSubmenu, LV_ALIGN_CENTER, 0, 16 );
        btnSubmenuSource = lv_list_add_btn( listSubmenu, nullptr, "Input Source" );
        btnSubmenuBT = lv_list_add_btn( listSubmenu, nullptr, "BT Pairing" );
        applyMonochromeFocusStyle( btnSubmenuSource );
        applyMonochromeFocusStyle( btnSubmenuBT );
    }

    // Register buttons to encoder group for focus highlighting
    lv_group_remove_all_objs( groupEncoder );
    lv_group_add_obj( groupEncoder, btnSubmenuSource );
    lv_group_add_obj( groupEncoder, btnSubmenuBT );

    // Load screen
    lv_scr_load( screenSubmenu );
}

void DisplayUI::
showSourceSelect( InputSource source ) {
    if( !rollerSource ) {
        rollerSource = lv_roller_create( screenSourceSelect );

        // Set list options separated by newlines
        lv_roller_set_options( rollerSource,
            "AUX In\n"
            "Coaxial\n"
            "Optical\n"
            "HDMI ARC\n"
            "USB Audio\n"
            "Bluetooth",
            LV_ROLLER_MODE_NORMAL
        );

        // Fit 3 visible rows on the 64px display (Selected item in middle)
        lv_roller_set_visible_row_count( rollerSource, 5 );
        lv_obj_set_width( rollerSource, 120 );
        lv_obj_align( rollerSource, LV_ALIGN_CENTER, 0, 0 );

        applyMonochromeRollerStyle( rollerSource );
    }

    // Pre-select whatever input is currently active
    uint16_t activeIdx = inputSourceToRollerIndex( source );
    lv_roller_set_selected( rollerSource, activeIdx, LV_ANIM_OFF );

    // Register to encoder group
    lv_group_remove_all_objs( groupEncoder );
    lv_group_add_obj( groupEncoder, rollerSource );

    lv_scr_load( screenSourceSelect );
}

void DisplayUI::
showBTPairing() {

}

void DisplayUI::
updateHomeAudioPlaying( bool isPlayingAudio ) {
    if( labelHomePlaying ) {
        lv_label_set_text( labelHomePlaying, isPlayingAudio ? "Playing" : "Paused" );
    }
}

void DisplayUI::
updateHomeVolume( int32_t volume ) {
    if( labelHomeVolume ) {
        lv_label_set_text_fmt( labelHomeVolume, "Volume: %d%%", volume );
    }
}

void DisplayUI::
updateHomeInputSource( InputSource source ) {
    if( labelHomeSource ) {
        lv_label_set_text_fmt( labelHomeSource, "Source: %s", toString(source) );
    }
}

void DisplayUI::
scrollMenuDown() {
    if( !groupEncoder ) return;

    lv_obj_t* focused = lv_group_get_focused(groupEncoder);

    if( focused && (focused == rollerSource) ) {
        // Increment roller index
        uint16_t sel = lv_roller_get_selected( rollerSource );
        uint16_t count = lv_roller_get_option_cnt( rollerSource );
        if( sel + 1 < count ) {
            lv_roller_set_selected( rollerSource, sel + 1, LV_ANIM_ON );
        }
    }
    else {
        // Move focus down list items
        lv_group_focus_next( groupEncoder );
    }
}

void DisplayUI::
scrollMenuUp() {
    if( !groupEncoder ) return;

    lv_obj_t* focused = lv_group_get_focused( groupEncoder );

    if( focused && (focused == rollerSource) ) {
        // Decrement roller index
        uint16_t sel = lv_roller_get_selected( rollerSource );
        if( sel > 0 ) {
            lv_roller_set_selected( rollerSource, sel - 1, LV_ANIM_ON );
        }
    }
    else {
        // Move focus up list items
        lv_group_focus_prev( groupEncoder );
    }
}

DisplayUI::Submenu DisplayUI::
getSubmenuSelected() {
    lv_obj_t* focused = lv_group_get_focused( groupEncoder );
    if( focused == btnSubmenuBT ) {
        return Submenu::BluetoothPairing;
    }
    return Submenu::InputSourceSelection;
}

InputSource DisplayUI::
getInputSourceSelected() {
    if( rollerSource ) {
        uint16_t sel = lv_roller_get_selected( rollerSource );
        return rollerIndexToInputSource( sel );
    }
    return InputSource::UNKNOWN;
}

void DisplayUI::
resetInactivityTimer() {
    if (lastActivityTime == 0) {
        // Screen was blanked, wake it up!
        display_blanking_off(display);
    }
    lastActivityTime = lv_tick_get();
}
