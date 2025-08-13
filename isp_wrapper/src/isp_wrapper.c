/**
 ******************************************************************************
 * @file    isp_wrapper.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "stm32n6xx_hal.h"

#include <assert.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "isp_api.h"

#include "imx335_isp_param_conf.h"
#include "zephyr/drivers/video-controls.h"
#include "zephyr/drivers/video.h"
#include "zephyr/drivers/video/stm32_dcmipp.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(isp_wrapper);

static const struct device *sensor_i;
static ISP_HandleTypeDef isp_i;
static struct k_thread isp_thread;
static K_THREAD_STACK_DEFINE(isp_thread_stack, 4096);
static struct k_sem isp_sem;

#define IMX335_BAYER_PATTERN      0
#define IMX335_COLOR_DEPTH        10
#define IMX335_WIDTH              2592
#define IMX335_HEIGHT             1944
#define IMX335_GAIN_MIN           (0 * 1000)
#define IMX335_GAIN_MAX           (72 * 1000)
#define IMX335_EXPOSURE_MIN       8
#define IMX335_EXPOSURE_MAX       33266

#define IMX335_1H_PERIOD_USEC (1000000.0F / 4500 / 30)

static ISP_StatusTypeDef isp_GetSensorInfo(uint32_t Instance, ISP_SensorInfoTypeDef *info)
{
  info->bayer_pattern = IMX335_BAYER_PATTERN;
  info->color_depth = IMX335_COLOR_DEPTH;
  info->width = IMX335_WIDTH;
  info->height = IMX335_HEIGHT;
  info->gain_min = IMX335_GAIN_MIN;
  info->gain_max = IMX335_GAIN_MAX;
  info->exposure_min = IMX335_EXPOSURE_MIN;
  info->exposure_max = IMX335_EXPOSURE_MAX;

  return 0;
}

static ISP_StatusTypeDef isp_SetSensorGain(uint32_t Instance, int32_t Gain)
{
  struct video_control ctrl;
  int ret;

  ctrl.id = VIDEO_CID_ANALOGUE_GAIN;
  ctrl.val = Gain;
  ret = video_set_ctrl(sensor_i, &ctrl);

  return ret;
}

static ISP_StatusTypeDef isp_GetSensorGain(uint32_t Instance, int32_t *Gain)
{
  struct video_control ctrl;
  int ret;

  ctrl.id = VIDEO_CID_ANALOGUE_GAIN;
  ret = video_get_ctrl(sensor_i, &ctrl);
  if (ret)
    return ret;

  *Gain = ctrl.val;

  return 0;
}

static ISP_StatusTypeDef isp_SetSensorExposure(uint32_t Instance, int32_t Exposure)
{
  struct video_control ctrl;
  int ret;

  ctrl.id = VIDEO_CID_EXPOSURE;
  ctrl.val = Exposure / IMX335_1H_PERIOD_USEC;
  ret = video_set_ctrl(sensor_i, &ctrl);

  return ret;
}

static ISP_StatusTypeDef isp_GetSensorExposure(uint32_t Instance, int32_t *Exposure)
{
  struct video_control ctrl;
  int ret;

  ctrl.id = VIDEO_CID_EXPOSURE;
  ret = video_get_ctrl(sensor_i, &ctrl);
  if (ret)
    return ret;

  *Exposure = ctrl.val * IMX335_1H_PERIOD_USEC;

  return 0;
}

static ISP_AppliHelpersTypeDef isp_helpers = {
  .GetSensorInfo = isp_GetSensorInfo,
  .SetSensorGain = isp_SetSensorGain,
  .GetSensorGain = isp_GetSensorGain,
  .SetSensorExposure = isp_SetSensorExposure,
  .GetSensorExposure = isp_GetSensorExposure,
};

static void isp_thread_ep(void *arg1, void *arg2, void *arg3)
{
  int res;

  while (1) {
    res = k_sem_take(&isp_sem, K_FOREVER);
    assert(res == 0);

    //LOG_INF("Run isp update");
    res = ISP_BackgroundProcess(&isp_i);
    assert(res == 0);
  }
}

int stm32_dcmipp_isp_init(DCMIPP_HandleTypeDef *hdcmipp, const struct device *sensor)
{
  k_tid_t isp_tid;
  int res;

  assert(sensor);

  sensor_i = sensor;
  res = ISP_Init(&isp_i, hdcmipp, 0, &isp_helpers, &ISP_IQParamCacheInit_IMX335);
  if (res)
    return -res;

  res = k_sem_init(&isp_sem, 0, 1);
  if (res)
    return -res;

  isp_tid = k_thread_create(&isp_thread, isp_thread_stack, K_THREAD_STACK_SIZEOF(isp_thread_stack), isp_thread_ep,
         NULL, NULL, NULL, 0, 0, K_NO_WAIT);
  if (!isp_tid)
    return -1;

  return 0;
}

int stm32_dcmipp_isp_start(void)
{
  ISP_StatusTypeDef res;

  res = ISP_Start(&isp_i);

  return res;
}

int stm32_dcmipp_isp_stop(void)
{
  assert(0);

  return 0;
}

void stm32_dcmipp_isp_vsync_update(DCMIPP_HandleTypeDef *hdcmipp, uint32_t Pipe)
{
  if (Pipe == 0) {
    ISP_IncDumpFrameId(&isp_i);
  } else if (Pipe == 1) {
    ISP_IncMainFrameId(&isp_i);
    ISP_GatherStatistics(&isp_i);
  } else if (Pipe == 2) {
    ISP_IncAncillaryFrameId(&isp_i);
  }

  if (Pipe != 1)
    return ;

  k_sem_give(&isp_sem);
}
