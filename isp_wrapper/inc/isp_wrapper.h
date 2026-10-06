/**
 ******************************************************************************
 * @file    isp_wrapper.h
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef __ISP_WRAPPER_H__
#define __ISP_WRAPPER_H__

#include "isp_api.h"

/**
 * @brief Get the ISP middleware handle initialized by the wrapper.
 *
 * @return Pointer to the ISP middleware handle.
 */
ISP_HandleTypeDef *ISP_Wrapper_GetHandler(void);

#endif /* __ISP_WRAPPER_H__ */
