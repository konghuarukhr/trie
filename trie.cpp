// By zhenglingyun (konghuarukhr@163.com), 2014-07-20
// Alghouth this is written in C++, I prefer C implementation because C is tiny,
// clear, and easy to debug. I use C++ mechanism only when it is hard to be
// implemented in C. So, this code is easy to be changed to C implementation.


#include "trie.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

// INC_BASE_WHEN_COLLISION is used to indicate increasing base value when two
// nodes collision happened, it will be faster (about 2x faster in my test).
// But it will probably use a little more memory I guess.
#define INC_BASE_WHEN_COLLISION
// START_BASE_OPTIMIZATION is used to optimize finding base value.
// 10x faster then no any optimization but 2x memory occupation in my test.
#define START_BASE_OPTIMIZATION
// BTW, how to find base value is significant in Double-Array Trie algorithm.

#define EXPECT(c, v) __builtin_expect(c, v)
// If __builtin_expect() is not supported by your compiler:
//#define EXPECT(c, v) (c)

#define PRE_ALLOCED_WORDS 64 // used to store words prefix returned to user call
#if PRE_ALLOCED_WORDS <= 0
#error PRE_ALLOCED_WORDS should be greater then 0
#endif

#define ROOT 1
#if ROOT <= 0
#error ROOT should be greater then 0
#endif
#define BASE (ROOT + 1)
#if BASE <= ROOT
#error BASE should be greater then ROOT
#endif

#define NEXT_INDEX(base, word) ((base) + (word))
#define NEXT_NODE(base, word) (nodes[NEXT_INDEX(base, word)])

#define INVARIANT_VIOLATION \
            "Invariant violation: every words should be distinguishable."

using namespace std;


struct TrieNode {
    Index base;
    Index prev;
};

struct _Tail {
    Word *words;
    Index n_words;
    Index data;
    Index used_by;
};

class _Trie {
    struct TrieNode *nodes;
    Index n_alloced_nodes;
#ifdef START_BASE_OPTIMIZATION
    Index next_unused_node_idx;
#endif
    struct _Tail *tails;
    Index n_alloced_tails;
    Index next_unused_tail_idx;

    void expand_nodes(Index next);
#ifdef START_BASE_OPTIMIZATION
    void inc_next_unused_node_idx(Index idx);
    void dec_next_unused_node_idx(Index idx);
#endif

    Index get_next_unused_tail_idx(void);
    void set_next_unused_tail_idx(Index idx);
    void fill_tail(Index tail_idx, const Word words[], Index n_words,
                   Index data, Index tn_idx);

    void adjust(Index tn_idx, const vector<Word>& subs);
    void adjust(Index tn_idx, const vector<Word>& subs, const Word& word);
    void move(Index tn_idx, const vector<Word>& subs, Index offset);
    void collect_sub_nodes(Index tn_idx, vector<Word>& subs) const;
    void erase_all_subs(Index tn_idx);
    void collect_all_subs(Index tn_idx, vector<Index>& results) const;
    void collect_all_subs(Index tn_idx, struct Tail *result,
                          Index n_alloced_words, vector<Tail>& results) const;

    bool search(const Word words[], Index n_words,
                struct _Tail **tailp, Index *data, Index *unmatch) const;
    bool search(const Word words[], Index n_words, struct _Tail **tailp) const;

public:
    _Trie(void);
    ~_Trie(void);

    void insert(const Word words[], Index n_words, Index data);
    void erase(const Word words[], Index n_words);
    bool search(const Word words[], Index n_words,
                Index *data, Index *unmatch) const;
    void prefix(const Word words[], Index n_words, vector<Tail>& results) const;
    void prefix(const Word words[], Index n_words, vector<Index>& results) const;
    bool segment_max_match(const Word words[], Index n_words, Word end_word,
                           Index *data, Index *unmatch) const;
    bool segment_min_match(const Word words[], Index n_words, Word end_word,
                           Index *data, Index *unmatch) const;
};

_Trie::_Trie(void)
{
    n_alloced_nodes = ROOT + 1;
    nodes = (struct TrieNode *)calloc(n_alloced_nodes, sizeof *nodes);
    if (!nodes) {
        throw bad_alloc();
    }
    nodes[ROOT].base = BASE;
#ifdef START_BASE_OPTIMIZATION
    // +1 is not needed but I want to leave the first Word space for the first
    // Word. (nodes[ROOT].base == BASE forever)
    next_unused_node_idx = BASE + (Word)(~0ULL) + 1;
#endif

    n_alloced_tails = 1;
    tails = (struct _Tail *)calloc(n_alloced_tails, sizeof (*tails));
    if (!tails) {
        free(nodes);
        throw bad_alloc();
    }
    next_unused_tail_idx = 0;
}

_Trie::~_Trie(void)
{
    free(nodes);
    for (Index i = 0; i < n_alloced_tails; i++) {
        free(tails[i].words);
    }
    free(tails);
}

void _Trie::expand_nodes(Index next)
{
    assert(next >= n_alloced_nodes);

    Index old_n_alloced_nodes = n_alloced_nodes;
    struct TrieNode *old_nodes = nodes;
    n_alloced_nodes = next >= (n_alloced_nodes << 1) ? next + 1
                                                     : (n_alloced_nodes << 1);
    nodes = (struct TrieNode *)realloc(nodes, n_alloced_nodes * sizeof *nodes);
    if (!nodes) {
        n_alloced_nodes = old_n_alloced_nodes;
        nodes = old_nodes;
        throw bad_alloc();
    }
    memset(nodes + old_n_alloced_nodes, 0,
           (n_alloced_nodes - old_n_alloced_nodes) * sizeof *nodes);
}

#ifdef START_BASE_OPTIMIZATION
void _Trie::inc_next_unused_node_idx(Index idx)
{
    if (idx == next_unused_node_idx) {
        while (++next_unused_node_idx < n_alloced_nodes) {
            if (!nodes[next_unused_node_idx].prev) {
                break;
            }
        }
    }
}

void _Trie::dec_next_unused_node_idx(Index idx)
{
    if (idx > BASE + (Word)(~0ULL) && idx < next_unused_node_idx) {
        next_unused_node_idx = idx;
    }
}
#endif

Index _Trie::get_next_unused_tail_idx(void)
{
    assert(next_unused_tail_idx <= n_alloced_tails);

    if (next_unused_tail_idx == n_alloced_tails) {
        Index old_n_alloced_tails = n_alloced_tails;
        struct _Tail *old_tails = tails;
        assert(n_alloced_tails);
        n_alloced_tails = n_alloced_tails << 1;
        tails = (struct _Tail *)realloc(tails, n_alloced_tails * sizeof *tails);
        if (!tails) {
            n_alloced_tails = old_n_alloced_tails;
            tails = old_tails;
            throw bad_alloc();
        }
        memset(tails + old_n_alloced_tails, 0,
                (n_alloced_tails - old_n_alloced_tails) * sizeof *tails);
        return next_unused_tail_idx++;
    }
    Index result = next_unused_tail_idx;
    while (++next_unused_tail_idx < n_alloced_tails) {
        if (!tails[next_unused_tail_idx].used_by) {
            break;
        }
    }
    return result;
}

void _Trie::set_next_unused_tail_idx(Index idx)
{
    if (idx < next_unused_tail_idx) {
        next_unused_tail_idx = idx;
    }
}

void _Trie::fill_tail(Index tail_idx, const Word words[], Index n_words,
                      Index data, Index tn_idx)
{
    struct _Tail *tail = tails + tail_idx;

    assert(!tail->words);
    tail->n_words = n_words;
    if (n_words) {
        tail->words = (Word *)malloc(n_words * sizeof *tail->words);
        if (!tail->words) {
            throw bad_alloc();
        }
        memcpy(tail->words, words, n_words * sizeof *tail->words);
    } else {
        tail->words = NULL;
    }
    tail->data = data;
    tail->used_by = tn_idx;
}

void _Trie::insert(const Word words[], Index n_words, Index data)
{
    struct _Tail *tail;
    if (search(words, n_words, &tail)) {
        tail->data = data;
        return;
    }

    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        Index next;

        if (base >= BASE) {
            next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0)) {
                expand_nodes(next);
            } else {
                Index next_prev = nodes[next].prev;

                if (next_prev == tn_idx) {
                    tn_idx = next;
                    continue;
                }

                if (next_prev) {
                    assert(next_prev >= ROOT);

                    // next node collision
                    vector<Word> sub1, sub2;
                    collect_sub_nodes(next_prev, sub1);
                    collect_sub_nodes(tn_idx, sub2);
                    if (sub1.size() <= sub2.size()) {
                        bool tn_idx_changed = false;
                        // If tn_idx is a sub-node of next_prev,
                        // tn_idx will be changed after adjusting sub1.
                        if (nodes[tn_idx].prev == next_prev) {
                            tn_idx_changed = true;
                        }
                        adjust(nodes[next].prev, sub1);
                        if (tn_idx_changed) {
                            assert(i);
                            tn_idx = NEXT_INDEX(nodes[next_prev].base,
                                                words[i - 1]);
                        }
                    } else {
                        adjust(tn_idx, sub2, words[i]);
                        // nodes[tn_idx].base is changed after adjusting sub2
                        next = NEXT_INDEX(nodes[tn_idx].base, words[i]);
                        if (EXPECT(next >= n_alloced_nodes, 0)) {
                            expand_nodes(next);
                        }
                    }
                }
            }
            assert(!nodes[next].prev);
            nodes[next].prev = tn_idx;
            Index tail_idx = get_next_unused_tail_idx();
            fill_tail(tail_idx, words + i + 1, n_words - i - 1, data, next);
            nodes[next].base = -tail_idx;
#ifdef START_BASE_OPTIMIZATION
            inc_next_unused_node_idx(next);
#endif
            return;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            while (j < tail->n_words && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                for (
#ifdef START_BASE_OPTIMIZATION
                     next = next_unused_node_idx;
#else
                     next = NEXT_INDEX(BASE, words[i]);
#endif
                     EXPECT(next < n_alloced_nodes, 1);
                     next++) {
                    if (!nodes[next].prev) {
                        break;
                    }
                }
                if (EXPECT(next >= n_alloced_nodes, 0)) {
                    expand_nodes(next);
                }
                nodes[tn_idx].base = next - words[i];
                assert(!nodes[next].prev);
                nodes[next].prev = tn_idx;
                tn_idx = next;
#ifdef START_BASE_OPTIMIZATION
                inc_next_unused_node_idx(next);
#endif

                j++;
                i++;
            }
            if (j == tail->n_words || i == n_words) {
                assert(!(j == tail->n_words && i == n_words));
                // Recovery.
                nodes[tn_idx].base = (Index)-(tail - tails);
                tail->used_by = tn_idx;
                tail->n_words -= j;
                if (tail->n_words) {
                    memmove(tail->words, tail->words + j,
                            tail->n_words * sizeof *tail->words);
                    tail->words = (Word *)realloc(tail->words,
                                           tail->n_words * sizeof *tail->words);
                    // We consider this realloc() should always succeed.
                    assert(tail->words);
                } else {
                    free(tail->words);
                    tail->words = NULL;
                }
                throw invalid_argument(INVARIANT_VIOLATION);
            }

            for (
#ifdef START_BASE_OPTIMIZATION
                 base = next_unused_node_idx -
                        (tail->words[j] < words[i] ? tail->words[j] : words[i]);
#else
                 base = BASE;
#endif
                 (EXPECT(NEXT_INDEX(base, tail->words[j]) < n_alloced_nodes, 1) &&
                  NEXT_NODE(base, tail->words[j]).prev) ||
                 (EXPECT(NEXT_INDEX(base, words[i]) < n_alloced_nodes, 1) &&
                  NEXT_NODE(base, words[i]).prev);
                 base++);

            if (EXPECT(NEXT_INDEX(base, tail->words[j]) >= n_alloced_nodes, 0)) {
                expand_nodes(NEXT_INDEX(base, tail->words[j]));
            }
            if (EXPECT(NEXT_INDEX(base, words[i]) >= n_alloced_nodes, 0)) {
                expand_nodes(NEXT_INDEX(base, words[i]));
            }

            nodes[tn_idx].base = base;

            next = NEXT_INDEX(base, tail->words[j]);
            assert(!nodes[next].prev);
            nodes[next].prev = tn_idx;
            nodes[next].base = (Index)-(tail - tails);
            tail->used_by = next;
            tail->n_words -= j + 1;
            if (tail->n_words) {
                memmove(tail->words, tail->words + j + 1,
                        tail->n_words * sizeof *tail->words);
                tail->words = (Word *)realloc(tail->words,
                                           tail->n_words * sizeof *tail->words);
                // We consider this realloc() should always succeed.
                assert(tail->words);
            } else {
                free(tail->words);
                tail->words = NULL;
            }
#ifdef START_BASE_OPTIMIZATION
            inc_next_unused_node_idx(next);
#endif

            next = NEXT_INDEX(base, words[i]);
            assert(!nodes[next].prev);
            nodes[next].prev = tn_idx;
            Index tail_idx = get_next_unused_tail_idx();
            nodes[next].base = -tail_idx;
            fill_tail(tail_idx, words + i + 1, n_words - i - 1, data, next);
#ifdef START_BASE_OPTIMIZATION
            inc_next_unused_node_idx(next);
#endif

            return;
        }
    }
    throw invalid_argument(INVARIANT_VIOLATION);
}

void _Trie::adjust(Index tn_idx, const vector<Word>& subs)
{
    Index subs_size = (Index)subs.size();
    assert(subs_size);

#ifdef INC_BASE_WHEN_COLLISION
    Index base = nodes[tn_idx].base + 1;
#elif defined(START_BASE_OPTIMIZATION)
    Index base = next_unused_node_idx;
#else
    Index base = BASE;
#endif
    for (Index i = 0; i < subs_size;) {
        Index next = NEXT_INDEX(base, subs[i]);
        if (EXPECT(next < n_alloced_nodes, 1) && nodes[next].prev) {
            base++;
            i = 0;
        } else {
            i++;
        }
    }

    move(tn_idx, subs, base - nodes[tn_idx].base);
}

void _Trie::adjust(Index tn_idx, const vector<Word>& subs, const Word& word)
{
#ifdef INC_BASE_WHEN_COLLISION
    Index base = nodes[tn_idx].base + 1;
#elif defined(START_BASE_OPTIMIZATION)
    Index base = next_unused_node_idx;
#else
    Index base = BASE;
#endif
    while (1) {
        Index next = NEXT_INDEX(base, word);
        while (EXPECT(next < n_alloced_nodes, 1) && nodes[next].prev) {
            base++;
            next++;
        }
        Index i;
        Index subs_size = (Index)subs.size();
        for (i = 0; i < subs_size; i++) {
            next = NEXT_INDEX(base, subs[i]);
            if (EXPECT(next < n_alloced_nodes, 1) && nodes[next].prev) {
                base++;
                break;
            }
        }
        if (i == subs_size) {
            break;
        }
    }

    move(tn_idx, subs, base - nodes[tn_idx].base);
}

void _Trie::move(Index tn_idx, const vector<Word>& subs, Index offset)
{
    assert(offset);

    Index subs_size = (Index)subs.size();
    for (Index i = 0; i < subs_size; i++) {
        Index base = NEXT_NODE(nodes[tn_idx].base, subs[i]).base;
        vector<Word> sub_subs;
        collect_sub_nodes(NEXT_INDEX(nodes[tn_idx].base, subs[i]), sub_subs);
        Index sub_subs_size = (Index)sub_subs.size();
        for (Index j = 0; j < sub_subs_size; j++) {
            nodes[base + sub_subs[j]].prev += offset;
        }
        Index next = NEXT_INDEX(nodes[tn_idx].base + offset, subs[i]);
        if (EXPECT(next >= n_alloced_nodes, 0)) {
            expand_nodes(next);
        }
        nodes[next] = nodes[next - offset];
        if (nodes[next].base <= 0) {
            assert(-nodes[next].base < n_alloced_tails);
            assert(tails[-nodes[next].base].used_by == next - offset);
            tails[-nodes[next].base].used_by = next;
        }
        nodes[next - offset].base = 0;
        nodes[next - offset].prev = 0;
#ifdef START_BASE_OPTIMIZATION
        dec_next_unused_node_idx(next - offset);
        inc_next_unused_node_idx(next);
#endif
    }
    nodes[tn_idx].base += offset;
}

void _Trie::collect_sub_nodes(Index tn_idx, vector<Word>& subs) const
{
    if (nodes[tn_idx].base >= BASE) {
        for (Index i = 0; i <= (Word)(~0ULL); i++) {
            Index next = NEXT_INDEX(nodes[tn_idx].base, i);
            if (EXPECT(next < n_alloced_nodes, 1) && nodes[next].prev == tn_idx) {
                subs.push_back((Word)i);
            }
        }
    }
}

void _Trie::erase(const Word words[], Index n_words)
{
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                return;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            while (j < tail->n_words && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (i < n_words) {
                return;
            }
            free(tail->words);
            memset(tail, 0, sizeof *tail);
            nodes[tn_idx].base = 0;
            nodes[tn_idx].prev = 0;
            set_next_unused_tail_idx(-base);
#ifdef START_BASE_OPTIMIZATION
            dec_next_unused_node_idx(tn_idx);
#endif
            return;
        }
    }

    Index base = nodes[tn_idx].base;
    if (base <= 0) {
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        free(tail->words);
        memset(tail, 0, sizeof *tail);
        nodes[tn_idx].base = 0;
        nodes[tn_idx].prev = 0;
        set_next_unused_tail_idx(-base);
#ifdef START_BASE_OPTIMIZATION
        dec_next_unused_node_idx(tn_idx);
#endif
        return;
    }

    erase_all_subs(tn_idx);
    if (tn_idx != ROOT) {
        nodes[tn_idx].base = 0;
        nodes[tn_idx].prev = 0;
#ifdef START_BASE_OPTIMIZATION
        dec_next_unused_node_idx(tn_idx);
#endif
    }
}

void _Trie::erase_all_subs(Index tn_idx)
{
    vector<Word> subs;
    collect_sub_nodes(tn_idx, subs);
    Index subs_size = (Index)subs.size();
    for (Index i = 0; i < subs_size; i++) {
        Index next = NEXT_INDEX(nodes[tn_idx].base, subs[i]);
        Index base = nodes[next].base;

        if (base >= BASE) {
            erase_all_subs(next);
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            free(tail->words);
            memset(tail, 0, sizeof *tail);
            set_next_unused_tail_idx(-base);
        }
        nodes[next].base = 0;
        nodes[next].prev = 0;
#ifdef START_BASE_OPTIMIZATION
        dec_next_unused_node_idx(next);
#endif
    }
}

bool _Trie::search(const Word words[], Index n_words,
                   struct _Tail **tailp) const
{
    return search(words, n_words, tailp, NULL, NULL);
}

bool _Trie::search(const Word words[], Index n_words,
                   Index *data, Index *unmatch) const
{
    return search(words, n_words, NULL, data, unmatch);
}

bool _Trie::search(const Word words[], Index n_words,
                   struct _Tail **tailp, Index *data, Index *unmatch) const
{
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                if (unmatch) {
                    *unmatch = i;
                }
                return false;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            while (j < tail->n_words && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (j == tail->n_words && i == n_words) {
                if (tailp) {
                    *tailp = tail;
                }
                if (data) {
                    *data = tail->data;
                }
                return true;
            }
            if (unmatch) {
                *unmatch = i;
            }
            return false;
        }
    }
    Index base = nodes[tn_idx].base;
    if (base >= BASE) {
        if (unmatch) {
            *unmatch = n_words;
        }
        return false;
    } else {
        assert(base <= 0);
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        if (tail->n_words) {
            if (unmatch) {
                *unmatch = n_words;
            }
            return false;
        } else {
            if (tailp) {
                *tailp = tail;
            }
            if (data) {
                *data = tail->data;
            }
            return true;
        }
    }
}

bool _Trie::segment_max_match(const Word words[], Index n_words, Word end_word,
                              Index *data, Index *unmatch) const
{
    bool find_one = false;
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index end = NEXT_INDEX(base, end_word);
            if (EXPECT(end < n_alloced_nodes, 1) && nodes[end].prev == tn_idx) {
                Index end_base = nodes[end].base;
                if (EXPECT(end_base <= 0, 1)) {
                    assert(-end_base < n_alloced_tails);

                    struct _Tail *tail = tails - end_base;
                    if (EXPECT(!tail->n_words, 1)) {
                        if (data) {
                            *data = tail->data;
                        }
                        if (unmatch) {
                            *unmatch = i;
                        }
                        find_one = true;
                    }
                }
            }
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                return find_one;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            if (!tail->n_words || tail->words[tail->n_words - 1] != end_word) {
                return find_one;
            }
            while (j < tail->n_words - 1 && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (j == tail->n_words - 1) {
                if (data) {
                    *data = tail->data;
                }
                if (unmatch) {
                    *unmatch = i;
                }
                return true;
            }
            return find_one;
        }
    }
    Index base = nodes[tn_idx].base;
    if (base >= BASE) {
        Index end = NEXT_INDEX(base, end_word);
        if (EXPECT(end < n_alloced_nodes, 1) && nodes[end].prev == tn_idx) {
            Index end_base = nodes[end].base;
            if (EXPECT(end_base <= 0, 1)) {
                assert(-end_base < n_alloced_tails);

                struct _Tail *tail = tails - end_base;
                if (EXPECT(!tail->n_words, 1)) {
                    if (data) {
                        *data = tail->data;
                    }
                    if (unmatch) {
                        *unmatch = n_words;
                    }
                    return true;
                }
            }
        }
    } else {
        assert(base <= 0);
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        if (tail->n_words == 1 && tail->words[0] == end_word) {
            if (data) {
                *data = tail->data;
            }
            if (unmatch) {
                *unmatch = n_words;
            }
            return true;
        }
    }
    return find_one;
}

bool _Trie::segment_min_match(const Word words[], Index n_words, Word end_word,
                              Index *data, Index *unmatch) const
{
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index end = NEXT_INDEX(base, end_word);
            if (EXPECT(end < n_alloced_nodes, 1) && nodes[end].prev == tn_idx) {
                Index end_base = nodes[end].base;
                if (EXPECT(end_base <= 0, 1)) {
                    assert(-end_base < n_alloced_tails);

                    struct _Tail *tail = tails - end_base;
                    if (EXPECT(!tail->n_words, 1)) {
                        if (data) {
                            *data = tail->data;
                        }
                        if (unmatch) {
                            *unmatch = i;
                        }
                        return true;
                    }
                }
            }
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                return false;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            if (!tail->n_words || tail->words[tail->n_words - 1] != end_word) {
                return false;
            }
            while (j < tail->n_words - 1 && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (j == tail->n_words - 1) {
                if (data) {
                    *data = tail->data;
                }
                if (unmatch) {
                    *unmatch = i;
                }
                return true;
            }
            return false;
        }
    }
    Index base = nodes[tn_idx].base;
    if (base >= BASE) {
        Index end = NEXT_INDEX(base, end_word);
        if (EXPECT(end < n_alloced_nodes, 1) && nodes[end].prev == tn_idx) {
            Index end_base = nodes[end].base;
            if (EXPECT(end_base <= 0, 1)) {
                assert(-end_base < n_alloced_tails);

                struct _Tail *tail = tails - end_base;
                if (EXPECT(!tail->n_words, 1)) {
                    if (data) {
                        *data = tail->data;
                    }
                    if (unmatch) {
                        *unmatch = n_words;
                    }
                    return true;
                }
            }
        }
    } else {
        assert(base <= 0);
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        if (tail->n_words == 1 && tail->words[0] == end_word) {
            if (data) {
                *data = tail->data;
            }
            if (unmatch) {
                *unmatch = n_words;
            }
            return true;
        }
    }
    return false;
}

void _Trie::prefix(const Word words[], Index n_words, vector<Index>& results) const
{
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                return;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            while (j < tail->n_words && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (i < n_words) {
                return;
            }

            results.push_back(tail->data);
            return;
        }
    }

    Index base = nodes[tn_idx].base;
    if (base <= 0) {
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        results.push_back(tail->data);
        return;
    }

    assert(base >= BASE);

    collect_all_subs(tn_idx, results);
}

void _Trie::collect_all_subs(Index tn_idx, vector<Index>& results) const
{
    vector<Word> subs;
    collect_sub_nodes(tn_idx, subs);
    Index subs_size = (Index)subs.size();
    for (Index i = 0; i < subs_size; i++) {
        Index next = NEXT_INDEX(nodes[tn_idx].base, subs[i]);
        Index base = nodes[next].base;

        if (base >= BASE) {
            collect_all_subs(next, results);
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            results.push_back(tail->data);
        }
    }
}

void _Trie::prefix(const Word words[], Index n_words, vector<Tail>& results) const
{
    Index tn_idx = ROOT;
    for (Index i = 0; i < n_words; i++) {
        Index base = nodes[tn_idx].base;
        if (base >= BASE) {
            Index next = NEXT_INDEX(base, words[i]);
            if (EXPECT(next >= n_alloced_nodes, 0) || nodes[next].prev != tn_idx) {
                return;
            }
            tn_idx = next;
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Index j = 0;

            while (j < tail->n_words && i < n_words) {
                if (tail->words[j] != words[i]) {
                    break;
                }
                j++;
                i++;
            }
            if (i < n_words) {
                return;
            }

            Tail result;
            result.n_words = tail->n_words - j;
            if (result.n_words) {
                result.words = (Word *)malloc(result.n_words * sizeof *result.words);
                if (!result.words) {
                    throw bad_alloc();
                }
                memcpy(result.words, tail->words + j,
                       result.n_words * sizeof *result.words);
            } else {
                result.words = NULL;
            }
            result.data = tail->data;
            results.push_back(result);
            return;
        }
    }

    Index base = nodes[tn_idx].base;
    if (base <= 0) {
        assert(-base < n_alloced_tails);

        struct _Tail *tail = tails - base;
        Tail result;
        result.n_words = tail->n_words;
        if (result.n_words) {
            result.words = (Word *)malloc(result.n_words * sizeof *result.words);
            if (!result.words) {
                throw bad_alloc();
            }
            memcpy(result.words, tail->words, result.n_words * sizeof *result.words);
        } else {
            result.words = NULL;
        }
        result.data = tail->data;
        results.push_back(result);
        return;
    }

    assert(base >= BASE);

    Tail result;
    Index n_alloced_words = PRE_ALLOCED_WORDS;
    result.words = (Word *)malloc(n_alloced_words * sizeof *result.words);
    if (!result.words) {
        throw bad_alloc();
    }
    result.n_words = 0;
    result.data = 0;
    collect_all_subs(tn_idx, &result, n_alloced_words, results);
    free(result.words);
}

void _Trie::collect_all_subs(Index tn_idx, struct Tail *result,
                             Index n_alloced_words, vector<Tail>& results) const
{
    vector<Word> subs;
    collect_sub_nodes(tn_idx, subs);
    Index subs_size = (Index)subs.size();
    if (!subs_size) {
        return;
    }

    Index next_word_idx = result->n_words;

    if (EXPECT(next_word_idx >= n_alloced_words, 0)) {
        Word *old_words = result->words;
        assert(n_alloced_words);
        n_alloced_words = n_alloced_words << 1;
        result->words = (Word *)realloc(result->words,
                                       n_alloced_words * sizeof *result->words);
        if (!result->words) {
            free(old_words);
            throw bad_alloc();
        }
    }

    for (Index i = 0; i < subs_size; i++) {
        Index next = NEXT_INDEX(nodes[tn_idx].base, subs[i]);
        Index base = nodes[next].base;

        result->words[result->n_words++] = subs[i];
        if (base >= BASE) {
            collect_all_subs(next, result, n_alloced_words, results);
        } else {
            assert(base <= 0);
            assert(-base < n_alloced_tails);

            struct _Tail *tail = tails - base;
            Tail tmp;
            tmp.n_words = result->n_words + tail->n_words;
            tmp.words = (Word *)malloc(tmp.n_words * sizeof *tmp.words);
            if (!tmp.words) {
                free(result->words);
                throw bad_alloc();
            }
            memcpy(tmp.words, result->words, result->n_words * sizeof *tmp.words);
            memcpy(tmp.words + result->n_words, tail->words,
                   tail->n_words * sizeof *tmp.words);
            tmp.data = tail->data;
            results.push_back(tmp);
        }
        result->n_words = next_word_idx;
    }
}



Trie::Trie(void)
{
    trie = new _Trie;
}

Trie::~Trie(void)
{
    delete (_Trie *)trie;
}

void Trie::insert(const Word words[], Index n_words, Index data)
{
    _Trie *native = (_Trie *)trie;
    native->insert(words, n_words, data);
}

void Trie::erase(const Word words[], Index n_words)
{
    _Trie *native = (_Trie *)trie;
    native->erase(words, n_words);
}

bool Trie::search(const Word words[], Index n_words,
                  Index *data, Index *unmatch) const
{
    _Trie *native = (_Trie *)trie;
    return native->search(words, n_words, data, unmatch);
}

void Trie::prefix(const Word words[], Index n_words, vector<Index>& tails) const
{
    _Trie *native = (_Trie *)trie;
    native->prefix(words, n_words, tails);
}

void Trie::prefix(const Word words[], Index n_words, vector<Tail>& tails) const
{
    _Trie *native = (_Trie *)trie;
    native->prefix(words, n_words, tails);
}

bool Trie::segment_max_match(const Word words[], Index n_words, Word end_word,
                             Index *data, Index *unmatch) const
{
    _Trie *native = (_Trie *)trie;
    return native->segment_max_match(words, n_words, end_word, data, unmatch);
}

bool Trie::segment_min_match(const Word words[], Index n_words, Word end_word,
                             Index *data, Index *unmatch) const
{
    _Trie *native = (_Trie *)trie;
    return native->segment_min_match(words, n_words, end_word, data, unmatch);
}
