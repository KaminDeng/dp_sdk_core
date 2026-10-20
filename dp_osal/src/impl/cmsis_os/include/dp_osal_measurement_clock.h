/** @brief 当前 CMSIS port 只有旧32位 hook，不能证明多圈/epoch或有界ISR采样。
 * 后续具有可靠宽源的 port 可在自己的头搜索路径提供私有适配。 */
#ifndef DP_OSAL_MEASUREMENT_CLOCK_H_
#define DP_OSAL_MEASUREMENT_CLOCK_H_
#include "detail/dp_osal_measurement_clock_fallback.h"
#endif
