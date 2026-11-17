/* Compile the selected planner with the actual menu allocator and target ABI.
 * This translation unit is a build check only; it is never linked into menu. */
#include "../Firm_Saturn/game_save.c"
#include "../Firm_Saturn/save_journal_plan.c"

int save_journal_plan_target_compile(SaveJournalBuilder *builder,
                                     SaveJournalParts *parts, u8 *workspace,
                                     u8 *claims, const u8 *game_id,
                                     const BUPDIR *dir, const u8 *data,
                                     const u8 *header, int dev)
{
    if (dev)
        return journal_plan_mems_parts(builder, parts, workspace, claims,
                                       game_id, dir, data, header);
    return journal_plan_parts_root(builder, parts, workspace, claims,
                                   game_id, dev);
}
