/*
 * Copyright (c) 2013-2019 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 *
 * Project:     CMSIS-RTOS RTX
 * Title:       Thread functions
 *
 * -----------------------------------------------------------------------------
 */

bool_t osRtxThreadStartup (void) {
  bool_t ret = TRUE;

  // Create Idle Thread
  osRtxInfo.thread.idle = osRtxThreadId(
    svcRtxThreadNew(osRtxIdleThread, NULL, osRtxConfig.idle_thread_attr)
  );

  // Create Timer Thread
  if (osRtxConfig.timer_mq_mcnt != 0U) {
    osRtxInfo.timer.thread = osRtxThreadId(
      svcRtxThreadNew(osRtxTimerThread, NULL, osRtxConfig.timer_thread_attr)
    );
    if (osRtxInfo.timer.thread == NULL) {
      ret = FALSE;
    }
  }

  return ret;
}
