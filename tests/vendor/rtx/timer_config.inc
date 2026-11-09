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
 * Title:       RTX Library Configuration
 *
 * -----------------------------------------------------------------------------
 */

#if ((OS_TIMER_THREAD_STACK_SIZE != 0) && (OS_TIMER_CB_QUEUE != 0))
  &os_timer_thread_attr,
  &os_timer_mq_attr,
  (uint32_t)OS_TIMER_CB_QUEUE
#else
  NULL,
  NULL,
  0U
#endif
