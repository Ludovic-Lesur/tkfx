/*
 * power.h
 *
 *  Created on: 22 jul. 2023
 *      Author: Ludo
 */

#ifndef __POWER_H__
#define __POWER_H__

#include "adc.h"
#include "lptim.h"
#include "types.h"

/*** POWER macros ***/

#define POWER_ON_DELAY_MS_ANALOG	100
#define POWER_ON_DELAY_MS_GPS		1000
#define POWER_ON_DELAY_MS_DIGITAL	100
#define POWER_ON_DELAY_MS_SENSORS	100
#define POWER_ON_DELAY_MS_RADIO		200

/*** POWER structures ***/

/*!******************************************************************
 * \enum POWER_status_t
 * \brief POWER driver error codes.
 *******************************************************************/
typedef enum {
	POWER_SUCCESS,
	POWER_ERROR_NULL_PARAMETER,
	POWER_ERROR_DOMAIN,
	POWER_ERROR_BASE_ADC = 0x0100,
	POWER_ERROR_BASE_LPTIM = (POWER_ERROR_BASE_ADC + ADC_ERROR_BASE_LAST),
	POWER_ERROR_BASE_LAST = (POWER_ERROR_BASE_LPTIM + LPTIM_ERROR_BASE_LAST)
} POWER_status_t;

/*!******************************************************************
 * \enum POWER_domain_t
 * \brief Board external power domains list.
 *******************************************************************/
typedef enum {
	POWER_DOMAIN_ANALOG = 0,
	POWER_DOMAIN_SENSORS,
	POWER_DOMAIN_GPS,
	POWER_DOMAIN_RADIO,
	POWER_DOMAIN_LAST
} POWER_domain_t;

/*** POWER functions ***/

/*!******************************************************************
 * \fn void POWER_init(void)
 * \brief Init power control module.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void POWER_init(void);

/*!******************************************************************
 * \fn POWER_status_t POWER_enable(POWER_domain_t domain, LPTIM_delay_mode_t delay_mode)
 * \brief Turn power domain on.
 * \param[in]  	domain: Power domain to enable.
 * \param[in]	delay_mode: Power on delay waiting mode.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
POWER_status_t POWER_enable(POWER_domain_t domain, LPTIM_delay_mode_t delay_mode);

/*!******************************************************************
 * \fn POWER_status_t POWER_disable(POWER_domain_t domain)
 * \brief Turn power domain off.
 * \param[in]  	domain: Power domain to disable.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
POWER_status_t POWER_disable(POWER_domain_t domain);

/*!******************************************************************
 * \fn POWER_status_t POWER_get_state(POWER_domain_t domain, uint8_t* state
 * \brief Return the current state of a power domain.
 * \param[in]  	domain: Power domain to check.
 * \param[out] 	state: Pointer to the state.
 * \retval		Function execution status.
 *******************************************************************/
POWER_status_t POWER_get_state(POWER_domain_t domain, uint8_t* state);

/*******************************************************************/
#define POWER_check_status(error_base) { if (power_status != POWER_SUCCESS) { status = error_base + power_status; goto errors; } }

/*******************************************************************/
#define POWER_stack_error(void) { ERROR_stack_error(power_status, POWER_SUCCESS, ERROR_BASE_POWER); }

/*******************************************************************/
#define POWER_print_error(void) { ERROR_print_error(power_status, POWER_SUCCESS, ERROR_BASE_POWER); }

#endif /* __POWER_H__ */