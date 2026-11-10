/* Set semantic fixture values against the actual production metadata layout.
 * The legacy branch keeps affected-revision negative controls buildable. */
#ifndef TEST_TRACK_FIXTURE_H
#define TEST_TRACK_FIXTURE_H
#ifndef TRACK_SIZE
#define TRACK_SIZE(tk) ((tk)->sector_size)
#define TRACK_CTRL(tk) ((tk)->ctrl_addr)
static inline void test_track_size(TRACK_INFO *tk, unsigned size) { tk->sector_size=size; }
static inline void test_track_mode(TRACK_INFO *tk, unsigned mode) {
    tk->mode=mode; tk->ctrl_addr=mode==3?0x01:0x41;
}
#else
static inline void test_track_size(TRACK_INFO *tk, unsigned size) {
    assert(size==2048 || size==2324 || size==2336 || size==2352); tk->sector_size=size;
}
static inline void test_track_mode(TRACK_INFO *tk, unsigned mode) {
    assert(mode>=1 && mode<=3); tk->mode=mode;
}
#endif
static inline void test_track_indices(TRACK_INFO *tk, int *values, unsigned base) {
#ifdef TRACK_INDEX
    tk->index_base=values?base+1:0;
    if (values) for (unsigned n=0;n+1<tk->max_index;n++) index_table[base+n]=values[n];
#else
    (void)base; tk->index=values;
#endif
}
#endif
