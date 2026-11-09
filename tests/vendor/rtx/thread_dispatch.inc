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

void osRtxThreadDispatch (os_thread_t *thread) {
  uint8_t      kernel_state;
  os_thread_t *thread_running;
  os_thread_t *thread_ready;

  kernel_state   = osRtxKernelGetState();
  thread_running = osRtxThreadGetRunning();

  if (thread == NULL) {
    thread_ready = osRtxInfo.thread.ready.thread_list;
    if ((kernel_state == osRtxKernelRunning) &&
        (thread_ready != NULL) &&
        (thread_ready->priority > thread_running->priority)) {
      // Preempt running Thread
      osRtxThreadListRemove(thread_ready);
      osRtxThreadBlock(thread_running);
      osRtxThreadSwitch(thread_ready);
    }
  } else {
    if ((kernel_state == osRtxKernelRunning) &&
        (thread->priority > thread_running->priority)) {
      // Preempt running Thread
      osRtxThreadBlock(thread_running);
      osRtxThreadSwitch(thread);
    } else {
      // Put Thread into Ready list
      osRtxThreadReadyPut(thread);
    }
  }
}
