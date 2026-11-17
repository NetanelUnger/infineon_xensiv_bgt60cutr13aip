#ifndef RADAR_CONFIG_H
#define RADAR_CONFIG_H

#ifdef RADAR_MOTION_DETECTION
#include "presence_detection_localization.h"
    #if defined(RADAR_ISM_BAND_JAPAN)
        #error "RADAR_ISM_BAND_JAPAN is not supported for RADAR_MOTION_DETECTION application"
    #endif
#elif defined(RADAR_ISM_BAND_JAPAN)
#include "presence_detection_tracking_japan.h"
#else
#include "presence_detection_tracking.h"
#endif
#endif /* RADAR_CONFIG_H */
