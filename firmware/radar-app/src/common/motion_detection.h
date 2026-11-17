#ifndef SRC_COMMON_MOTION_DETECTION_H
#define SRC_COMMON_MOTION_DETECTION_H

#include "xensiv_bgt60cutrxx_mtb.h"
#include "radar_motion_detection.h"

void handle_motion_detector_interrupt(void);
int motion_detection_init(xensiv_bgt60cutrxx_t* radar_sensor);
void motion_detection_get_sequence_settings(meta_sequence_settings_t* ulp, meta_sequence_settings_t* rd);
void motion_detection_set_sequence_settings(const meta_sequence_settings_t* ulp, const meta_sequence_settings_t* rd);
void print_motion_detection_results(motion_detection_t* md_results);
int motion_detection_get_results(motion_detection_t* md_results);
void motion_detection_clutter_detection_enable(bool enable);
void motion_detection_clustering_enable(bool enable);
#endif /*SRC_COMMON_MOTION_DETECTION_H*/
