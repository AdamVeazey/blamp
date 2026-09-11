#pragma once

#include <zephyr/drivers/sensor.h>

/*
 * Intended to be used with Device Tree
 * QuadratureEncoder qdec( DEVICE_DT_GET(DT_NODELABEL(qdec)) );
 * qdec.getValue();
 */
class QuadratureEncoder {
private:
    const device* const dev;
public:
    QuadratureEncoder( const device* const dev ) : dev(dev) {}
    bool isReady() const { return device_is_ready(dev); }
    int32_t getValue() const {
        sensor_value v;
        sensor_sample_fetch( dev );
        sensor_channel_get( dev, SENSOR_CHAN_ROTATION, &v );
        return v.val1;
    };
};

class MockQuadratureEncoder {
private:
    static inline int32_t current = 0;
public:
    static bool isReady() { return true; }
    static int32_t getValue() { return current; };
    static void mockRotate( int32_t value ) { current += value; }
};