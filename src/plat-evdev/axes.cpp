extern "C" {
#include <linux/input.h>
}
void evdev_init_axes(const char** x) {
#ifdef ABS_X
x[ABS_X] = "X";
#endif
#ifdef ABS_Y
x[ABS_Y] = "Y";
#endif
#ifdef ABS_Z
x[ABS_Z] = "Z";
#endif
#ifdef ABS_RX
x[ABS_RX] = "RX";
#endif
#ifdef ABS_RY
x[ABS_RY] = "RY";
#endif
#ifdef ABS_RZ
x[ABS_RZ] = "RZ";
#endif
#ifdef ABS_THROTTLE
x[ABS_THROTTLE] = "THROTTLE";
#endif
#ifdef ABS_RUDDER
x[ABS_RUDDER] = "RUDDER";
#endif
#ifdef ABS_WHEEL
x[ABS_WHEEL] = "WHEEL";
#endif
#ifdef ABS_GAS
x[ABS_GAS] = "GAS";
#endif
#ifdef ABS_BRAKE
x[ABS_BRAKE] = "BRAKE";
#endif
#ifdef ABS_HAT0X
x[ABS_HAT0X] = "HAT0X";
#endif
#ifdef ABS_HAT0Y
x[ABS_HAT0Y] = "HAT0Y";
#endif
#ifdef ABS_HAT1X
x[ABS_HAT1X] = "HAT1X";
#endif
#ifdef ABS_HAT1Y
x[ABS_HAT1Y] = "HAT1Y";
#endif
#ifdef ABS_HAT2X
x[ABS_HAT2X] = "HAT2X";
#endif
#ifdef ABS_HAT2Y
x[ABS_HAT2Y] = "HAT2Y";
#endif
#ifdef ABS_HAT3X
x[ABS_HAT3X] = "HAT3X";
#endif
#ifdef ABS_HAT3Y
x[ABS_HAT3Y] = "HAT3Y";
#endif
#ifdef ABS_PRESSURE
x[ABS_PRESSURE] = "PRESSURE";
#endif
#ifdef ABS_DISTANCE
x[ABS_DISTANCE] = "DISTANCE";
#endif
#ifdef ABS_TILT_X
x[ABS_TILT_X] = "TILT_X";
#endif
#ifdef ABS_TILT_Y
x[ABS_TILT_Y] = "TILT_Y";
#endif
#ifdef ABS_TOOL_WIDTH
x[ABS_TOOL_WIDTH] = "TOOL_WIDTH";
#endif
#ifdef ABS_VOLUME
x[ABS_VOLUME] = "VOLUME";
#endif
#ifdef ABS_MISC
x[ABS_MISC] = "MISC";
#endif
#ifdef ABS_MT_SLOT
x[ABS_MT_SLOT] = "MT_SLOT";
#endif
#ifdef ABS_MT_TOUCH_MAJOR
x[ABS_MT_TOUCH_MAJOR] = "MT_TOUCH_MAJOR";
#endif
#ifdef ABS_MT_TOUCH_MINOR
x[ABS_MT_TOUCH_MINOR] = "MT_TOUCH_MINOR";
#endif
#ifdef ABS_MT_WIDTH_MAJOR
x[ABS_MT_WIDTH_MAJOR] = "MT_WIDTH_MAJOR";
#endif
#ifdef ABS_MT_WIDTH_MINOR
x[ABS_MT_WIDTH_MINOR] = "MT_WIDTH_MINOR";
#endif
#ifdef ABS_MT_ORIENTATION
x[ABS_MT_ORIENTATION] = "MT_ORIENTATION";
#endif
#ifdef ABS_MT_POSITION_X
x[ABS_MT_POSITION_X] = "MT_POSITION_X";
#endif
#ifdef ABS_MT_POSITION_Y
x[ABS_MT_POSITION_Y] = "MT_POSITION_Y";
#endif
#ifdef ABS_MT_TOOL_TYPE
x[ABS_MT_TOOL_TYPE] = "MT_TOOL_TYPE";
#endif
#ifdef ABS_MT_BLOB_ID
x[ABS_MT_BLOB_ID] = "MT_BLOB_ID";
#endif
#ifdef ABS_MT_TRACKING_ID
x[ABS_MT_TRACKING_ID] = "MT_TRACKING_ID";
#endif
#ifdef ABS_MT_PRESSURE
x[ABS_MT_PRESSURE] = "MT_PRESSURE";
#endif
#ifdef ABS_MT_DISTANCE
x[ABS_MT_DISTANCE] = "MT_DISTANCE";
#endif
}
