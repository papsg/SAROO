//! Offline, actual Cortex-M7 RTX API/exception contracts. No board operations.
use super::{capture, fail, mcu, new_build_output, sha, Result};
use serde_json::{json, Value};
use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
    time::{Duration, Instant},
};

fn pool_options(case: &str, flags: &[String]) -> Result<(u8, u8, bool)> {
    let mut atomic = None;
    let mut phase = None;
    for flag in flags {
        if ["--baseline", "--lto", "--archive"].contains(&flag.as_str()) {
            continue;
        }
        if [
            "message",
            "message-get",
            "event",
            "pool-free",
            "thread-flags",
        ]
        .contains(&case)
            && flag.starts_with("--scenario=")
        {
            continue;
        }
        if case == "syscounter" && flag.starts_with("--mode=") {
            continue;
        }
        if case == "event" && flag == "--ignore-event-refusal" {
            continue;
        }
        if case == "pool-free" && flag == "--old-free" {
            continue;
        }
        if case == "message-get" && flag.starts_with("--get-variant=") {
            continue;
        }
        if case == "message"
            && ["--old-space", "--omit-walk-invalidation", "--old-put"].contains(&flag.as_str())
        {
            continue;
        }
        if ![
            "pool",
            "pool-free",
            "message",
            "message-get",
            "event",
            "retire",
            "thread-flags",
        ]
        .contains(&case)
        {
            return fail(
                "Atomic options require a supported pool/message/event/retire/thread-flags case",
            );
        }
        if let Some(value) = flag.strip_prefix("--atomic=") {
            if atomic.is_some() || !["0", "1"].contains(&value) {
                return fail("Specify --atomic=0 or --atomic=1 once");
            }
            atomic = Some(value.parse::<u8>()?);
        } else if let Some(value) = flag.strip_prefix("--phase=") {
            if case != "pool" || phase.is_some() {
                return fail("Specify one pool phase");
            }
            phase = Some(match value {
                "release" => (0, true),
                "allocate" => (1, true),
                "masked" => (2, true),
                "after" => (0, false),
                _ => return fail("Unknown pool phase"),
            });
        } else {
            return fail("Unknown RTOS contract option");
        }
    }
    let (phase, inside) = phase.unwrap_or((0, true));
    Ok((atomic.unwrap_or(1), phase, inside))
}

fn scenario_options(case: &str, flags: &[String]) -> Result<(u8, bool, bool)> {
    let mut scenario = None;
    let mut old_space = false;
    let mut omit_invalidation = false;
    for flag in flags {
        if let Some(value) = flag.strip_prefix("--scenario=") {
            if scenario.is_some() {
                return fail("Specify one message scenario");
            }
            scenario = Some(match (case, value) {
                ("message", "reuse") | ("event", "single") => 0,
                ("message" | "event", "waiters") => 1,
                ("message", "walk") | ("event", "nested") => 2,
                ("message", "space") | ("event", "legacy") => 3,
                ("message", "admission") => 4,
                ("message", "last-slot") => 5,
                ("message", "receiver") => 6,
                ("message-get", "full") => 6,
                ("message-get", "batch") => 7,
                ("message-get", "gap") => 8,
                ("pool-free", "admission") => 0,
                ("pool-free", "last-slot") => 1,
                ("pool-free", "no-waiter") => 2,
                ("pool-free", "waiters") => 3,
                ("thread-flags", "normal") => 0,
                ("thread-flags", "paired") => 1,
                _ => return fail("Unknown RTOS contract scenario"),
            });
        } else if flag == "--old-space" {
            if old_space {
                return fail("Repeated old-space control");
            }
            old_space = true;
        } else if flag == "--omit-walk-invalidation" {
            if omit_invalidation {
                return fail("Repeated walk control");
            }
            omit_invalidation = true;
        }
    }
    let scenario = scenario.unwrap_or(if case == "message-get" { 6 } else { 0 });
    if (old_space && scenario != 3) || (omit_invalidation && scenario != 2) {
        return fail("Message negative control does not match its scenario");
    }
    Ok((scenario, old_space, omit_invalidation))
}

fn syscounter_mode(case: &str, flags: &[String]) -> Result<u8> {
    let mut mode = None;
    for flag in flags {
        if let Some(value) = flag.strip_prefix("--mode=") {
            if case != "syscounter" || mode.is_some() {
                return fail("Specify one syscounter mode only for --case syscounter");
            }
            let value = value.parse::<u8>()?;
            if value > 10 {
                return fail("Syscounter mode must be 0 through 10");
            }
            mode = Some(value);
        }
    }
    Ok(mode.unwrap_or(0))
}

fn instrument_syscounter(source: &Path, output: &Path, mode: u8) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let mut text = original.clone();
    let mut hooks = Vec::new();
    if mode == 3 {
        let marker = "  OS_Tick_AcknowledgeIRQ();\n";
        if text.matches(marker).count() != 1 {
            return fail("Missing unique SysTick acknowledgement boundary");
        }
        text = format!(
            "extern void syscounter_ack_probe(void);\n{}",
            text.replace(marker, &format!("{marker}  syscounter_ack_probe();\n"))
        );
        hooks.push("after-acknowledgement");
    }
    if mode == 4 || mode == 7 {
        let marker = "  OS_Tick_Disable();\n";
        if text.matches(marker).count() != 1 {
            return fail("Missing unique kernel-block tick boundary");
        }
        text = format!(
            "extern void syscounter_block_probe(void);\n{}",
            text.replace(marker, &format!("  syscounter_block_probe();\n{marker}"))
        );
        hooks.push("before-kernel-block");
    }
    if mode == 5 {
        let marker = "OS_Tick_GetOverflow()";
        if text.matches(marker).count() != 1 {
            return fail("Missing unique destructive overflow observation");
        }
        text = format!(
            "#include <stdint.h>\nextern uint32_t syscounter_observe_overflow(void);\n{}",
            text.replace(marker, "syscounter_observe_overflow()")
        );
        hooks.push("after-overflow-observation");
    }
    if mode == 7 {
        let marker = "  osRtxInfo.kernel.tick += sleep_ticks;";
        if text.matches(marker).count() != 1 {
            return fail("Missing unique sleep-tick publication boundary");
        }
        text = format!(
            "extern void syscounter_resume_probe(void);\n{}",
            text.replace(marker, &format!("{marker}\n  syscounter_resume_probe();"))
        );
        hooks.push("after-sleep-tick-publication");
    }
    if hooks.is_empty() {
        return fail("Syscounter mode needs no source instrument");
    }
    let output = output.join(format!("syscounter-{mode}-instrumented.c"));
    fs::write(&output, &text)?;
    Ok((
        output,
        json!({"source_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"test_hooks":hooks}),
    ))
}

fn instrument_retire(
    source: &Path,
    output: &Path,
    original_order: bool,
) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let mut text = original.clone();
    let start = text
        .find("static os_object_t *isr_queue_get (void) {")
        .ok_or("Missing ISR ring consumer")?;
    let end = text[start..]
        .find("//  ==== Library Functions ====")
        .ok_or("Missing ring consumer boundary")?
        + start;
    let mut body = text[start..end].to_owned();
    if original_order {
        let selected = "  if (osRtxInfo.isr_queue.cnt != 0U) {\n    /* PendSV alone consumes. Read the object before an interrupt producer\n     * can reuse this slot; the atomic count update orders these accesses. */\n    n = osRtxInfo.isr_queue.out;\n    ret = osRtxObject(osRtxInfo.isr_queue.data[n]);\n    if (++osRtxInfo.isr_queue.out == max) {\n      osRtxInfo.isr_queue.out = 0U;\n    }\n    (void)atomic_dec16_nz(&osRtxInfo.isr_queue.cnt);";
        if body.matches(selected).count() != 1 {
            return fail("Retirement baseline requires the corrected consumer");
        }
        body = body.replace(selected, "  if (atomic_dec16_nz(&osRtxInfo.isr_queue.cnt) != 0U) {\n    n = atomic_inc16_lim(&osRtxInfo.isr_queue.out, max);\n    ret = osRtxObject(osRtxInfo.isr_queue.data[n]);");
    }
    let decrement = "atomic_dec16_nz(&osRtxInfo.isr_queue.cnt)";
    let unmask = "  __enable_irq();\n#else";
    if body.matches(decrement).count() != 1 || body.matches(unmask).count() != 1 {
        return fail("Missing unique ring-retirement boundaries");
    }
    body = body.replace(decrement, "retire_test_dec(&osRtxInfo.isr_queue.cnt)");
    body = body.replace(
        unmask,
        "  __enable_irq();\n  queue_retire_probe(&osRtxInfo.isr_queue.cnt);\n#else",
    );
    text.replace_range(start..end, &body);
    text.insert_str(start, "extern void queue_retire_probe(uint16_t *);\n#if (EXCLUSIVE_ACCESS != 0)\n__STATIC_INLINE uint16_t retire_test_dec(uint16_t *mem) {\n  uint16_t value = atomic_dec16_nz(mem);\n  queue_retire_probe(mem);\n  return value;\n}\n#endif\n");
    let path = output.join("retire-test-provider.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"original_retirement_order":original_order,"schedule_hook":"after successful count decrement; after unmask for mode0"}),
    ))
}

fn header_matches(name: &str, original: &[u8], owned: &[u8]) -> bool {
    if original == owned {
        return true;
    }
    if name != "rtx_lib.h" {
        return false;
    }
    let marker = "extern void osRtxPostProcess    (os_object_t *object);\n";
    std::str::from_utf8(original).ok().is_some_and(|text| {
        text.matches(marker).count() == 1
            && text
                .replace(
                    marker,
                    &format!(
                        "{marker}extern uint32_t osRtxPostProcessTry (os_object_t *object);\n"
                    ),
                )
                .as_bytes()
                == owned
    })
}

fn instrument_event(
    source: &Path,
    output: &Path,
    ignore_refusal: bool,
) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let mut text = original.clone();
    let start = text
        .find("uint32_t isrRtxEventFlagsSet (")
        .ok_or("Missing event frontend")?;
    let end = text[start..]
        .find("uint32_t isrRtxEventFlagsWait (")
        .ok_or("Missing event frontend boundary")?
        + start;
    let mut body = text[start..end].to_owned();
    let guard = "  if (osRtxPostProcessTry(osRtxObject(ef)) == 0U) {\n    EvrRtxEventFlagsError(ef, (int32_t)osErrorResource);\n    return ((uint32_t)osErrorResource);\n  }\n";
    if body.matches(guard).count() != 1 {
        return fail("Nested-event test requires the corrected admission guard");
    }
    if ignore_refusal {
        body = body.replace(guard, "  (void)osRtxPostProcessTry(osRtxObject(ef));\n");
    }
    let store = "  event_flags = EventFlagsSet(ef, flags);\n";
    if body.matches(store).count() != 1 {
        return fail("Missing unique event publication boundary");
    }
    body = body.replace(store, &format!("  event_publication_probe(ef);\n{store}"));
    text.replace_range(start..end, &body);
    let marker = "#include \"rtx_lib.h\"\n";
    if text.matches(marker).count() != 1 {
        return fail("Missing event declaration boundary");
    }
    text = text.replace(
        marker,
        &format!("{marker}extern void event_publication_probe(os_event_flags_t *);\n"),
    );
    let path = output.join("event-test-provider.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"irq_hook":true,"ignored_refusal":ignore_refusal}),
    ))
}

fn instrument_message(
    source: &Path,
    output: &Path,
    scenario: u8,
    old_space: bool,
    omit_invalidation: bool,
    old_put: bool,
) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let mut text = original.clone();
    if old_space {
        let expression = "mq->mp_info.max_blocks - mq->mp_info.used_blocks";
        if text.matches(expression).count() != 2 {
            return fail("Missing corrected queue-space expressions");
        }
        text = text.replace(expression, "mq->mp_info.max_blocks - mq->msg_count");
    }
    if scenario == 2 {
        let put = text
            .find("static void MessageQueuePut (")
            .ok_or("Missing inserter")?;
        let get = text[put..]
            .find("static os_message_t *MessageQueueGet (")
            .ok_or("Missing inserter boundary")?
            + put;
        let walk = text[put..get]
            .find("    while ((prev != NULL)")
            .ok_or("Missing priority walk")?
            + put;
        let enable = "        __enable_irq();";
        let at = text[walk..get]
            .find(enable)
            .ok_or("Priority-walk test requires the corrected provider")?
            + walk;
        text.insert_str(at, "        message_walk_probe(mq);\n");
        let at = text
            .find("static void MessageQueuePut (")
            .ok_or("Missing inserter")?;
        text.insert_str(
            at,
            "extern void message_walk_probe(os_message_queue_t *);\n",
        );
    }
    if omit_invalidation {
        let store = "    mq->flags |= MSGQUEUE_CHANGED;";
        if text.matches(store).count() != 1 {
            return fail("Missing queue walk invalidation");
        }
        text = text.replace(store, "    /* Test-only omission of walk invalidation. */");
    }
    if old_put {
        let selected = "    if (osRtxPostProcessTry(osRtxObject(msg)) != 0U) {\n      EvrRtxMessageQueueInsertPending(mq, msg_ptr);\n      status = osOK;\n    } else {\n      // The message is still private: no PendSV consumer can release it.\n      msg->id = osRtxIdInvalid;\n      (void)osRtxMemoryPoolFree(&mq->mp_info, msg);\n      EvrRtxMessageQueueNotInserted(mq, msg_ptr);\n      status = osErrorResource;\n    }";
        if text.matches(selected).count() != 1 {
            return fail("Missing corrected ISR Put admission for the negative control");
        }
        text = text.replace(selected,
            "    osRtxPostProcess(osRtxObject(msg));\n    EvrRtxMessageQueueInsertPending(mq, msg_ptr);\n    status = osOK;");
    }
    let path = output.join("message-test-provider.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"walk_irq_hook":scenario==2,"old_space":old_space,"omitted_invalidation":omit_invalidation,"old_put":old_put}),
    ))
}

/// Pend a normal test IRQ after failed SVC allocation in the copied provider.
/// Production source and the pinned SDK are never instrumented.
fn instrument_message_get_gap(source: &Path, output: &Path) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let start = original
        .find("static osStatus_t svcRtxMessageQueuePut (")
        .ok_or("Missing SVC Put boundary")?;
    let end = original[start..]
        .find("static osStatus_t svcRtxMessageQueueGet (")
        .ok_or("Missing SVC Get boundary")?
        + start;
    let marker = "    msg = osRtxMemoryPoolAlloc(&mq->mp_info);\n";
    let body = &original[start..end];
    if body.matches(marker).count() != 1 {
        return fail("Missing unique failed-allocation scheduling boundary");
    }
    let body = body.replace(
        marker,
        &format!("{marker}    if (msg == NULL) {{ queue_get_gap_probe(mq); }}\n"),
    );
    let mut text = original.clone();
    text.replace_range(start..end, &body);
    text.insert_str(
        start,
        "extern void queue_get_gap_probe(os_message_queue_t *mq);\n",
    );
    let path = output.join("message-get-gap-provider.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"pending_irq_after_failed_alloc":true}),
    ))
}

/// Public-history and partial-fix controls are copied only into the QEMU
/// output. The pinned parent is the preceding public main, not raw Arm RTX.
fn message_get_variant(
    repo: &Path,
    source: &Path,
    output: &Path,
    variant: &str,
) -> Result<(PathBuf, Value)> {
    let selected = fs::read_to_string(source)?;
    let text = match variant {
        "parent" => {
            let out = Command::new("git")
                .args([
                    "show",
                    "38908c3fa19016ce052c059f136c8eb814b9c9d5:Firm_MCU/RTE/CMSIS/RTX/rtx_msgqueue.c",
                ])
                .current_dir(repo)
                .output()?;
            if !out.status.success()
                || sha(&out.stdout)
                    != "842e196627e7383edbc4ab0accb9bcf78080ceb448ed4039a78c907b5cb8de92"
            {
                return fail("Missing exact SSR-134 public parent source");
            }
            String::from_utf8(out.stdout)?
        }
        "waiter-only" => {
            let marker = "((mq->flags & MSGQUEUE_PUT_PENDING) != 0U) ||\n       ";
            if selected.matches(marker).count() != 1 {
                return fail("Missing in-flight sender admission for partial control");
            }
            selected.replace(marker, "")
        }
        "no-window-guard" => {
            let start = selected
                .find("static osStatus_t svcRtxMessageQueuePut (")
                .ok_or("Missing SVC Put for partial control")?;
            let end = selected[start..]
                .find("static osStatus_t svcRtxMessageQueueGet (")
                .ok_or("Missing SVC Get for partial control")?
                + start;
            let marker = "      __disable_irq();\n    }\n    msg = osRtxMemoryPoolAlloc";
            let body = &selected[start..end];
            if body.matches(marker).count() != 1 {
                return fail("Missing allocation exclusion for partial control");
            }
            let body = body.replace(marker, "      /* Test-only omission of allocation exclusion. */\n    }\n    msg = osRtxMemoryPoolAlloc");
            let mut text = selected.clone();
            text.replace_range(start..end, &body);
            text
        }
        _ => return fail("Unknown Message Get control variant"),
    };
    let path = output.join(format!("message-get-{variant}-provider.c"));
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"variant":variant,"selected_sha256":sha(selected.as_bytes()),"test_source_sha256":sha(text.as_bytes())}),
    ))
}

fn instrument_old_pool_free(source: &Path, output: &Path) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let selected = "  /* Invalid blocks retain the existing parameter-error path below. For a\n   * valid block, admit any waiting allocator's callback before releasing\n   * caller ownership. PendSV cannot run in this ISR/masked context. */\n  if (((uintptr_t)block >= (uintptr_t)mp->mp_info.block_base) &&\n      ((uintptr_t)block <  (uintptr_t)mp->mp_info.block_lim) &&\n      (mp->thread_list != NULL) &&\n      (osRtxPostProcessTry(osRtxObject(mp)) == 0U)) {\n    EvrRtxMemoryPoolFreeFailed(mp, block);\n    return osErrorResource;\n  }\n\n  // Free memory; no callback is needed when there is no waiting allocator.\n  status = osRtxMemoryPoolFree(&mp->mp_info, block);\n  if (status == osOK) {\n    EvrRtxMemoryPoolDeallocated(mp, block);";
    if original.matches(selected).count() != 1 {
        return fail("Missing corrected ISR Free admission for the negative control");
    }
    let text = original.replace(selected,
        "  // Free memory\n  status = osRtxMemoryPoolFree(&mp->mp_info, block);\n  if (status == osOK) {\n    // Register post ISR processing\n    osRtxPostProcess(osRtxObject(mp));\n    EvrRtxMemoryPoolDeallocated(mp, block);");
    let path = output.join("pool-free-test-provider.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"old_free":true}),
    ))
}

/// Inject only a pending test IRQ into a copied provider. Keep the pinned
/// production input unchanged and record the complete instrumented input.
fn instrument_pool(source: &Path, output: &Path) -> Result<(PathBuf, Value)> {
    let original = fs::read_to_string(source)?;
    let mut text = original.clone();
    let mut sites = 0;
    for (allocating, pattern) in [
        (1, "    mp_info->block_free = *((void **)block);\n"),
        (1, "  block = atomic_link_get(&mp_info->block_free);\n"),
        (0, "  mp_info->block_free = block;\n"),
        (0, "  atomic_link_put(&mp_info->block_free, block);\n"),
    ] {
        let count = text.matches(pattern).count();
        if count > 1 {
            return fail("Ambiguous pool publication hook");
        }
        if count == 1 {
            text = text.replace(
                pattern,
                &format!("{pattern}  pool_publication_probe(mp_info, {allocating});\n"),
            );
            sites += 1;
        }
    }
    if ![2, 4].contains(&sites) {
        return fail("Missing pool publication boundaries");
    }
    let marker = "//  ==== Library functions ====";
    if text.matches(marker).count() != 1 {
        return fail("Missing pool declaration boundary");
    }
    text = text.replace(
        marker,
        &format!("extern void pool_publication_probe(os_mp_info_t *, unsigned);\n{marker}"),
    );
    let path = output.join("pool-instrumented.c");
    fs::write(&path, &text)?;
    Ok((
        path,
        json!({"original_sha256":sha(original.as_bytes()),"instrumented_sha256":sha(text.as_bytes()),"hook_sites":sites}),
    ))
}

pub(super) fn validate(app: &Path, rtx: &Path) -> Result<()> {
    let owned = app.join("RTE/CMSIS/RTX");
    if owned.is_dir() {
        validate_owned(&owned, rtx)?;
    }
    validate_tick(&app.join("RTE/CMSIS/OS_Tick"), rtx)
}

fn validate_tick(owned: &Path, rtx: &Path) -> Result<()> {
    if !owned.exists() {
        return Ok(());
    }
    let pins: Value = serde_json::from_slice(&fs::read(owned.join("provenance.json"))?)?;
    for (field, path) in [
        (
            "original_os_systick_sha256",
            rtx.join("Source/os_systick.c"),
        ),
        (
            "original_os_tick_header_sha256",
            rtx.join("Include/os_tick.h"),
        ),
        ("selected_os_systick_sha256", owned.join("os_systick.c")),
    ] {
        let expected = pins[field].as_str().ok_or("Missing OS Tick source pin")?;
        if sha(&fs::read(path)?) != expected {
            return fail("OS Tick source does not match the selected CMSIS pack");
        }
    }
    let original = fs::read_to_string(rtx.join("Source/os_systick.c"))?;
    let selected = fs::read_to_string(owned.join("os_systick.c"))?;
    let changes = [
        ("  if (load > 0x00FFFFFFU) {", "  /* papsg: reject reload zero before changing timer or saved tick state.\n   * Adapted from paps-g a90f6e3c (SSR-130). */\n  if ((load == 0U) || (load > 0x00FFFFFFU)) {"),
        ("static uint8_t PendST;", "static uint8_t PendST;\n/* papsg: own writable CTRL bits without consuming read-clear COUNTFLAG.\n * Adapted from paps-g 3feed85e (SSR-132). */\nstatic uint32_t TickControl;"),
        ("  SysTick->CTRL =  SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk;", "  TickControl = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk;\n  SysTick->CTRL = TickControl;"),
        ("  SysTick->CTRL |=  SysTick_CTRL_ENABLE_Msk;", "  TickControl |= SysTick_CTRL_ENABLE_Msk;\n  SysTick->CTRL = TickControl;"),
        ("  SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;", "  TickControl &= ~SysTick_CTRL_ENABLE_Msk;\n  SysTick->CTRL = TickControl;"),
    ];
    let mut expected = original;
    for (step, (before, after)) in changes.into_iter().enumerate() {
        if expected.matches(before).count() != 1 {
            return fail("OS Tick base differs from declared source changes");
        }
        expected = expected.replace(before, after);
        if step == 0 && expected == selected {
            // The directly preceding public checkpoint owns only SSR-130.
            return Ok(());
        }
    }
    if expected != selected {
        return fail("OS Tick owner differs from declared reload/control changes");
    }
    Ok(())
}

fn validate_owned(owned: &Path, rtx: &Path) -> Result<()> {
    let pins: Value = serde_json::from_slice(&fs::read(owned.join("provenance.json"))?)?;
    for section in ["headers", "providers"] {
        for (name, expected) in pins[section].as_object().ok_or("Missing RTX pins")? {
            if !name.starts_with("rtx_") || name.contains(['/', '\\']) {
                return fail("Invalid RTX pin path");
            }
            let source = rtx.join("RTX/Source").join(name);
            let original = fs::read(&source)?;
            if Some(sha(&original).as_str()) != expected.as_str() {
                return fail("RTX SDK differs from the selected 5.5.1 provider");
            }
            if section == "headers"
                && !header_matches(name, &original, &fs::read(owned.join(name))?)
            {
                return fail(
                    "RTX support header differs beyond the declared postprocess prototype",
                );
            }
        }
    }
    let selected_kernel = owned.join("rtx_kernel.c");
    if selected_kernel.is_file() {
        let expected = pins["selected_kernel_sha256"]
            .as_str()
            .ok_or("Missing selected kernel source pin")?;
        if sha(&fs::read(selected_kernel)?) != expected {
            return fail("Selected kernel source differs from the declared tick reader");
        }
    }
    let selected_mutex = owned.join("rtx_mutex.c");
    if selected_mutex.is_file() {
        let expected = pins["selected_mutex_sha256"]
            .as_str()
            .ok_or("Missing selected mutex source pin")?;
        if sha(&fs::read(selected_mutex)?) != expected {
            return fail("Selected mutex source differs from the declared correction");
        }
    }
    let selected_semaphore = owned.join("rtx_semaphore.c");
    if selected_semaphore.is_file() {
        let expected = pins["selected_semaphore_sha256"]
            .as_str()
            .ok_or("Missing selected semaphore source pin")?;
        if sha(&fs::read(selected_semaphore)?) != expected {
            return fail("Selected semaphore source differs from the declared allocation policy");
        }
    }
    Ok(())
}

pub(super) fn run(args: &[String]) -> Result<()> {
    let case = args[14].as_str();
    let provider = match case {
        "delay" => "rtx_delay.c",
        "queue" => "rtx_msgqueue.c",
        "timer" => "rtx_timer.c",
        "pool" => "rtx_mempool.c",
        "pool-free" => "rtx_mempool.c",
        "message" => "rtx_msgqueue.c",
        "message-get" => "rtx_msgqueue.c",
        "event" => "rtx_evflags.c",
        "retire" => "rtx_system.c",
        "syscounter" => "rtx_kernel.c",
        "thread-flags" => "rtx_thread.c",
        "mutex" => "rtx_mutex.c",
        _ => return fail("Unknown RTOS contract case"),
    };
    let (atomic, phase, inside) = pool_options(case, &args[15..])?;
    let (scenario, old_space, omit_invalidation) = scenario_options(case, &args[15..])?;
    let counter_mode = syscounter_mode(case, &args[15..])?;
    let ignore_event_refusal = args[15..]
        .iter()
        .filter(|v| *v == "--ignore-event-refusal")
        .count();
    if ignore_event_refusal > 1 || (ignore_event_refusal == 1 && (case != "event" || scenario != 2))
    {
        return fail("Event refusal control requires one --case event --scenario=nested");
    }
    let old_put = args[15..].iter().filter(|v| *v == "--old-put").count();
    if old_put > 1 || (old_put == 1 && (case != "message" || !(4..=6).contains(&scenario))) {
        return fail("Old Put control requires one message admission scenario");
    }
    let old_free = args[15..].iter().filter(|v| *v == "--old-free").count();
    if old_free > 1 || (old_free == 1 && case != "pool-free") {
        return fail("Old Free control requires one pool-free scenario");
    }
    let get_variants = args[15..]
        .iter()
        .filter_map(|v| v.strip_prefix("--get-variant="))
        .collect::<Vec<_>>();
    if get_variants.len() > 1
        || (!get_variants.is_empty() && case != "message-get")
        || get_variants
            .first()
            .is_some_and(|v| !["parent", "waiter-only", "no-window-guard"].contains(v))
    {
        return fail("Specify one valid Message Get test variant");
    }
    let get_variant = get_variants.first().copied().unwrap_or("selected");
    let baseline = args[15..].iter().any(|v| v == "--baseline");
    if case == "syscounter" && baseline {
        return fail("Use the preceding public worktree for a complete syscounter baseline");
    }
    if case == "message-get" && baseline {
        return fail("Use the exact public parent for the message Get baseline");
    }
    if baseline && (old_put == 1 || old_free == 1) {
        return fail("A single-source inverse cannot be combined with --baseline");
    }
    let archive = args[15..].iter().any(|v| v == "--archive");
    let lto = args[15..].iter().any(|v| v == "--lto");
    let repo = Path::new(&args[2]).canonicalize()?;
    let out = new_build_output(&repo, Path::new(&args[4]))?;
    let bin = Path::new(&args[6]).canonicalize()?.join("bin");
    let cc = bin.join("arm-none-eabi-gcc");
    if capture(Command::new(&cc).arg("-dumpmachine"))?.trim() != "arm-none-eabi" {
        return fail("Expected Arm bare-metal GCC");
    }
    let mut pins = mcu::Inputs::default();
    for (from, name, key) in [
        (
            Path::new(&args[8]).join("Core/Include"),
            "core",
            "CMSIS/Core/Include",
        ),
        (Path::new(&args[10]).to_owned(), "stm32", "STM32/Include"),
        (Path::new(&args[12]).to_owned(), "rtos", "CMSIS/RTOS2"),
        (
            repo.join("Firm_MCU/RTE/CMSIS"),
            "config",
            "Firm_MCU/RTE/CMSIS",
        ),
        (repo.join("Firm_MCU/RTE/_App"), "rte", "Firm_MCU/RTE/_App"),
    ] {
        mcu::copy_tree(&from, &out.join(name), key, &mut pins)?;
    }
    mcu::copy_file(
        &repo.join("Firm_MCU/RTE/CMSIS/RTX/provenance.json"),
        &out.join("config/RTX/provenance.json"),
        "Firm_MCU/RTE/CMSIS/RTX/provenance.json".into(),
        &mut pins,
    )?;
    if repo.join("Firm_MCU/RTE/CMSIS/OS_Tick").is_dir() {
        mcu::copy_file(
            &repo.join("Firm_MCU/RTE/CMSIS/OS_Tick/provenance.json"),
            &out.join("config/OS_Tick/provenance.json"),
            "Firm_MCU/RTE/CMSIS/OS_Tick/provenance.json".into(),
            &mut pins,
        )?;
    }
    // Validate the staged owner, not a later read of the working tree.
    validate_owned(&out.join("config/RTX"), &out.join("rtos"))?;
    validate_tick(&out.join("config/OS_Tick"), &out.join("rtos"))?;
    let compiler_version = capture(Command::new(&cc).arg("--version"))?;
    let compiler_sha256 = sha(&fs::read(&cc)?);
    let builder_sha256 = sha(&fs::read(std::env::current_exe()?)?);
    for name in [
        "rtos_contract.c",
        "rtos_contract.ld",
        "rtos_timer_contract.inc",
        "rtos_pool_contract.inc",
        "rtos_pool_free_admission.inc",
        "rtos_message_contract.inc",
        "rtos_message_space.inc",
        "rtos_message_admission.inc",
        "rtos_event_contract.inc",
        "rtos_event_nested.inc",
        "rtos_retire_contract.inc",
    ] {
        let input = if name == "rtos_contract.c" {
            match case {
                "syscounter" => "rtos_syscounter.c",
                "thread-flags" => "rtos_thread_admission.c",
                "message-get" => "rtos_message_get_admission.c",
                "mutex" => "rtos_mutex.c",
                _ => name,
            }
        } else {
            name
        };
        mcu::copy_file(
            &repo.join("tests").join(input),
            &out.join(name),
            format!("tests/{input}"),
            &mut pins,
        )?;
    }
    let deadline = Instant::now() + Duration::from_secs(180);
    let mut commands = Vec::new();
    let mut instrumentation = Value::Null;
    let result = (|| -> Result<()> {
        let kernel = out.join("rtos/RTX/Source");
        let owned = out.join("config/RTX");
        let mut sources = fs::read_dir(&kernel)?
            .map(|e| e.map(|e| e.path()))
            .collect::<std::io::Result<Vec<_>>>()?;
        sources.retain(|s| s.extension().is_some_and(|v| v == "c"));
        sources.sort();
        let tick_owner = out.join("config/OS_Tick/os_systick.c");
        let tick = if tick_owner.is_file() {
            tick_owner
        } else {
            out.join("rtos/Source/os_systick.c")
        };
        let mut direct = vec![
            out.join("rtos_contract.c"),
            out.join("config/RTX_Config.c"),
            kernel.join("GCC/irq_cm4f.S"),
            tick,
        ];
        let mut kernel_objects = Vec::new();
        for source in &mut sources {
            let name = source.file_name().ok_or("Missing kernel source name")?;
            if owned.join(name).is_file() && !(baseline && name == provider && case != "retire") {
                direct.push(owned.join(name));
                if !archive {
                    *source = owned.join(name);
                }
            }
        }
        if !archive {
            direct.retain(|s| s.parent() != Some(owned.as_path()));
            direct.extend(sources.iter().cloned());
            sources.clear();
        }
        if case == "pool" && inside {
            let selected = if !baseline && owned.join(provider).is_file() {
                owned.join(provider)
            } else {
                kernel.join(provider)
            };
            let (instrumented, record) = instrument_pool(&selected, &out)?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Pool provider selection is not unique");
            }
            instrumentation = record;
        }
        if case == "message" && (scenario == 2 || old_space || old_put == 1) {
            let selected = if !baseline && owned.join(provider).is_file() {
                owned.join(provider)
            } else {
                kernel.join(provider)
            };
            let (instrumented, record) = instrument_message(
                &selected,
                &out,
                scenario,
                old_space,
                omit_invalidation,
                old_put == 1,
            )?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Message provider selection is not unique");
            }
            instrumentation = record;
        }
        if case == "message-get" && (scenario == 8 || get_variant != "selected") {
            let selected = if owned.join(provider).is_file() {
                owned.join(provider)
            } else {
                kernel.join(provider)
            };
            let mut test_source = selected.clone();
            let mut records = Vec::new();
            if get_variant != "selected" {
                let (path, record) = message_get_variant(&repo, &test_source, &out, get_variant)?;
                test_source = path;
                records.push(record);
            }
            if scenario == 8 {
                let (path, record) = instrument_message_get_gap(&test_source, &out)?;
                test_source = path;
                records.push(record);
            }
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = test_source.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Message Get provider selection is not unique");
            }
            instrumentation = json!(records);
        }
        if case == "pool-free" && old_free == 1 {
            let selected = owned.join(provider);
            let (instrumented, record) = instrument_old_pool_free(&selected, &out)?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Pool Free provider selection is not unique");
            }
            instrumentation = record;
        }
        if case == "event" && scenario == 2 {
            let selected = if !baseline && owned.join(provider).is_file() {
                owned.join(provider)
            } else {
                kernel.join(provider)
            };
            let (instrumented, record) =
                instrument_event(&selected, &out, ignore_event_refusal == 1)?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Event provider selection is not unique");
            }
            instrumentation = record;
        }
        if case == "retire" {
            let selected = if owned.join(provider).is_file() {
                owned.join(provider)
            } else {
                kernel.join(provider)
            };
            let (instrumented, record) = instrument_retire(&selected, &out, baseline)?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Retirement provider selection is not unique");
            }
            instrumentation = record;
        }
        if case == "syscounter" && [3, 4, 5, 7].contains(&counter_mode) {
            let name = if counter_mode == 3 {
                "rtx_system.c"
            } else {
                "rtx_kernel.c"
            };
            let selected = if owned.join(name).is_file() {
                owned.join(name)
            } else {
                kernel.join(name)
            };
            let (instrumented, record) = instrument_syscounter(&selected, &out, counter_mode)?;
            let mut replaced = 0;
            for source in sources.iter_mut().chain(direct.iter_mut()) {
                if *source == selected {
                    *source = instrumented.clone();
                    replaced += 1;
                }
            }
            if replaced != 1 {
                return fail("Syscounter provider selection is not unique");
            }
            instrumentation = record;
        }
        let mut objects = Vec::new();
        for (index, source) in sources.iter().chain(&direct).enumerate() {
            let object = out.join(format!(
                "{index}-{}.o",
                source.file_stem().unwrap().to_string_lossy()
            ));
            let mut command = Command::new(&cc);
            command.args([
                "-mcpu=cortex-m7",
                "-mthumb",
                "-mfpu=fpv5-d16",
                "-mfloat-abi=softfp",
                "-Os",
                "-g",
                "-fstack-usage",
                "-ffunction-sections",
                "-fdata-sections",
                "-fno-strict-aliasing",
                "-DSTM32H750xx",
                "-DHSE_VALUE=16000000",
            ]);
            if case == "timer" {
                command.args([
                    "-DTEST_RTX_TIMERS=1",
                    "-DOS_TIMER_THREAD_STACK_SIZE=512",
                    "-DOS_TIMER_CB_QUEUE=8",
                ]);
            }
            if case == "queue" {
                command.arg("-DTEST_RTX_QUEUE=1");
            }
            if case == "pool" {
                command.args([
                    "-DTEST_RTX_POOL=1".to_owned(),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                    format!("-DTEST_POOL_PHASE={phase}"),
                    format!("-DTEST_POOL_INSIDE={}", u8::from(inside)),
                ]);
            }
            if case == "pool-free" {
                command.args([
                    "-DTEST_RTX_POOL_FREE=1".to_owned(),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                    format!("-DTEST_POOL_FREE_SCENARIO={scenario}"),
                ]);
            }
            if case == "message" {
                command.args([
                    "-DTEST_RTX_MESSAGE=1".to_owned(),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                    format!("-DTEST_MESSAGE_SCENARIO={scenario}"),
                ]);
            }
            if case == "event" {
                command.args([
                    "-DTEST_RTX_EVENTS=1".to_owned(),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                    format!("-DTEST_EVENT_SCENARIO={scenario}"),
                ]);
            }
            if case == "retire" {
                command.args([
                    "-DTEST_RTX_RETIRE=1".to_owned(),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                ]);
            }
            if case == "syscounter" {
                command.arg(format!("-DTEST_SYSCOUNTER_MODE={counter_mode}"));
            }
            if case == "thread-flags" {
                command.args([
                    format!("-DTEST_SCENARIO={scenario}"),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                ]);
            }
            if case == "message-get" {
                command.args([
                    format!("-DTEST_SCENARIO={scenario}"),
                    format!("-DEXCLUSIVE_ACCESS={atomic}"),
                ]);
            }
            if case == "mutex" {
                command.arg("-DOBSERVE_PRIORITIES=1");
            }
            if lto {
                command.args(["-flto=4", "-flto-partition=one"]);
            }
            for include in [
                out.join("config"),
                owned.clone(),
                out.join("rte"),
                out.join("core"),
                out.join("stm32"),
                out.join("rtos/Include"),
                out.join("rtos/RTX/Include"),
                kernel.clone(),
            ] {
                command.arg("-I").arg(include);
            }
            command.args(["-c"]).arg(source).arg("-o").arg(&object);
            mcu::execute(&mut command, &out, &mut commands, deadline)?;
            if index < sources.len() {
                kernel_objects.push(object);
            } else {
                objects.push(object);
            }
        }
        if archive {
            let library = out.join("rtx-original.a");
            mcu::execute(
                Command::new(bin.join("arm-none-eabi-ar"))
                    .arg("rcs")
                    .arg(&library)
                    .args(kernel_objects),
                &out,
                &mut commands,
                deadline,
            )?;
            objects.push(library);
        }
        let elf = out.join("contract.elf");
        let mut command = Command::new(&cc);
        command.args([
            "-mcpu=cortex-m7",
            "-mthumb",
            "-mfpu=fpv5-d16",
            "-mfloat-abi=softfp",
        ]);
        if lto {
            command.args(["-flto=4", "-flto-partition=one"]);
        }
        command
            .arg("-nostartfiles")
            .arg("-Wl,--gc-sections")
            .arg(format!("-Wl,-Map={}", out.join("contract.map").display()))
            .arg("-T")
            .arg(out.join("rtos_contract.ld"));
        command
            .arg("-Wl,--start-group")
            .args(objects)
            .args(["-lc", "-lgcc", "-Wl,--end-group", "-o"])
            .arg(&elf);
        mcu::execute(&mut command, &out, &mut commands, deadline)?;
        let version = capture(Command::new("qemu-system-arm").arg("--version"))?;
        fs::write(out.join("qemu-version.txt"), version)?;
        mcu::execute(
            Command::new("qemu-system-arm")
                .args([
                    "-M",
                    "mps2-an500",
                    "-nographic",
                    "-monitor",
                    "none",
                    "-serial",
                    "none",
                    "-semihosting-config",
                    "enable=on,target=native",
                    "-kernel",
                ])
                .arg(&elf),
            &out,
            &mut commands,
            deadline,
        )?;
        if case == "mutex" {
            let log = fs::read_to_string(out.join("build.log"))?;
            let expected = if baseline {
                "SUMMARY cases=0x00000480 wrong_after=0x000000c0 wrong_cleanup=0x00000120"
            } else {
                "SUMMARY cases=0x00000480 wrong_after=0x00000000 wrong_cleanup=0x00000000"
            };
            if !log.contains(expected) {
                return fail("Mutex priority summary differs from the selected or raw contract");
            }
        }
        Ok(())
    })();
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(
            &json!({"case":case,"baseline":baseline,"archive":archive,"lto":lto,"pool_atomic":atomic,"pool_phase":phase,"pool_inside":inside,"message_scenario":scenario,"message_get_scenario":scenario,"message_get_variant":get_variant,"event_scenario":scenario,"thread_scenario":scenario,"syscounter_mode":counter_mode,"old_put":old_put==1,"old_free":old_free==1,"instrumentation":instrumentation,"compiler_version":compiler_version,"compiler_sha256":compiler_sha256,"builder_sha256":builder_sha256,"success":result.is_ok(),"error":result.as_ref().err().map(ToString::to_string),"inputs_sha256":pins.hashes,"commands":commands,"scope":"Actual RTX API/SVC/kernel in QEMU; no SAROO or game qualification"}),
        )?,
    )?;
    result?;
    if case == "mutex" && baseline {
        println!(
            "RTOS mutex raw-provider defect reproduced; {}",
            out.display()
        );
    } else {
        println!("RTOS {case} contract passes; {}", out.display());
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn syscounter_modes_are_bounded_and_single() {
        assert_eq!(syscounter_mode("syscounter", &[]).unwrap(), 0);
        assert_eq!(
            syscounter_mode("syscounter", &["--mode=10".into()]).unwrap(),
            10
        );
        for flags in [
            vec!["--mode=11"],
            vec!["--mode=0", "--mode=1"],
            vec!["--mode=bad"],
        ] {
            assert!(syscounter_mode(
                "syscounter",
                &flags.into_iter().map(String::from).collect::<Vec<_>>()
            )
            .is_err());
        }
        assert!(syscounter_mode("delay", &["--mode=0".into()]).is_err());
    }

    #[test]
    fn pool_case_refuses_ambiguous_or_misapplied_configuration() {
        for flags in [
            vec!["--atomic=0", "--atomic=1"],
            vec!["--atomic=2"],
            vec!["--phase=masked", "--phase=after"],
            vec!["--phase=unknown"],
        ] {
            let flags = flags.into_iter().map(String::from).collect::<Vec<_>>();
            assert!(pool_options("pool", &flags).is_err());
        }
        assert!(pool_options("delay", &["--atomic=0".into()]).is_err());
        assert_eq!(pool_options("pool", &[]).unwrap(), (1, 0, true));
        assert_eq!(
            pool_options("pool", &["--phase=after".into(), "--atomic=0".into()]).unwrap(),
            (0, 0, false)
        );
    }

    #[test]
    fn message_controls_require_the_corresponding_scenario() {
        for flags in [
            vec!["--scenario=reuse", "--old-space"],
            vec!["--scenario=space", "--omit-walk-invalidation"],
            vec!["--scenario=walk", "--scenario=space"],
            vec!["--scenario=unknown"],
            vec!["--scenario=space", "--old-space", "--old-space"],
        ] {
            let flags = flags.into_iter().map(String::from).collect::<Vec<_>>();
            assert!(scenario_options("message", &flags).is_err());
        }
        assert!(pool_options("pool", &["--scenario=space".into()]).is_err());
        assert!(pool_options("message", &["--phase=after".into()]).is_err());
        assert_eq!(
            scenario_options(
                "message",
                &["--scenario=space".into(), "--old-space".into()]
            )
            .unwrap(),
            (3, true, false)
        );
    }

    #[test]
    fn message_get_scenarios_and_variants_stay_isolated() {
        assert_eq!(scenario_options("message-get", &[]).unwrap().0, 6);
        for (name, expected) in [("full", 6), ("batch", 7), ("gap", 8)] {
            assert_eq!(
                scenario_options("message-get", &[format!("--scenario={name}")])
                    .unwrap()
                    .0,
                expected
            );
        }
        assert!(scenario_options("message-get", &["--scenario=receiver".into()]).is_err());
        assert!(scenario_options("message", &["--scenario=gap".into()]).is_err());
        assert!(pool_options(
            "message-get",
            &["--atomic=0".into(), "--get-variant=parent".into()]
        )
        .is_ok());
        assert!(pool_options("message", &["--get-variant=parent".into()]).is_err());
    }

    #[test]
    fn event_modes_do_not_admit_message_mutations_or_unrelated_modes() {
        assert_eq!(
            scenario_options("event", &["--scenario=nested".into()]).unwrap(),
            (2, false, false)
        );
        assert_eq!(
            scenario_options("event", &["--scenario=legacy".into()]).unwrap(),
            (3, false, false)
        );
        assert!(scenario_options("event", &["--scenario=walk".into()]).is_err());
        assert!(scenario_options("message", &["--scenario=single".into()]).is_err());
        assert!(pool_options("event", &["--old-space".into()]).is_err());
        assert!(pool_options("message", &["--ignore-event-refusal".into()]).is_err());
    }

    #[test]
    fn only_the_shared_postprocess_declaration_can_extend_a_support_header() {
        let original = b"before\nextern void osRtxPostProcess    (os_object_t *object);\nafter\n";
        let selected = b"before\nextern void osRtxPostProcess    (os_object_t *object);\nextern uint32_t osRtxPostProcessTry (os_object_t *object);\nafter\n";
        assert!(header_matches("rtx_lib.h", original, selected));
        assert!(header_matches("rtx_lib.h", original, original));
        assert!(!header_matches("rtx_core_c.h", original, selected));
        let mut extra = selected.to_vec();
        extra.extend_from_slice(b"extern int unrelated;\n");
        assert!(!header_matches("rtx_lib.h", original, &extra));
        assert!(!header_matches("rtx_lib.h", b"other SDK", selected));
    }

    #[test]
    fn provider_admission_refuses_changed_headers_and_sdk_versions() {
        let root = std::env::temp_dir().join(format!("saroo-rtx-pins-{}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let app = root.join("app");
        let owned = app.join("RTE/CMSIS/RTX");
        let sdk = root.join("sdk/RTX/Source");
        fs::create_dir_all(&owned).unwrap();
        fs::create_dir_all(&sdk).unwrap();
        fs::write(owned.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_delay.c"), b"provider").unwrap();
        fs::write(owned.join("rtx_kernel.c"), b"selected kernel").unwrap();
        fs::write(
            owned.join("provenance.json"),
            serde_json::to_vec(&json!({
                "headers":{"rtx_lib.h":sha(b"header")},
                "providers":{"rtx_delay.c":sha(b"provider")},
                "selected_kernel_sha256":sha(b"selected kernel")
            }))
            .unwrap(),
        )
        .unwrap();
        assert!(validate(&app, &root.join("sdk")).is_ok());
        fs::write(owned.join("rtx_kernel.c"), b"modified kernel").unwrap();
        assert!(validate(&app, &root.join("sdk")).is_err());
        fs::write(owned.join("rtx_kernel.c"), b"selected kernel").unwrap();
        fs::write(owned.join("rtx_lib.h"), b"modified").unwrap();
        assert!(validate(&app, &root.join("sdk")).is_err());
        fs::write(owned.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_delay.c"), b"different version").unwrap();
        assert!(validate(&app, &root.join("sdk")).is_err());
        assert_eq!(
            fs::read(sdk.join("rtx_delay.c")).unwrap(),
            b"different version"
        );
        fs::remove_dir_all(root).unwrap();
    }
}
