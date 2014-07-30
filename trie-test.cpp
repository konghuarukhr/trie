#include "trie.h"
#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>

// Every line should be different in this test.
#define DICT "dict.txt"

using namespace std;

bool g_exit = false;
int g_rand = 0;

void handle_sigusr1(int sig)
{
    g_exit = true;
}

bool load_dict(const char *fname, vector<string>& dict)
{
    ifstream dict_file(fname);

    if (!dict_file.is_open()) {
        return false;
    }

    string line;
    while (getline(dict_file, line)) {
        dict.push_back(line);
    }

    dict_file.close();

    return true;
}

void set_random(int rand)
{
    g_rand = rand;
}

bool random_sort(const string& s1, const string& s2)
{
    int idx = 0;
    int len = s1.size() < s2.size() ? s1.size() : s2.size();
    if (len) {
        idx = g_rand % len;
    }
    return string(s1, idx, string::npos) < string(s2, idx, string::npos);
}

void test(vector<string>& dict)
{
    clock_t begin, end;
    unsigned dict_size = dict.size();
    Trie trie;
    vector<Tail> tails;
    Index data;
    unsigned i, j;
    int loop;

    for (loop = 0; loop < 5; loop++) {
        set_random(clock());
        sort(dict.begin(), dict.end(), random_sort);

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            trie.insert((Word *)dict[i].c_str(), dict[i].size() + 1, i + 1);
        }
        end = clock();
        cout << "INSERT" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            if (!trie.search((Word *)dict[i].c_str(), dict[i].size() + 1, &data)) {
                cout << "error: SEARCH DATA" << ": " << dict[i] << " not found" << endl;
                exit(-2);
            }
            if (data != (Index)i + 1) {
                cout << "error: SEARCH DATA" << ": " << dict[i] << " data changed: " << i << "=>" << data << endl;
                exit(-2);
            }
        }
        end = clock();
        cout << "SEARCH DATA" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        set_random(clock());
        sort(dict.begin(), dict.end(), random_sort);

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            if (!trie.search((Word *)dict[i].c_str(), dict[i].size() + 1)) {
                cout << "error: SEARCH" << ": " << dict[i] << " not found" << endl;
                exit(-2);
            }
        }
        end = clock();
        cout << "SEARCH" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        tails.clear();
        begin = clock();
        trie.prefix(NULL, 0, tails);
        end = clock();
        if (tails.size() != dict_size) {
            cout << "error: SEARCH ALL" << ": " << "size changed: " << dict_size << "=>" << tails.size() << endl;
            exit(-2);
        }
        cout << "SEARCH ALL" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
        for (j = 0; j < tails.size(); j++) {
            if (!tails[j].data) {
                cout << "error: SEARCH ALL" << ": " << "data corrupt" << endl;
                exit(-2);
            }
            if (tails[j].n_words && !tails[j].words) {
                cout << "error: SEARCH ALL" << ": " << "words number != 0 but words == NULL" << endl;
                exit(-2);
            }
            if (!tails[j].n_words && tails[j].words) {
                cout << "error: SEARCH ALL" << ": " << "words number == 0 but words != NULL" << endl;
                exit(-2);
            }
            if (tails[j].n_words) {
                free(tails[j].words);
            }
        }

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            tails.clear();
            trie.prefix((Word *)dict[i].c_str(), dict[i].size(), tails);
            if (tails.size() != 1) {
                cout << "error: SEARCH PREFIX" << ": " << dict[i] << " not found" << endl;
                exit(-2);
            }
            for (j = 0; j < tails.size(); j++) {
                if (!tails[j].data) {
                    cout << "error: SEARCH PREFIX" << ": " << "data corrupt" << endl;
                    exit(-2);
                }
                if (tails[j].n_words && !tails[j].words) {
                    cout << "error: SEARCH PREFIX" << ": " << "words number != 0 but words == NULL" << endl;
                    exit(-2);
                }
                if (!tails[j].n_words && tails[j].words) {
                    cout << "error: SEARCH PREFIX" << ": " << "words number == 0 but words != NULL" << endl;
                    exit(-2);
                }
                if (tails[j].n_words) {
                    free(tails[j].words);
                }
            }
        }
        end = clock();
        cout << "SEARCH PREFIX" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            trie.erase((Word *)dict[i].c_str(), dict[i].size() + 1);
        }
        end = clock();
        cout << "ERASE" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        begin = clock();
        for (i = 0; i < dict_size; i++) {
            if (trie.search((Word *)dict[i].c_str(), dict[i].size() + 1)) {
                cout << "error: SEARCH AFTER ERASE" << ": " << dict[i] << " found" << endl;
                exit(-2);
            }
        }
        end = clock();
        cout << "SEARCH AFTER ERASE" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        tails.clear();
        begin = clock();
        trie.prefix(NULL, 0, tails);
        end = clock();
        if (tails.size() != 0) {
            cout << "error: SEARCH ALL AFTER ERASE" << ": " << "size not zero" << endl;
            exit(-2);
        }
        cout << "SEARCH ALL AFTER ERASE" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;

        begin = clock();
        trie.erase(NULL, 0);
        end = clock();
        cout << "ERASE ALL" << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
    }
}

int main(void)
{
    clock_t begin, end;

    signal(SIGUSR1, handle_sigusr1);

    vector<string> dict;
    if (!load_dict(DICT, dict)) {
        cerr << "Need a dictionary file called " << DICT << " in the current directory" << endl;
        return -1;
    }
    cout << "DICT SIZE: " << dict.size() << endl;

    int i = 1;
    while (!g_exit) {
        begin = clock();
        test(dict);
        end = clock();
        cout << i << ": " << (end - begin) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
        i++;
    }

    dict.clear();

    return 0;
}
