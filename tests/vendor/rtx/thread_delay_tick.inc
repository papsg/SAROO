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

void osRtxThreadDelayTick (void) {
  os_thread_t *thread;

  thread = osRtxInfo.thread.delay_list;
  if (thread == NULL) {
    //lint -e{904} "Return statement before end of function" [MISRA Note 1]
    return;
  }

  thread->delay--;

  if (thread->delay == 0U) {
    do {
      switch (thread->state) {
        case osRtxThreadWaitingDelay:
          EvrRtxDelayCompleted(thread);
          break;
        case osRtxThreadWaitingThreadFlags:
          EvrRtxThreadFlagsWaitTimeout(thread);
          break;
        case osRtxThreadWaitingEventFlags:
          EvrRtxEventFlagsWaitTimeout((osEventFlagsId_t)osRtxThreadListRoot(thread));
          break;
        case osRtxThreadWaitingMutex:
          EvrRtxMutexAcquireTimeout((osMutexId_t)osRtxThreadListRoot(thread));
          break;
        case osRtxThreadWaitingSemaphore:
          EvrRtxSemaphoreAcquireTimeout((osSemaphoreId_t)osRtxThreadListRoot(thread));
          break;
        case osRtxThreadWaitingMemoryPool:
          EvrRtxMemoryPoolAllocTimeout((osMemoryPoolId_t)osRtxThreadListRoot(thread));
          break;
        case osRtxThreadWaitingMessageGet:
          EvrRtxMessageQueueGetTimeout((osMessageQueueId_t)osRtxThreadListRoot(thread));
          break;
        case osRtxThreadWaitingMessagePut:
          EvrRtxMessageQueuePutTimeout((osMessageQueueId_t)osRtxThreadListRoot(thread));
          break;
        default:
          // Invalid
          break;
      }
      EvrRtxThreadUnblocked(thread, (osRtxThreadRegPtr(thread))[0]);
      osRtxThreadListRemove(thread);
      osRtxThreadReadyPut(thread);
      thread = thread->delay_next;
    } while ((thread != NULL) && (thread->delay == 0U));
    if (thread != NULL) {
      thread->delay_prev = NULL;
    }
    osRtxInfo.thread.delay_list = thread;
  }
}
