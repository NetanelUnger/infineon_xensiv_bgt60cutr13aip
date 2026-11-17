#ifdef RADAR_MOTION_DETECTION
#include <inttypes.h>
#include "common.h"
#include "motion_detection.h"
#include "radar_motion_detection.h"
#include "radar_clutter_localization.h"
#include "radar_clustering.h"
#include "resource_map.h"
#include "radar_config.h"
#include "xensiv_bgt60cutrxx_mtb.h"
#include "xensiv_bgt60cutrxx_regs.h"

/*******************************************************************************
 * Macros
 ******************************************************************************/

/** Enables runtime measurement of data processing components */
//#define MEASURE_PROCESSING_TIME

/** This data buffer is used to store the processed targets */
motion_detection_t motion_detection;

/** This data buffer is used to store the data and config for clutter localization */
clutter_localization_t clutter_localization = { 0 };
/** This variable is used to store the configuration for clutter localization */
clutter_localization_config_t clutter_localization_config = { 0 };

/** This data buffer is used to store the data and config for clustering */
clustering_t clustering;

/** allocates the memory to store the full range doppler map*/
#ifdef RADAR_MOTION_DETECTION_USE_RD_BUFFER
uint32_t rd_data_buffer[BGT60CUTR13_RD_DATABUFFER_MAX_SIZE];
#else
uint32_t rd_data_buffer[0];
#endif


// Function Prototypes

static void timer_start(void);
static uint32_t timer_get_elapsed_us(void);

// Global Variables

#ifdef MEASURE_PROCESSING_TIME
static cyhal_timer_t timer_obj;
#endif


// for certain configurations these values might not be provided
// to allow compilation define here default values
#ifndef RD_RANGE_RESOLUTION_M
#define RD_RANGE_RESOLUTION_M 0.36527864281192735
#endif

#ifndef RD_VELOCITY_GRID_MPS
#define RD_VELOCITY_GRID_MPS \
    {-2.384654096666888, -2.2356132156252078, -2.086572334583527, -1.9375314535418466, -1.788490572500166, -1.6394496914584855, -1.4904088104168052, -1.3413679293751246, -1.192327048333444, -1.0432861672917635, -0.894245286250083, -0.7452044052084026, -0.596163524166722, -0.4471226431250415, -0.298081762083361, -0.1490408810416805, 0.0, 0.1490408810416805, 0.298081762083361, 0.4471226431250415, 0.596163524166722, 0.7452044052084026, 0.894245286250083, 1.0432861672917635, 1.192327048333444, 1.3413679293751246, 1.4904088104168052, 1.6394496914584855, 1.788490572500166, 1.9375314535418466, 2.086572334583527, 2.2356132156252078}
#endif


static uint32_t clustering_elapsed_us = 0;
static float rd_velocity_grid[MAX_VELOCITY_GRID_BINS] = RD_VELOCITY_GRID_MPS;
static uint32_t rd_velocity_grid_bin_count = 32;
static sequence_settings_t rd_sequence_settings =
{
    .enabled            = MD_RD_ENABLED,
    .velocity_grid_mps  = rd_velocity_grid,
    .range_resolution_m = RD_RANGE_RESOLUTION_M
};

static float ulp_velocity_grid[MAX_VELOCITY_GRID_BINS] = ULP_VELOCITY_GRID_MPS;
static uint32_t ulp_velocity_grid_bin_count = 0;
static sequence_settings_t ulp_sequence_settings =
{
    .enabled            = MD_ULP_ENABLED,
    .velocity_grid_mps  = ulp_velocity_grid,
    .range_resolution_m = ULP_RANGE_RESOLUTION_M
};

clustering_settings_t configuration =
{
    .max_rows                        = 25,
    .min_range_m                     = 0.5f,
    .static_check_frame_count        = 5,
    .cluster_clear_counter_threshold = 100,
    .dbscan_eps_m                    = 1.0f,
    .dbscan_min_samples              = 5
};

static bool clutter_detection_enable = false;
static bool clustering_enable = false;

/*******************************************************************************
* Function Definitions
*******************************************************************************/

static void timer_start(void)
{
    #ifdef MEASURE_PROCESSING_TIME
    const cyhal_timer_cfg_t timer_cfg =
    {
        .compare_value = 0,                         // Timer compare value, not used
        .period        = CYHAL_TIMER_DEFAULT_FREQ,  // Timer period 1 second
        .direction     = CYHAL_TIMER_DIR_UP,        // Timer counts up
        .is_compare    = false,                     // Don't use compare mode
        .is_continuous = false,                     // Do not run timer indefinitely
        .value         = 0                          // Initial value of counter
    };

    cyhal_timer_free(&timer_obj);
    cyhal_timer_init(&timer_obj, NC, NULL);
    cyhal_timer_configure(&timer_obj, &timer_cfg);
    cyhal_timer_set_frequency(&timer_obj, CYHAL_TIMER_DEFAULT_FREQ);
    cyhal_timer_start(&timer_obj);
    #endif // ifdef MEASURE_PROCESSING_TIME
}


static uint32_t timer_get_elapsed_us(void)
{
    #ifdef MEASURE_PROCESSING_TIME
    cyhal_timer_stop(&timer_obj);
    return cyhal_timer_read(&timer_obj);
    #else
    return 0;
    #endif
}


void motion_detection_set_sequence_settings(const meta_sequence_settings_t* ulp, const meta_sequence_settings_t* rd)
{
    // Copy ULP velocity grid
    for (size_t i = 0; i < ulp->num_rd_velocity_bins && i < MAX_VELOCITY_GRID_BINS; ++i)
    {
        ulp_velocity_grid[i] = ulp->rd_velocity_grid_mps[i];
    }
    ulp_velocity_grid_bin_count = ulp->num_rd_velocity_bins;

    // Copy RD velocity grid
    for (size_t i = 0; i < rd->num_rd_velocity_bins && i < MAX_VELOCITY_GRID_BINS; ++i)
    {
        rd_velocity_grid[i] = rd->rd_velocity_grid_mps[i];
    }
    rd_velocity_grid_bin_count = rd->num_rd_velocity_bins;

    ulp_sequence_settings.enabled = ulp->enabled;
    ulp_sequence_settings.range_resolution_m = ulp->range_resolution_m;

    rd_sequence_settings.enabled = rd->enabled;
    rd_sequence_settings.range_resolution_m = rd->range_resolution_m;
}


void motion_detection_get_sequence_settings(meta_sequence_settings_t* ulp, meta_sequence_settings_t* rd)
{
    // Copy ULP velocity grid
    ulp->num_rd_velocity_bins = ulp_velocity_grid_bin_count;
    for (size_t i = 0; i < ulp->num_rd_velocity_bins && i < MAX_VELOCITY_GRID_BINS; ++i)
    {
        ulp->rd_velocity_grid_mps[i] = ulp_velocity_grid[i];
    }

    // Copy RD velocity grid
    rd->num_rd_velocity_bins = rd_velocity_grid_bin_count;
    for (size_t i = 0; i < rd->num_rd_velocity_bins && i < MAX_VELOCITY_GRID_BINS; ++i)
    {
        rd->rd_velocity_grid_mps[i] = rd_velocity_grid[i];
    }

    ulp->enabled = ulp_sequence_settings.enabled;
    ulp->range_resolution_m = ulp_sequence_settings.range_resolution_m;

    rd->enabled = rd_sequence_settings.enabled;
    rd->range_resolution_m = rd_sequence_settings.range_resolution_m;
}


void motion_detection_clutter_detection_enable(bool enable)
{
    clutter_detection_enable = enable;
}


void motion_detection_clustering_enable(bool enable)
{
    clustering_enable = enable;
}


int motion_detection_init(xensiv_bgt60cutrxx_t* radar_sensor)
{
    // initializes and connect the motion detector with the radar sensor
    int32_t result = radar_motion_detection_init(&motion_detection,
                                                 radar_sensor,
                                                 rd_data_buffer,
                                                 &rd_sequence_settings,
                                                 &ulp_sequence_settings);

    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        debug_print("radar system_init failed\n");
    }

    //update clutter exclusion zones
    // first reset the struct
    memset(&clutter_localization, 0, sizeof(clutter_localization_t));
    // read exclusion zone register
    result = xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_ZONE_EXCL0,
                                        &clutter_localization_config.default_exclusion_zones.MD_ZONE_EXCL0);
    result |= xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_ZONE_EXCL1,
                                         &clutter_localization_config.default_exclusion_zones.MD_ZONE_EXCL1);
    result |= xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_ZONE_EXCL2,
                                         &clutter_localization_config.default_exclusion_zones.MD_ZONE_EXCL2);
    result |= xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_ZONE_EXCL3,
                                         &clutter_localization_config.default_exclusion_zones.MD_ZONE_EXCL3);
    result |= xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_ZONE_CTRL,
                                         &clutter_localization_config.default_exclusion_zones.MD_ZONE_CTRL);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        debug_print("reading exclusion zone registers failed\n");
    }

    //calculate samples per chirp from the fft size
    uint32_t md_rd_ctrl = 0;
    result = xensiv_bgt60cutrxx_get_reg(radar_sensor, BGT60CUTR13B_REG_MD_RD_CTRL, &md_rd_ctrl);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        debug_print("reading RD_CTRL register failed\n");
    }
    uint32_t samples_per_chirp = 1 <<
                                 ((md_rd_ctrl & BGT60CUTR13B_MD_RD_CTRL_RFFT_SIZE_msk) >>
                                  BGT60CUTR13B_MD_RD_CTRL_RFFT_SIZE_pos);

    clutter_localization_config.doppler_gating_speed_mps = 0.25;
    clutter_localization_config.increment_count = 2;
    clutter_localization_config.decrement_count = 1;
    clutter_localization_config.saturation_limit = 100;
    clutter_localization_config.settings_threshold = 10;
    clutter_localization_config.priority_h_plane = false;
    clutter_localization_config.num_neighboring_zones = 1;
    clutter_localization_config.period_ticks = 150;
    clutter_localization_config.max_duration_ticks = 100;
    clutter_localization_config.auto_masking_active = true;
    clutter_localization_config.deactivation_counter_threshold_ticks = 500;
    clutter_localization_config.range_max = motion_detection.rd_sequence_settings->range_resolution_m *
                                            samples_per_chirp / 2.0f;

    result = clutter_localization_init(&motion_detection, &clutter_localization, &clutter_localization_config);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        debug_print("clutter localization init failed\n");
    }

    debug_print("Xensiv radar motion detection and clutter localization initialized!!!\r\n\n");

    result = clustering_init(&clustering, &configuration);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        debug_print("clustering init failed\n");
    }

    debug_print("Xensiv radar motion detection and clustering initialized!!!\r\n\n");

    uint32_t irq_status = 0;
    xensiv_bgt60cutrxx_get_irq_status(motion_detection.radar_sensor, &irq_status);
    debug_print("IR_STAT before clear: 0x%08lX\n", irq_status);
    // clear irq 0 to 11(seq3)
    xensiv_bgt60cutrxx_set_irq_clear(motion_detection.radar_sensor, BGT60CUTR13B_IR_CLEAR_FOR_STARTUP_MSK);
    return result;
}


void print_presence(bool presence, uint32_t counter)
{
    // Print the presence status in JSON format
    //  e.g. {"presence": {"state": true}}
    terminal_printf("{\"ts\": %" PRIu32 ", \"presence\":{\"state\":%s}}\n",
                    counter,
                    (presence != false) ? "true" : "false");
}


void print_detections(motion_detection_t* motion_detection, uint32_t counter, bool print_attributes)
{
    uint16_t type = 0;
    bool target_detected = false;

    terminal_printf("{\"ts\": %" PRIu32 ", \"detections\": {", counter);

    if (print_attributes)
    {
        terminal_printf(
            "\"attributes\":[\"range_m\",\"azimuth_deg\",\"elevation_deg\",\"magnitude_dBFS\",\"speed_mps\",\"rx_idx\",\"range_bin\",\"doppler_bin\",\"sequence_num\",\"type\",\"seq_count_1\",\"seq_count_2\",\"ir_stat\"],");
    }

    terminal_printf("\"data\": [");

    if (motion_detection->seq1_done)
    {
        type = 1; // ulp detected
    }
    else if (motion_detection->seq2_done)
    {
        type = 2; // rd detected
    }
    else
    {
        type = 0; // No motion detected
    }

    for (uint8_t i = 0; i < BGT60CUTR13_PROCESSED_MD_TARGET_MAX; i++)
    {
        // check if targets are detected then print the targets and in case of no targets are detected, print an empty
        // array
        if ((motion_detection->processed_targets[i].antenna_rx > 0) ||
            ((i == (BGT60CUTR13_PROCESSED_MD_TARGET_MAX - 1)) && !target_detected))
        {
            if (target_detected)
            {
                terminal_printf(", ");
            }
            target_detected = true;
            terminal_printf("[%.2f, %.2f, %.2f, %.2f, %.2f, %d, %d, %d, %d, %d, %d, %d, %" PRIu32 "]",
                            motion_detection->processed_targets[i].range_m,
                            motion_detection->processed_targets[i].azimuth_deg,
                            motion_detection->processed_targets[i].elevation_deg,
                            motion_detection->processed_targets[i].magnitude_dBFS,
                            motion_detection->processed_targets[i].velocity_mps,
                            motion_detection->processed_targets[i].antenna_rx,
                            motion_detection->range_bin[i],
                            motion_detection->doppler_bin[i],
                            motion_detection->sequence,
                            type,
                            motion_detection->sequence_counters.seq1_counter,
                            motion_detection->sequence_counters.seq2_counter,
                            motion_detection->ir_stat);
        }
    }
    terminal_printf("]}}\n");
}


void print_clutter_zones(clutter_localization_t* clutter, uint32_t counter)
{
    static clutter_localization_state_t previous_state = CLUTTER_LOCALIZATION_STATE_ZONES_RESET;
    static char* event_type = NULL;
    static bool is_azimuth_clutter = false;
    bool target_detected = false;

    //only print the zones if the exclusion zone registers are written
    if ((clutter->register_update_state == previous_state) && (clutter->period_counter_ticks != 0))
    {
        return;
    }

    if (clutter->register_update_state != CLUTTER_LOCALIZATION_STATE_ZONES_RESET)
    {
        is_azimuth_clutter = clutter->zone_registers.MD_ZONE_CTRL ==
                             (BGT60CUTR13B_MD_ZONE_CTRL_PLANE_SEL_E_PLANE << BGT60CUTR13B_MD_ZONE_CTRL_PLANE_SEL_pos);
        event_type = is_azimuth_clutter ? "azimuth_clutter" : "elevation_clutter";
    }

    // convert register to zone marker array
    zone_marker_array_t zone_marker = { 0 };
    clutter_localization_map_registers_to_zones(clutter, &zone_marker);

    terminal_printf("{\"ts\": %" PRIu32 ", \"%s\": {\"active\":[", counter, event_type);
    for (uint8_t r_zone = 0; r_zone < CLUTTER_LOCALIZATION_NUM_RANGE_ZONES; r_zone++)
    {
        for (uint8_t a_zone = 0; a_zone < CLUTTER_LOCALIZATION_NUM_ANGLE_ZONES; a_zone++)
        {
            if (zone_marker[r_zone][a_zone] >= CLUTTER_LOCALIZATION_EXCLUSION_VALUE)
            {
                if (target_detected)
                {
                    terminal_printf(", ");
                }
                target_detected = true;
                terminal_printf("[%d, %d]", r_zone, a_zone);
            }
        }
    }
    terminal_printf("]}}\n");
    previous_state = clutter->register_update_state;
}


void print_calibration_data(xensiv_bgt60cutrxx_calibration_data_t* calibration_settings, uint32_t* calibration_data,
                            uint32_t counter)
{
    CY_ASSERT(calibration_settings != NULL);
    CY_ASSERT(calibration_data != NULL);

    terminal_printf("{\"ts\": %" PRIu32 ", \"calibration_data\": {", counter);
    // Print the calibration settings
    terminal_printf(
        "\"rx0_address\": \"%d\",\"rx1_address\":\"%d\",\"rx2_address\":\"%d\",\"bytes_to_read\":\"%d\", \"data\": [",
        calibration_settings->rx0_address,
        calibration_settings->rx1_address,
        calibration_settings->rx2_address,
        calibration_settings->bytes_to_read);
    for (uint32_t i = 0; i < (3 * calibration_settings->bytes_to_read) / sizeof(uint32_t); i++)
    {
        if (i != 0)
        {
            terminal_printf(", ");
        }
        terminal_printf("\"%" PRIu32 "\"", calibration_data[i]);
    }
    terminal_printf("]}}\n");
}


void print_clustering_results(clustering_t* clustering, uint32_t counter, bool print_attributes)
{
    const uint16_t type = 3;  // clustering centroid type
    const uint32_t dummy = 0;

    terminal_printf("{\"ts\": %" PRIu32 ", \"detections\": {", counter);
    if (print_attributes)
    {
        terminal_printf(
            "\"attributes\":[\"range_m\",\"azimuth_deg\",\"elevation_deg\",\"magnitude_dBFS\",\"speed_mps\",\"rx_idx\",\"range_bin\",\"doppler_bin\",\"sequence_num\",\"type\",\"seq_count_1\",\"seq_count_2\",\"ir_stat\"],");
    }

    terminal_printf("\"data\": [");

    if (clustering->num_clusters == 0)
    {
        // if no centroids were detected, print an empty array
        terminal_printf("[%.2f, %.2f, %.2f, %.2f, %.2f, %d, %d, %d, %d, %d, %d, %d, %" PRIu32 "]",
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f, // magnitude not available for clustering centroids
                        0.0f, // speed not available for clustering centroids
                        0, // rx antenna not available for clustering centroids
                        0, // range bin not available for clustering centroids
                        0, // doppler bin not available for clustering centroids
                        0, // sequence not available for clustering centroids
                        type,
                        0, // seq_count_1 not available for clustering centroids
                        0, // seq_count_2 not available for clustering centroids
                        dummy); // ir_stat not available for clustering centroids
        terminal_printf("]}}\n");
    }
    else
    {
        // print detected centroids
        for (int32_t i = 0; i < clustering->num_clusters; i++)
        {
            if (i != 0)
            {
                terminal_printf(", ");
            }
            terminal_printf("[%.2f, %.2f, %.2f, %.2f, %.2f, %d, %d, %d, %d, %d, %d, %d, %" PRIu32 "]",
                            clustering->centroids[i].range_m,
                            clustering->centroids[i].azimuth_deg,
                            clustering->centroids[i].elevation_deg,
                            0.0f, // magnitude not available for clustering centroids
                            0.0f, // speed not available for clustering centroids
                            0, // rx antenna not available for clustering centroids
                            0, // range bin not available for clustering centroids
                            0, // doppler bin not available for clustering centroids
                            0, // sequence not available for clustering centroids
                            type,
                            0, // seq_count_1 not available for clustering centroids
                            0, // seq_count_2 not available for clustering centroids
                            dummy); // ir_stat not available for clustering centroids
        }
        terminal_printf("]}}\n");
    }
}


void print_algo_processing_times(uint32_t counter, uint32_t motion_detection_elapsed_us,
                                 uint32_t clutter_localization_elapsed_us, uint32_t clustering_elapsed_us,
                                 uint32_t clock_speed_hz)
{
    static uint32_t min_motion_detection_time_us = UINT32_MAX;
    static uint32_t min_clutter_localization_time_us = UINT32_MAX;
    static uint32_t min_clustering_time_us = UINT32_MAX;

    static uint32_t max_motion_detection_time_us = 0;
    static uint32_t max_clutter_localization_time_us = 0;
    static uint32_t max_clustering_time_us = 0;

    static uint32_t avg_motion_detection_time_us = 0;
    static uint32_t avg_clutter_localization_time_us = 0;
    static uint32_t avg_clustering_time_us = 0;

    // update min times
    if (motion_detection_elapsed_us < min_motion_detection_time_us)
    {
        min_motion_detection_time_us = motion_detection_elapsed_us;
    }
    if (clutter_localization_elapsed_us < min_clutter_localization_time_us)
    {
        min_clutter_localization_time_us = clutter_localization_elapsed_us;
    }
    if (clustering_elapsed_us < min_clustering_time_us)
    {
        min_clustering_time_us = clustering_elapsed_us;
    }

    // update max times
    if (motion_detection_elapsed_us > max_motion_detection_time_us)
    {
        max_motion_detection_time_us = motion_detection_elapsed_us;
    }
    if (clutter_localization_elapsed_us > max_clutter_localization_time_us)
    {
        max_clutter_localization_time_us = clutter_localization_elapsed_us;
    }
    if (clustering_elapsed_us > max_clustering_time_us)
    {
        max_clustering_time_us = clustering_elapsed_us;
    }

    // update average times

    if (avg_motion_detection_time_us != 0)
    {
        avg_motion_detection_time_us += motion_detection_elapsed_us;
        avg_motion_detection_time_us /= 2;
    }
    else
    {
        avg_motion_detection_time_us = motion_detection_elapsed_us;
    }
    if (avg_clutter_localization_time_us != 0)
    {
        avg_clutter_localization_time_us += clutter_localization_elapsed_us;
        avg_clutter_localization_time_us /= 2;
    }
    else
    {
        avg_clutter_localization_time_us = clutter_localization_elapsed_us;
    }
    if (avg_clustering_time_us != 0)
    {
        avg_clustering_time_us += clustering_elapsed_us;
        avg_clustering_time_us /= 2;
    }
    else
    {
        avg_clustering_time_us = clustering_elapsed_us;
    }
    terminal_printf("{\"ts\": %" PRIu32 ", \"clock_speed_hz\":%" PRIu32 ", \"processing_times_us\": {", counter,
                    clock_speed_hz);
    terminal_printf(
        "\"motion_detection_min\":%" PRIu32 ",\"motion_detection_max\":%" PRIu32 ",\"motion_detection_avg\":%" PRIu32 ",",
        min_motion_detection_time_us,
        max_motion_detection_time_us,
        avg_motion_detection_time_us);
    terminal_printf(
        "\"clutter_localization_min\":%" PRIu32 ",\"clutter_localization_max\":%" PRIu32 ",\"clutter_localization_avg\":%" PRIu32 ",",
        min_clutter_localization_time_us,
        max_clutter_localization_time_us,
        avg_clutter_localization_time_us);
    terminal_printf("\"clustering_min\":%" PRIu32 ",\"clustering_max\":%" PRIu32 ",\"clustering_avg\":%" PRIu32 "",
                    min_clustering_time_us,
                    max_clustering_time_us,
                    avg_clustering_time_us);
    terminal_printf("}}\n");
}


void handle_motion_detector_interrupt()
{
    /* Process radar motion detection */
    int32_t result = radar_motion_detection_process(&motion_detection);
    if (result < 0)
    {
        debug_print("Motion Detection process error: %ld\n", result);
    }
}


void print_motion_detection_results(motion_detection_t* md_results)
{
    static uint32_t counter = 0;
    bool clutter_detection_active = clutter_detection_enable;
    bool clustering_active = clustering_enable;
    int32_t result = 0;
    // process clutter localization only in case of range doppler sequence has been finished
    if (clutter_detection_active)
    {
        clutter_localization_process(md_results, &clutter_localization);
    }

    // Process clustering
    if (clustering_active)
    {
        timer_start();
        result = clustering_process(md_results, &clustering);
        clustering_elapsed_us = timer_get_elapsed_us();
        if (result < 0)
        {
            debug_print("Clustering process error: %ld\n", result);
        }
    }

    // Print the detections
    if (clustering_active)
    {
        if (md_results->seq2_done || clustering.ulp_seq_done_flag)
        {
            print_detections(&motion_detection, counter, true);
        }
    }
    else
    {
        print_detections(&motion_detection, counter, true);
    }

    // Print the clutter zones, if they have been updated
    if (clutter_detection_active && !md_results->seq1_done)
    {
        print_clutter_zones(&clutter_localization, counter);
    }

    // Print clustering results
    if (clustering_active && !md_results->seq2_done)
    {
        if (clustering.ulp_seq_done_flag)
        {
            print_clustering_results(&clustering, counter, true);
        }
    }
    counter++;
}


int motion_detection_get_results(motion_detection_t* md_results)
{
    memcpy(md_results, &motion_detection, sizeof(motion_detection_t));
    return 0;
}


#endif // ifdef RADAR_MOTION_DETECTION
/* [] END OF FILE */
