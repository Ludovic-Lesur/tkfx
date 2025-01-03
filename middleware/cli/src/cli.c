/*
 * at.c
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#include "cli.h"

// Peripherals.
#include "i2c_address.h"
#include "iwdg.h"
#include "lptim.h"
#include "nvic_priority.h"
#include "nvm.h"
#include "nvm_address.h"
#include "pwr.h"
#include "rcc.h"
// Utils.
#include "at.h"
#include "at_instance.h"
#include "error.h"
#include "math.h"
#include "parser.h"
#include "string.h"
#include "terminal_instance.h"
#include "types.h"
// Components.
#include "sht3x.h"
// Middleware.
#include "analog.h"
#include "gps.h"
#include "power.h"
// Sigfox.
#include "manuf/rf_api.h"
#include "sigfox_ep_addon_rfp_api.h"
#include "sigfox_ep_api.h"
#include "sigfox_error.h"
#include "sigfox_rc.h"
#include "sigfox_types.h"
// Applicative.
#include "error_base.h"
#include "tkfx_flags.h"
#include "version.h"

#ifdef TKFX_MODE_CLI

/*** CLI local macros ***/

// Parsing.
#define CLI_CHAR_SEPARATOR          STRING_CHAR_COMMA
// Duration of RSSI command.
#define CLI_RSSI_REPORT_PERIOD_MS   500
// Enabled commands.
#define CLI_COMMAND_NVM
#define CLI_COMMAND_SENSORS
#define CLI_COMMAND_GPS
#define CLI_COMMAND_SIGFOX_EP_LIB
#define CLI_COMMAND_SIGFOX_EP_ADDON_RFP
#define CLI_COMMAND_CW
#define CLI_COMMAND_RSSI

/*** CLI local structures ***/

/*******************************************************************/
typedef struct {
    volatile uint8_t at_process_flag;
    PARSER_context_t* at_parser_ptr;
} CLI_context_t;

/*** CLI local functions declaration ***/

/*******************************************************************/
static AT_status_t _CLI_rst_callback(void);
static AT_status_t _CLI_rcc_callback(void);
/*******************************************************************/
#ifdef CLI_COMMAND_NVM
static AT_status_t _CLI_nvm_callback(void);
static AT_status_t _CLI_get_ep_id_callback(void);
static AT_status_t _CLI_set_ep_id_callback(void);
static AT_status_t _CLI_get_ep_key_callback(void);
static AT_status_t _CLI_set_ep_key_callback(void);
#endif
/*******************************************************************/
#ifdef CLI_COMMAND_SENSORS
static AT_status_t _CLI_adc_callback(void);
static AT_status_t _CLI_ths_callback(void);
static AT_status_t _CLI_acc_callback(void);
#endif
/*******************************************************************/
#ifdef CLI_COMMAND_GPS
static AT_status_t _CLI_gps_callback(void);
#endif
/*******************************************************************/
#ifdef CLI_COMMAND_SIGFOX_EP_LIB
#ifdef CONTROL_KEEP_ALIVE_MESSAGE
static AT_status_t _CLI_so_callback(void);
#endif
static AT_status_t _CLI_sb_callback(void);
static AT_status_t _CLI_sf_callback(void);
#endif
/*******************************************************************/
#ifdef CLI_COMMAND_SIGFOX_EP_ADDON_RFP
static AT_status_t _CLI_tm_callback(void);
#endif
/*******************************************************************/
#ifdef CLI_COMMAND_CW
static AT_status_t _CLI_cw_callback(void);
#endif
#if (defined CLI_COMMAND_RSSI) && (defined BIDIRECTIONAL)
static AT_status_t _CLI_rssi_callback(void);
#endif

/*** CLI local global variables ***/

static const AT_command_t CLI_COMMANDS_LIST[] = {
    {
        .syntax = "$RST",
        .parameters = NULL,
        .description = "Reset MCU",
        .callback = &_CLI_rst_callback
    },
    {
        .syntax = "$RCC?",
        .parameters = NULL,
        .description = "Get clocks frequency",
        .callback = &_CLI_rcc_callback
    },
#ifdef CLI_COMMAND_NVM
    {
        .syntax = "$NVM=",
        .parameters = "<address[dec]>",
        .description = "Read NVM byte",
        .callback = &_CLI_nvm_callback
    },
    {
        .syntax = "$ID?",
        .parameters = NULL,
        .description = "Get Sigfox EP ID",
        .callback = &_CLI_get_ep_id_callback
    },
    {
        .syntax = "$ID=",
        .parameters = "<id[hex]>",
        .description = "Set Sigfox EP ID",
        .callback = &_CLI_set_ep_id_callback
    },
    {
        .syntax = "$KEY?",
        .parameters = NULL,
        .description = "Get Sigfox EP key",
        .callback = &_CLI_get_ep_key_callback
    },
    {
        .syntax = "$KEY=",
        .parameters = "<key[hex]>",
        .description = "Set Sigfox EP key",
        .callback = &_CLI_set_ep_key_callback
    },
#endif
#ifdef CLI_COMMAND_SENSORS
    {
        .syntax = "$ADC?",
        .parameters = NULL,
        .description = "Read analog measurements",
        .callback = &_CLI_adc_callback
    },
    {
        .syntax = "$THS?",
        .parameters = NULL,
        .description = "Read temperature and humidity",
        .callback = &_CLI_ths_callback
    },
    {
        .syntax = "$ACC?",
        .parameters = NULL,
        .description = "Read accelerometer chip ID",
        .callback = &_CLI_acc_callback
    },
#endif
#ifdef CLI_COMMAND_GPS
    {
        .syntax = "$GPS=",
        .parameters = "<timeout[s]>",
        .description = "Get GPS position",
        .callback = &_CLI_gps_callback
    },
#endif
#ifdef CLI_COMMAND_SIGFOX_EP_LIB
#ifdef CONTROL_KEEP_ALIVE_MESSAGE
    {
        .syntax = "$SO",
        .parameters = NULL,
        .description = "Sigfox send control keep-alive",
        .callback = &_CLI_so_callback
    },
#endif
    {
        .syntax = "$SB=",
        .parameters = "<data[bit]>,(<bidir_flag[bit]>)",
        .description = "Sigfox send bit",
        .callback = &_CLI_sb_callback
    },
    {
        .syntax = "$SF=",
        .parameters = "<data[hex]>,(<bidir_flag[bit]>)",
        .description = "Sigfox send frame",
        .callback = &_CLI_sf_callback
    },
#endif
#ifdef CLI_COMMAND_SIGFOX_EP_ADDON_RFP
    {
        .syntax = "$TM=",
        .parameters = "<bit_rate_index[dec]>,<test_mode_reference[dec]>",
        .description = "Sigfox RFP test mode",
        .callback = _CLI_tm_callback
    },
#endif
#ifdef CLI_COMMAND_CW
    {
        .syntax = "$CW=",
        .parameters = "<frequency[hz]>,<enable[bit]>,(<output_power[dbm]>)",
        .description = "Continuous wave",
        .callback = &_CLI_cw_callback
    },
#endif
#if (defined CLI_COMMAND_RSSI) && (defined BIDIRECTIONAL)
    {
        .syntax = "$RSSI=",
        .parameters = "<frequency[hz]>,<duration[s]>",
        .description = "Continuous RSSI measurement",
        .callback = &_CLI_rssi_callback
    }
#endif
};

static CLI_context_t cli_ctx;

/*** CLI local functions ***/

/*******************************************************************/
#define _CLI_check_driver_status(driver_status, driver_success, driver_error_base) { \
    /* Check value */ \
    if (driver_status != driver_success) { \
        /* Stack error */ \
        ERROR_stack_add((ERROR_code_t) (driver_error_base + driver_status)); \
        /* Exit with execution error */ \
        status = AT_ERROR_COMMAND_EXECUTION; \
        goto errors; \
    } \
}

/*******************************************************************/
static void _CLI_at_process_callback(void) {
    // Set local flag.
    cli_ctx.at_process_flag = 1;
}

/*******************************************************************/
static AT_status_t _CLI_rst_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Execute command.
    PWR_software_reset();
    return status;
}

/*******************************************************************/
static AT_status_t _CLI_rcc_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    RCC_status_t rcc_status = RCC_SUCCESS;
    char_t* rcc_clock_name[RCC_CLOCK_LAST - 1] = { "SYS", "HSI", "MSI", "HSE", "PLL", "LSI", "LSE" };
    uint8_t rcc_clock_index = 0;
    uint8_t clock_status = 0;
    uint32_t clock_frequency = 0;
    uint8_t idx = 0;
    // Calibrate clocks.
    rcc_status = RCC_calibrate_internal_clocks(NVIC_PRIORITY_CLOCK_CALIBRATION);
    _CLI_check_driver_status(rcc_status, RCC_SUCCESS, ERROR_BASE_RCC);
    // Clocks loop.
    for (idx = 0; idx < (RCC_CLOCK_LAST - 1); idx++) {
        // Convert to RCC clock index.
        rcc_clock_index = (RCC_CLOCK_NONE + 1 + idx);
        // Read status.
        rcc_status = RCC_get_status(rcc_clock_index, &clock_status);
        _CLI_check_driver_status(rcc_status, RCC_SUCCESS, ERROR_BASE_RCC);
        // Read frequency.
        rcc_status = RCC_get_frequency_hz(rcc_clock_index, &clock_frequency);
        _CLI_check_driver_status(rcc_status, RCC_SUCCESS, ERROR_BASE_RCC);
        // Print data.
        AT_reply_add_string(AT_INSTANCE_CLI, rcc_clock_name[idx]);
        AT_reply_add_string(AT_INSTANCE_CLI, (clock_status == 0) ? ": OFF " : ": ON  ");
        AT_reply_add_integer(AT_INSTANCE_CLI, (int32_t) clock_frequency, STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "Hz");
        AT_send_reply(AT_INSTANCE_CLI);
    }
errors:
    return status;
}

#ifdef CLI_COMMAND_NVM
/*******************************************************************/
static AT_status_t _CLI_nvm_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    NVM_status_t nvm_status = NVM_SUCCESS;
    int32_t address = 0;
    uint8_t nvm_data = 0;
    // Read address parameter.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &address);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Read byte at requested address.
    nvm_status = NVM_read_byte((NVM_address_t) address, &nvm_data);
    _CLI_check_driver_status(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM);
    // Print data.
    AT_reply_add_integer(AT_INSTANCE_CLI, (int32_t) nvm_data, STRING_FORMAT_HEXADECIMAL, 1);
    AT_send_reply(AT_INSTANCE_CLI);
errors:
    return status;
}
#endif

#ifdef CLI_COMMAND_NVM
/*******************************************************************/
static AT_status_t _CLI_get_ep_id_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    NVM_status_t nvm_status = NVM_SUCCESS;
    uint8_t idx = 0;
    uint8_t id_byte = 0;
    // Retrieve device ID in NVM.
    for (idx = 0; idx < SIGFOX_EP_ID_SIZE_BYTES; idx++) {
        nvm_status = NVM_read_byte((NVM_ADDRESS_SIGFOX_EP_ID + idx), &id_byte);
        _CLI_check_driver_status(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM);
        AT_reply_add_integer(AT_INSTANCE_CLI, id_byte, STRING_FORMAT_HEXADECIMAL, ((idx == 0) ? 1 : 0));
    }
    AT_send_reply(AT_INSTANCE_CLI);
errors:
    return status;
}
#endif

#ifdef CLI_COMMAND_NVM
/*******************************************************************/
static AT_status_t _CLI_set_ep_id_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    NVM_status_t nvm_status = NVM_SUCCESS;
    uint8_t sigfox_ep_id[SIGFOX_EP_ID_SIZE_BYTES];
    uint32_t unused = 0;
    uint8_t idx = 0;
    // Read ID parameter.
    parser_status = PARSER_get_byte_array(cli_ctx.at_parser_ptr, STRING_CHAR_NULL, SIGFOX_EP_ID_SIZE_BYTES, 1, sigfox_ep_id, &unused);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Write device ID in NVM.
    for (idx = 0; idx < SIGFOX_EP_ID_SIZE_BYTES; idx++) {
        nvm_status = NVM_write_byte((NVM_ADDRESS_SIGFOX_EP_ID + idx), sigfox_ep_id[idx]);
        _CLI_check_driver_status(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM);
    }
errors:
    return status;
}
#endif

#ifdef CLI_COMMAND_NVM
/*******************************************************************/
static AT_status_t _CLI_get_ep_key_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    NVM_status_t nvm_status = NVM_SUCCESS;
    uint8_t idx = 0;
    uint8_t key_byte = 0;
    // Retrieve device key in NVM.
    for (idx = 0; idx < SIGFOX_EP_KEY_SIZE_BYTES; idx++) {
        nvm_status = NVM_read_byte((NVM_ADDRESS_SIGFOX_EP_KEY + idx), &key_byte);
        _CLI_check_driver_status(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM);
        AT_reply_add_integer(AT_INSTANCE_CLI, key_byte, STRING_FORMAT_HEXADECIMAL, ((idx == 0) ? 1 : 0));
    }
    AT_send_reply(AT_INSTANCE_CLI);
errors:
    return status;
}
#endif

#ifdef CLI_COMMAND_NVM
/*******************************************************************/
static AT_status_t _CLI_set_ep_key_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    NVM_status_t nvm_status = NVM_SUCCESS;
    uint8_t sigfox_ep_key[SIGFOX_EP_KEY_SIZE_BYTES];
    uint32_t unused = 0;
    uint8_t idx = 0;
    // Read key parameter.
    parser_status = PARSER_get_byte_array(cli_ctx.at_parser_ptr, STRING_CHAR_NULL, SIGFOX_EP_KEY_SIZE_BYTES, 1, sigfox_ep_key, &unused);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Write device ID in NVM.
    for (idx = 0; idx < SIGFOX_EP_KEY_SIZE_BYTES; idx++) {
        nvm_status = NVM_write_byte((NVM_ADDRESS_SIGFOX_EP_KEY + idx), sigfox_ep_key[idx]);
        _CLI_check_driver_status(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM);
    }
errors:
    return status;
}
#endif

#ifdef CLI_COMMAND_SENSORS
/*******************************************************************/
static AT_status_t _CLI_adc_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    POWER_status_t power_status = POWER_SUCCESS;
    ANALOG_status_t analog_status = ANALOG_SUCCESS;
    int32_t generic_s32 = 0;
    // Turn analog front-end on.
    power_status = POWER_enable(POWER_DOMAIN_ANALOG, LPTIM_DELAY_MODE_ACTIVE);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // MCU voltage.
    analog_status = ANALOG_convert_channel(ANALOG_CHANNEL_VMCU_MV, &generic_s32);
    _CLI_check_driver_status(analog_status, ANALOG_SUCCESS, ERROR_BASE_ANALOG);
    AT_reply_add_string(AT_INSTANCE_CLI, "Vmcu=");
    AT_reply_add_integer(AT_INSTANCE_CLI, generic_s32, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "mV");
    AT_send_reply(AT_INSTANCE_CLI);
    // MCU temperature.
    analog_status = ANALOG_convert_channel(ANALOG_CHANNEL_TMCU_DEGREES, &generic_s32);
    _CLI_check_driver_status(analog_status, ANALOG_SUCCESS, ERROR_BASE_ANALOG);
    AT_reply_add_string(AT_INSTANCE_CLI, "Tmcu=");
    AT_reply_add_integer(AT_INSTANCE_CLI, generic_s32, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "dC");
    AT_send_reply(AT_INSTANCE_CLI);
    // Source voltage.
    AT_reply_add_string(AT_INSTANCE_CLI, "Vsrc=");
    analog_status = ANALOG_convert_channel(ANALOG_CHANNEL_VSRC_MV, &generic_s32);
    _CLI_check_driver_status(analog_status, ANALOG_SUCCESS, ERROR_BASE_ANALOG);
    AT_reply_add_integer(AT_INSTANCE_CLI, generic_s32, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "mV");
    AT_send_reply(AT_INSTANCE_CLI);
    // Supercap voltage.
    AT_reply_add_string(AT_INSTANCE_CLI, "Vstr=");
    analog_status = ANALOG_convert_channel(ANALOG_CHANNEL_VSTR_MV, &generic_s32);
    _CLI_check_driver_status(analog_status, ANALOG_SUCCESS, ERROR_BASE_ANALOG);
    AT_reply_add_integer(AT_INSTANCE_CLI, generic_s32, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "mV");
    AT_send_reply(AT_INSTANCE_CLI);
    // Turn analog front-end off.
    power_status = POWER_disable(POWER_DOMAIN_ANALOG);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    goto end;
errors:
    POWER_disable(POWER_DOMAIN_ANALOG);
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_SENSORS
/*******************************************************************/
static AT_status_t _CLI_ths_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    POWER_status_t power_status = POWER_SUCCESS;
    SHT3X_status_t sht3x_status = SHT3X_SUCCESS;
    int32_t temperature_degrees = 0;
    int32_t humidity_percent = 0;
    // Turn digital sensors on.
    power_status = POWER_enable(POWER_DOMAIN_SENSORS, LPTIM_DELAY_MODE_SLEEP);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Perform measurements.
    sht3x_status = SHT3X_get_temperature_humidity(I2C_ADDRESS_SHT30, &temperature_degrees, &humidity_percent);
    _CLI_check_driver_status(sht3x_status, SHT3X_SUCCESS, ERROR_BASE_SHT30);
    // Turn digital sensors off.
    power_status = POWER_disable(POWER_DOMAIN_SENSORS);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Read and print data.
    // Temperature.
    AT_reply_add_string(AT_INSTANCE_CLI, "T=");
    AT_reply_add_integer(AT_INSTANCE_CLI, temperature_degrees, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "dC");
    AT_send_reply(AT_INSTANCE_CLI);
    // Humidity.
    AT_reply_add_string(AT_INSTANCE_CLI, "H=");
    AT_reply_add_integer(AT_INSTANCE_CLI, humidity_percent, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "%");
    AT_send_reply(AT_INSTANCE_CLI);
    goto end;
errors:
    POWER_disable(POWER_DOMAIN_SENSORS);
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_SENSORS
/*******************************************************************/
static AT_status_t _CLI_acc_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    POWER_status_t power_status = POWER_SUCCESS;
    MMA865XFC_status_t mma865xfc_status = MMA865XFC_SUCCESS;
    uint8_t chip_id = 0;
    // Turn digital sensors on.
    power_status = POWER_enable(POWER_DOMAIN_SENSORS, LPTIM_DELAY_MODE_SLEEP);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Perform measurements.
    mma865xfc_status = MMA865XFC_get_id(I2C_ADDRESS_MMA8653FC, &chip_id);
    _CLI_check_driver_status(mma865xfc_status, MMA865XFC_SUCCESS, ERROR_BASE_MMA8653FC);
    // Turn digital sensors off.
    power_status = POWER_disable(POWER_DOMAIN_SENSORS);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Read and print data.
    // Temperature.
    AT_reply_add_string(AT_INSTANCE_CLI, "MMA8653FC chip_id=");
    AT_reply_add_integer(AT_INSTANCE_CLI, chip_id, STRING_FORMAT_HEXADECIMAL, 1);
    AT_send_reply(AT_INSTANCE_CLI);
    goto end;
errors:
    POWER_disable(POWER_DOMAIN_SENSORS);
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_GPS
/*******************************************************************/
static AT_status_t _CLI_gps_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    POWER_status_t power_status = POWER_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    GPS_status_t gps_status = GPS_SUCCESS;
    GPS_position_t gps_position;
    GPS_acquisition_status_t acquisition_status = GPS_ACQUISITION_ERROR_LAST;
    int32_t timeout_seconds = 0;
    uint32_t fix_duration_seconds = 0;
    // Read timeout parameter.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &timeout_seconds);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Turn analog front-end to monitor storage element voltage.
    power_status = POWER_enable(POWER_DOMAIN_ANALOG, LPTIM_DELAY_MODE_SLEEP);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Turn GPS on.
    power_status = POWER_enable(POWER_DOMAIN_GPS, LPTIM_DELAY_MODE_SLEEP);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Perform time acquisition.
    gps_status = GPS_get_position(&gps_position, 0, (uint32_t) timeout_seconds, &fix_duration_seconds, &acquisition_status);
    _CLI_check_driver_status(gps_status, GPS_SUCCESS, ERROR_BASE_GPS);
    // Turn GPS off.
    power_status = POWER_disable(POWER_DOMAIN_GPS);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    power_status = POWER_disable(POWER_DOMAIN_ANALOG);
    _CLI_check_driver_status(power_status, POWER_SUCCESS, ERROR_BASE_POWER);
    // Check status.
    if (acquisition_status == GPS_ACQUISITION_SUCCESS) {
        // Latitude.
        AT_reply_add_string(AT_INSTANCE_CLI, "Lat=");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.lat_degrees), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "d");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.lat_minutes), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "'");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.lat_seconds), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "''");
        AT_reply_add_string(AT_INSTANCE_CLI, ((gps_position.lat_north_flag) == 0) ? "S" : "N");
        // Longitude.
        AT_reply_add_string(AT_INSTANCE_CLI, " Long=");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.long_degrees), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "d");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.long_minutes), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "'");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.long_seconds), STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "''");
        AT_reply_add_string(AT_INSTANCE_CLI, ((gps_position.long_east_flag) == 0) ? "W" : "E");
        // Altitude.
        AT_reply_add_string(AT_INSTANCE_CLI, " Alt=");
        AT_reply_add_integer(AT_INSTANCE_CLI, (gps_position.altitude), STRING_FORMAT_DECIMAL, 0);
        // Fix duration.
        AT_reply_add_string(AT_INSTANCE_CLI, "m Fix=");
        AT_reply_add_integer(AT_INSTANCE_CLI, fix_duration_seconds, STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "s");
    }
    else {
        AT_reply_add_string(AT_INSTANCE_CLI, "GPS timeout");
    }
    AT_send_reply(AT_INSTANCE_CLI);
    goto end;
errors:
    POWER_disable(POWER_DOMAIN_GPS);
    POWER_disable(POWER_DOMAIN_ANALOG);
end:
    return status;
}
#endif

#if (defined CLI_COMMAND_SIGFOX_EP_LIB) && (defined BIDIRECTIONAL)
/*******************************************************************/
static AT_status_t _CLI_print_dl_payload(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    SIGFOX_EP_API_status_t sigfox_ep_api_status = SIGFOX_EP_API_SUCCESS;
    sfx_u8 dl_payload[SIGFOX_DL_PAYLOAD_SIZE_BYTES];
    sfx_s16 dl_rssi_dbm = 0;
    // Read downlink payload.
    sigfox_ep_api_status = SIGFOX_EP_API_get_dl_payload(dl_payload, SIGFOX_DL_PAYLOAD_SIZE_BYTES, &dl_rssi_dbm);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    // Print downlink payload.
    CLI_print_dl_payload(dl_payload, SIGFOX_DL_PAYLOAD_SIZE_BYTES, dl_rssi_dbm);
errors:
    return status;
}
#endif

#if (defined CLI_COMMAND_SIGFOX_EP_LIB) && (defined CONTROL_KEEP_ALIVE_MESSAGE)
/*******************************************************************/
static AT_status_t _CLI_so_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    SIGFOX_EP_API_status_t sigfox_ep_api_status = SIGFOX_EP_API_SUCCESS;
    SIGFOX_EP_API_config_t lib_config;
    SIGFOX_EP_API_control_message_t control_message;
    // Library configuration.
    lib_config.rc = &SIGFOX_RC1;
    // Default control message parameters.
    control_message.type = SIGFOX_CONTROL_MESSAGE_TYPE_KEEP_ALIVE;
    control_message.common_parameters.number_of_frames = 3;
    control_message.common_parameters.ul_bit_rate = SIGFOX_UL_BIT_RATE_100BPS;
    // Open library.
    sigfox_ep_api_status = SIGFOX_EP_API_open(&lib_config);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    // Send application message.
    sigfox_ep_api_status = SIGFOX_EP_API_send_control_message(&control_message);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    // Close library.
    sigfox_ep_api_status = SIGFOX_EP_API_close();
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    goto end;
errors:
    SIGFOX_EP_API_close();
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_SIGFOX_EP_LIB
/*******************************************************************/
static AT_status_t _CLI_sb_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    SIGFOX_EP_API_status_t sigfox_ep_api_status = SIGFOX_EP_API_SUCCESS;
    SIGFOX_EP_API_config_t lib_config;
    SIGFOX_EP_API_application_message_t application_message;
    int32_t ul_bit = 0;
    int32_t bidir_flag = 0;
    // Library configuration.
    lib_config.rc = &SIGFOX_RC1;
    // Default application message parameters.
    application_message.common_parameters.number_of_frames = 3;
    application_message.common_parameters.ul_bit_rate = SIGFOX_UL_BIT_RATE_100BPS;
    application_message.ul_payload = SFX_NULL;
    application_message.ul_payload_size_bytes = 0;
    // First try with 2 parameters.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, CLI_CHAR_SEPARATOR, &ul_bit);
    if (parser_status == PARSER_SUCCESS) {
        // Try parsing downlink request parameter.
        parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &bidir_flag);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update parameters.
        application_message.type = (SIGFOX_APPLICATION_MESSAGE_TYPE_BIT0 + ul_bit);
#ifdef BIDIRECTIONAL
        application_message.bidirectional_flag = bidir_flag;
#endif
    }
    else {
        // Try with 1 parameter.
        parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &ul_bit);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update parameters.
        application_message.type = (SIGFOX_APPLICATION_MESSAGE_TYPE_BIT0 + ul_bit);
#ifdef BIDIRECTIONAL
        application_message.bidirectional_flag = 0;
#endif
    }
    // Open library.
    sigfox_ep_api_status = SIGFOX_EP_API_open(&lib_config);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    // Send application message.
    sigfox_ep_api_status = SIGFOX_EP_API_send_application_message(&application_message);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
#ifdef BIDIRECTIONAL
    // Read and print DL payload if needed.
    if ((application_message.bidirectional_flag) == SFX_TRUE) {
        status = _CLI_print_dl_payload();
        if (status != AT_SUCCESS) goto errors;
    }
#endif
    // Close library.
    sigfox_ep_api_status = SIGFOX_EP_API_close();
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    goto end;
errors:
    SIGFOX_EP_API_close();
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_SIGFOX_EP_LIB
/*******************************************************************/
static AT_status_t _CLI_sf_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    SIGFOX_EP_API_status_t sigfox_ep_api_status = SIGFOX_EP_API_SUCCESS;
    SIGFOX_EP_API_config_t lib_config;
    SIGFOX_EP_API_application_message_t application_message;
    sfx_u8 data[SIGFOX_UL_PAYLOAD_MAX_SIZE_BYTES];
    uint32_t extracted_size = 0;
    int32_t bidir_flag = 0;
    // Library configuration.
    lib_config.rc = &SIGFOX_RC1;
    // Default application message parameters.
    application_message.common_parameters.number_of_frames = 3;
    application_message.common_parameters.ul_bit_rate = SIGFOX_UL_BIT_RATE_100BPS;
    application_message.type = SIGFOX_APPLICATION_MESSAGE_TYPE_BYTE_ARRAY;
#ifdef BIDIRECTIONAL
    application_message.bidirectional_flag = 0;
#endif
    application_message.ul_payload = SFX_NULL;
    application_message.ul_payload_size_bytes = 0;
    // First try with 2 parameters.
    parser_status = PARSER_get_byte_array(cli_ctx.at_parser_ptr, CLI_CHAR_SEPARATOR, SIGFOX_UL_PAYLOAD_MAX_SIZE_BYTES, 0, data, &extracted_size);
    if (parser_status == PARSER_SUCCESS) {
        // Try parsing downlink request parameter.
        parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &bidir_flag);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update parameters.
        application_message.ul_payload = (sfx_u8*) data;
        application_message.ul_payload_size_bytes = extracted_size;
#ifdef BIDIRECTIONAL
        application_message.bidirectional_flag = bidir_flag;
#endif
    }
    else {
        // Try with 1 parameter.
        parser_status = PARSER_get_byte_array(cli_ctx.at_parser_ptr, STRING_CHAR_NULL, SIGFOX_UL_PAYLOAD_MAX_SIZE_BYTES, 0, data, &extracted_size);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update parameters.
        application_message.ul_payload = (sfx_u8*) data;
        application_message.ul_payload_size_bytes = extracted_size;
    }
    // Open library.
    sigfox_ep_api_status = SIGFOX_EP_API_open(&lib_config);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    // Send application message.
    sigfox_ep_api_status = SIGFOX_EP_API_send_application_message(&application_message);
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
#ifdef BIDIRECTIONAL
    // Read and print DL payload if needed.
    if ((application_message.bidirectional_flag) == SFX_TRUE) {
        status = _CLI_print_dl_payload();
        if (status != AT_SUCCESS) goto errors;
    }
#endif
    // Close library.
    sigfox_ep_api_status = SIGFOX_EP_API_close();
    _CLI_check_driver_status(sigfox_ep_api_status, SIGFOX_EP_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_SIGFOX_EP_API * ERROR_BASE_STEP)));
    goto end;
errors:
    SIGFOX_EP_API_close();
end:
    return status;
}
#endif

#if (defined CLI_COMMAND_SIGFOX_EP_ADDON_RFP)
/*******************************************************************/
static AT_status_t _CLI_tm_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    SIGFOX_EP_ADDON_RFP_API_status_t sigfox_ep_addon_rfp_status = SIGFOX_EP_ADDON_RFP_API_SUCCESS;
    SIGFOX_EP_ADDON_RFP_API_config_t addon_config;
    SIGFOX_EP_ADDON_RFP_API_test_mode_t test_mode;
    int32_t bit_rate_index = 0;
    int32_t test_mode_reference = 0;
    // Read bit rate.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, CLI_CHAR_SEPARATOR, &bit_rate_index);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Read test mode parameter.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &test_mode_reference);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Addon configuration.
    addon_config.rc = &SIGFOX_RC1;
    // Test mode parameters.
    test_mode.test_mode_reference = (SIGFOX_EP_ADDON_RFP_API_test_mode_reference_t) test_mode_reference;
    test_mode.ul_bit_rate = (SIGFOX_ul_bit_rate_t) bit_rate_index;
    // Open addon.
    sigfox_ep_addon_rfp_status = SIGFOX_EP_ADDON_RFP_API_open(&addon_config);
    _CLI_check_driver_status(sigfox_ep_addon_rfp_status, SIGFOX_EP_ADDON_RFP_API_SUCCESS, ERROR_BASE_SIGFOX_EP_ADDON_RFP);
    // Call test mode function.
    sigfox_ep_addon_rfp_status = SIGFOX_EP_ADDON_RFP_API_test_mode(&test_mode);
    _CLI_check_driver_status(sigfox_ep_addon_rfp_status, SIGFOX_EP_ADDON_RFP_API_SUCCESS, ERROR_BASE_SIGFOX_EP_ADDON_RFP);
    // Close addon.
    sigfox_ep_addon_rfp_status = SIGFOX_EP_ADDON_RFP_API_close();
    _CLI_check_driver_status(sigfox_ep_addon_rfp_status, SIGFOX_EP_ADDON_RFP_API_SUCCESS, ERROR_BASE_SIGFOX_EP_ADDON_RFP);
    goto end;
errors:
    SIGFOX_EP_ADDON_RFP_API_close();
end:
    return status;
}
#endif

#ifdef CLI_COMMAND_CW
/*******************************************************************/
static AT_status_t _CLI_cw_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    RF_API_status_t rf_api_status = RF_API_SUCCESS;
    RF_API_radio_parameters_t radio_params;
    int32_t enable = 0;
    int32_t frequency_hz = 0;
    int32_t power_dbm = 0;
    // Set common radio parameters.
    radio_params.rf_mode = RF_API_MODE_TX;
    radio_params.modulation = RF_API_MODULATION_NONE;
    radio_params.bit_rate_bps = 0;
#ifdef BIDIRECTIONAL
    radio_params.deviation_hz = 0;
#endif
    // Read frequency parameter.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, CLI_CHAR_SEPARATOR, &frequency_hz);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Update radio configuration.
    radio_params.frequency_hz = (sfx_u32) frequency_hz;
    // First try with 3 parameters.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, CLI_CHAR_SEPARATOR, &enable);
    if (parser_status == PARSER_SUCCESS) {
        // There is a third parameter, try to parse power.
        parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &power_dbm);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update radio configuration.
        radio_params.tx_power_dbm_eirp = (sfx_s8) power_dbm;
    }
    else {
        // Power is not given, try to parse enable as last parameter.
        parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &enable);
        PARSER_exit_error(AT_ERROR_BASE_PARSER);
        // Update radio configuration.
        radio_params.tx_power_dbm_eirp = TX_POWER_DBM_EIRP;
    }
    // Stop CW.
    rf_api_status = RF_API_de_init();
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    rf_api_status = RF_API_sleep();
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    // Restart if required.
    if (enable != 0) {
        // Init radio.
        rf_api_status = RF_API_wake_up();
        _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
        rf_api_status = RF_API_init(&radio_params);
        _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
        // Start CW.
        rf_api_status = RF_API_start_continuous_wave();
        _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
        AT_reply_add_string(AT_INSTANCE_CLI, "CW running...");
        AT_send_reply(AT_INSTANCE_CLI);
    }
    goto end;
errors:
    RF_API_de_init();
    RF_API_sleep();
end:
    return status;
}
#endif

#if (defined CLI_COMMAND_RSSI) && (defined BIDIRECTIONAL)
/*******************************************************************/
static AT_status_t _CLI_rssi_callback(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    PARSER_status_t parser_status = PARSER_SUCCESS;
    RF_API_status_t rf_api_status = RF_API_SUCCESS;
    S2LP_status_t s2lp_status = S2LP_SUCCESS;
    LPTIM_status_t lptim_status = LPTIM_SUCCESS;
    RF_API_radio_parameters_t radio_params;
    int32_t frequency_hz = 0;
    int32_t duration_seconds = 0;
    int16_t rssi_dbm = 0;
    uint32_t report_loop = 0;
    // Read frequency parameter.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, CLI_CHAR_SEPARATOR, &frequency_hz);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Read duration parameters.
    parser_status = PARSER_get_parameter(cli_ctx.at_parser_ptr, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &duration_seconds);
    PARSER_exit_error(AT_ERROR_BASE_PARSER);
    // Radio configuration.
    radio_params.rf_mode = RF_API_MODE_RX;
    radio_params.frequency_hz = (sfx_u32) frequency_hz;
    radio_params.modulation = RF_API_MODULATION_NONE;
    radio_params.bit_rate_bps = 0;
    radio_params.tx_power_dbm_eirp = TX_POWER_DBM_EIRP;
    radio_params.deviation_hz = 0;
    // Init radio.
    rf_api_status = RF_API_wake_up();
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    rf_api_status = RF_API_init(&radio_params);
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    // Start continuous listening.
    s2lp_status = S2LP_send_command(S2LP_COMMAND_READY);
    _CLI_check_driver_status(s2lp_status, S2LP_SUCCESS, ERROR_BASE_S2LP);
    s2lp_status = S2LP_wait_for_state(S2LP_STATE_READY);
    _CLI_check_driver_status(s2lp_status, S2LP_SUCCESS, ERROR_BASE_S2LP);
    s2lp_status = S2LP_send_command(S2LP_COMMAND_RX);
    _CLI_check_driver_status(s2lp_status, S2LP_SUCCESS, ERROR_BASE_S2LP);
    // Measurement loop.
    while (report_loop < ((uint32_t) ((duration_seconds * 1000) / CLI_RSSI_REPORT_PERIOD_MS))) {
        // Read RSSI.
        s2lp_status = S2LP_get_rssi(S2LP_RSSI_TYPE_RUN, &rssi_dbm);
        _CLI_check_driver_status(s2lp_status, S2LP_SUCCESS, ERROR_BASE_S2LP);
        // Print RSSI.
        AT_reply_add_string(AT_INSTANCE_CLI, "RSSI=");
        AT_reply_add_integer(AT_INSTANCE_CLI, rssi_dbm, STRING_FORMAT_DECIMAL, 0);
        AT_reply_add_string(AT_INSTANCE_CLI, "dBm");
        AT_send_reply(AT_INSTANCE_CLI);
        // Report delay.
        lptim_status = LPTIM_delay_milliseconds(CLI_RSSI_REPORT_PERIOD_MS, LPTIM_DELAY_MODE_ACTIVE);
        _CLI_check_driver_status(lptim_status, LPTIM_SUCCESS, ERROR_BASE_LPTIM);
        report_loop++;
        // Reload watchdog.
        IWDG_reload();
    }
    // Turn radio off.
    rf_api_status = RF_API_de_init();
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    rf_api_status = RF_API_sleep();
    _CLI_check_driver_status(rf_api_status, RF_API_SUCCESS, (ERROR_BASE_SIGFOX_EP_LIB + (SIGFOX_ERROR_SOURCE_RF_API * ERROR_BASE_STEP)));
    goto end;
errors:
    RF_API_de_init();
    RF_API_sleep();
end:
    return status;
}
#endif

/*** CLI functions ***/

/*******************************************************************/
CLI_status_t CLI_init(void) {
    // Local variables.
    CLI_status_t status = CLI_SUCCESS;
    AT_status_t at_status = AT_SUCCESS;
    uint8_t idx = 0;
    // Init context.
    cli_ctx.at_process_flag = 0;
    cli_ctx.at_parser_ptr = NULL;
    // Init AT driver.
    at_status = AT_init(AT_INSTANCE_CLI, TERMINAL_INSTANCE_CLI, &_CLI_at_process_callback, &(cli_ctx.at_parser_ptr));
    AT_exit_error(CLI_ERROR_BASE_AT);
    // Register commands.
    for (idx = 0; idx < (sizeof(CLI_COMMANDS_LIST) / sizeof(AT_command_t)); idx++) {
        at_status = AT_register_command(AT_INSTANCE_CLI, &(CLI_COMMANDS_LIST[idx]));
        AT_exit_error(CLI_ERROR_BASE_AT);
    }
errors:
    return status;
}

/*******************************************************************/
CLI_status_t CLI_de_init(void) {
    // Local variables.
    CLI_status_t status = CLI_SUCCESS;
    AT_status_t at_status = AT_SUCCESS;
    uint8_t idx = 0;
    // Unregister commands.
    for (idx = 0; idx < (sizeof(CLI_COMMANDS_LIST) / sizeof(AT_command_t)); idx++) {
        at_status = AT_unregister_command(AT_INSTANCE_CLI, &(CLI_COMMANDS_LIST[idx]));
        AT_exit_error(CLI_ERROR_BASE_AT);
    }
    // Release AT driver.
    at_status = AT_de_init(AT_INSTANCE_CLI);
    AT_exit_error(CLI_ERROR_BASE_AT);
errors:
    return status;
}

/*******************************************************************/
CLI_status_t CLI_process(void) {
    // Local variables.
    CLI_status_t status = CLI_SUCCESS;
    AT_status_t at_status = AT_SUCCESS;
    // Check process flag.
    if (cli_ctx.at_process_flag != 0) {
        // Clear flag.
        cli_ctx.at_process_flag = 0;
        // Process AT driver.
        at_status = AT_process(AT_INSTANCE_CLI);
        AT_exit_error(CLI_ERROR_BASE_AT);
    }
errors:
    return status;
}

/*******************************************************************/
void CLI_print_dl_payload(sfx_u8* dl_payload, sfx_u8 dl_payload_size, sfx_s16 rssi_dbm) {
    // Local variables.
    uint8_t idx = 0;
    // Print DL payload.
    AT_reply_add_string(AT_INSTANCE_CLI, "+RX=");
    for (idx = 0; idx < dl_payload_size; idx++) {
        AT_reply_add_integer(AT_INSTANCE_CLI, dl_payload[idx], STRING_FORMAT_HEXADECIMAL, 0);
    }
    AT_reply_add_string(AT_INSTANCE_CLI, " (RSSI=");
    AT_reply_add_integer(AT_INSTANCE_CLI, rssi_dbm, STRING_FORMAT_DECIMAL, 0);
    AT_reply_add_string(AT_INSTANCE_CLI, "dBm)");
    AT_send_reply(AT_INSTANCE_CLI);
}

#endif /* TKFX_MODE_CLI */
