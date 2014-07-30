#ifndef _trie_h_
#define _trie_h_

#include <vector>

typedef int Index; // assert((Index)-1 < 0)
// Change Word to other bigger types is not recommended.
// (Will slow down the algorithm.)
typedef unsigned char Word; // assert((Word)-1 > 0)

struct Tail {
    Word *words;
    Index n_words;
    Index data;
};

class Trie {
    void *trie;

public:
    Trie(void);
    ~Trie(void);

    void insert(const Word words[], Index n_words, Index data=0);
    void erase(const Word words[], Index n_words);
    // @data returns data correspond to @words stored in trie when @words is found;
    // @unmatch returns the index of the first unmatch word in @words when @words is not found.
    bool search(const Word words[], Index n_words, Index *data=NULL, Index *unmatch=NULL) const;
    void prefix(const Word words[], Index n_words, std::vector<Index>& tails) const;
    // @tails return all tails begin with @words.
    // Don't forget to free memory in @tails[]->words.
    void prefix(const Word words[], Index n_words, std::vector<Tail>& tails) const;
    bool segment_max_match(const Word words[], Index n_words, Word end_word,
                           Index *data, Index *unmatch) const;
    bool segment_min_match(const Word words[], Index n_words, Word end_word,
                           Index *data, Index *unmatch) const;
};

#endif
