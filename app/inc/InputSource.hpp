#pragma once

enum class InputSource {
    AUX_IN,
    RCA_DIGITAL_COAX,
    OPTICAL,
    HDMI_ARC,
    USB,
    BLUETOOTH,
    UNKNOWN,
};

constexpr bool canPlayPause( InputSource s ) {
    switch(s) {
    case InputSource::HDMI_ARC:
    case InputSource::USB:
    case InputSource::BLUETOOTH:
        return true;
    case InputSource::AUX_IN:
    case InputSource::RCA_DIGITAL_COAX:
    case InputSource::OPTICAL:
    case InputSource::UNKNOWN:
    default:
        return false;
    }
}

constexpr const char* toString( InputSource s ) {
    switch(s){
    case InputSource::AUX_IN:           return "AUX_IN";
    case InputSource::RCA_DIGITAL_COAX: return "RCA_DIGITAL_COAX";
    case InputSource::OPTICAL:          return "OPTICAL";
    case InputSource::HDMI_ARC:         return "HDMI_ARC";
    case InputSource::USB:              return "USB";
    case InputSource::BLUETOOTH:        return "BLUETOOTH";
    case InputSource::UNKNOWN:
    default:                            return "UNKNOWN";
    }
}