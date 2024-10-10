/*$$$LICENCE_NORDIC_STANDARD<2015>$$$*/
#ifndef NRF_SD_DEF_H__
#define NRF_SD_DEF_H__

#include <stdint.h>
#include "nrf_soc.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef NRF_SOC_SD_PPI_CHANNELS_SD_ENABLED_MSK
#define SD_PPI_CHANNELS_USED    NRF_SOC_SD_PPI_CHANNELS_SD_ENABLED_MSK /**< PPI channels utilized by SotfDevice (not available to the application). */
#else
#define SD_PPI_CHANNELS_USED    0xFFFE0000uL                           /**< PPI channels utilized by SotfDevice (not available to the application). */
#endif // NRF_SOC_SD_PPI_CHANNELS_SD_ENABLED_MSK

#ifdef NRF_SOC_SD_PPI_GROUPS_SD_ENABLED_MSK
#define SD_PPI_GROUPS_USED      NRF_SOC_SD_PPI_GROUPS_SD_ENABLED_MSK /**< PPI groups utilized by SoftDevice (not available to the application). */
#else
#define SD_PPI_GROUPS_USED      0x0000000CuL                         /**< PPI groups utilized by SoftDevice (not available to the application). */
#endif // NRF_SOC_SD_PPI_GROUPS_SD_ENABLED_MSK

#define SD_TIMERS_USED          0x00000001uL /**< Timers used by SoftDevice. */
#define SD_SWI_USED             0x00000036uL /**< Software interrupts used by SoftDevice */


#ifdef __cplusplus
}
#endif

#endif /* NRF_SD_DEF_H__ */
