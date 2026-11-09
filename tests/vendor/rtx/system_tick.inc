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
 * Title:       System functions
 *
 * -----------------------------------------------------------------------------
 */

void osRtxTick_Handler (void) {
  os_thread_t *thread;

  OS_Tick_AcknowledgeIRQ();
  osRtxInfo.kernel.tick++;

  // Process Timers
  if (osRtxInfo.timer.tick != NULL) {
    osRtxInfo.timer.tick();
  }

  // Process Thread Delays
  osRtxThreadDelayTick();

  osRtxThreadDispatch(NULL);

  // Check Round Robin timeout
  if (osRtxInfo.thread.robin.timeout != 0U) {
    if (osRtxInfo.thread.robin.thread != osRtxInfo.thread.run.next) {
      // Reset Round Robin
      osRtxInfo.thread.robin.thread = osRtxInfo.thread.run.next;
      osRtxInfo.thread.robin.tick   = osRtxInfo.thread.robin.timeout;
    } else {
      if (osRtxInfo.thread.robin.tick != 0U) {
        osRtxInfo.thread.robin.tick--;
      }
      if (osRtxInfo.thread.robin.tick == 0U) {
        // Round Robin Timeout
        if (osRtxKernelGetState() == osRtxKernelRunning) {
          thread = osRtxInfo.thread.ready.thread_list;
          if ((thread != NULL) && (thread->priority == osRtxInfo.thread.robin.thread->priority)) {
            osRtxThreadListRemove(thread);
            osRtxThreadReadyPut(osRtxInfo.thread.robin.thread);
            EvrRtxThreadPreempted(osRtxInfo.thread.robin.thread);
            osRtxThreadSwitch(thread);
            osRtxInfo.thread.robin.thread = thread;
            osRtxInfo.thread.robin.tick   = osRtxInfo.thread.robin.timeout;
          }
        }
      }
    }
  }
}
