#include <iostream>
#include <string>
#include <set>
#include <map>
#include <queue>
#include <vector>
#include <utility>
#include <tuple>
#include <cassert>
#include <fstream>
using namespace std;

vector<string> keys{"Address_bits:", "Block_size:", "Cache_sets:", "Associativity:"};

int log2(unsigned int n) {
    unsigned int l = 0;
    if (n == 0) return -1;
    while (n >>= 1) { l++; };
    return l;
}

unsigned int pow2(int x) {
    if (x == -1) return 0;
    return (1 << x);
}

int min(int a, int b) {return (a > b) ? b : a;};
int max(int a, int b) {return (a > b) ? a : b;};

map<string, int> parse_params(string path) {
    // Open file stream for parameters
    ifstream f;
    f.open(path, ios::in);

    map<string, int> r;
    string key;
    int value;
    while(!f.eof()) {
        f >> key >> value;
        r[key] = value;
    }
    /*fparams >> key >> r[0];
    assert(key == "Address_bits:");
    fparams >> key >> r[1];
    assert(key == "Block_size:");
    fparams >> key >> r[2];
    assert(key == "Cache_sets:");
    fparams >> key >> r[3];
    assert(key == "Associativity:");*/
    return r;
}

void print_params(ostream &out, map<string, int> &params) {
    out << "Address bits: " << params[keys[0]] << endl;
    out << "Cache sets: " << params[keys[2]] << endl;
    out << "Associativity: " << params[keys[3]] << endl;
    out << "Block size: " << params[keys[1]] << endl;
    out << endl;
}

vector<string> parse_refs(string path, string &tcname, unsigned int lenaddr) {
    // Open file stream for references
    ifstream f;
    f.open(path, ios::in);

    vector<string> r;
    string in;

    f >> in;
    assert(in == ".benchmark");
    f >> tcname;
    while (!f.eof()) {
        f >> in;
        if (in == ".end") break;
        r.push_back(in);
        assert(in.length() == lenaddr);
    }

    return r;
}

template<typename Container>
void print(Container& c) {
    cout << "===" << endl;
    for (auto it: c)
        cout << it << endl;
    cout << "===" << endl;
}

template<typename Container>
void print2d(Container& c) {
    cout << "===" << endl;
    for (auto row : c) {
        for (auto cell: row)
            cout << cell << ' ';
        cout << endl;
    }
    cout << "===" << endl;
}

vector<int> count_1(set<string> refs, int len) {
    vector<int> r(len, 0);

    for (auto addr: refs) {
        for (int i=0; i < len; i++)
            if (addr[i] == '1') r[i]++;
    }

    return r;
}

vector<double> calc_quality(vector<int> ones, int nrefs) {
    vector<double> qvec;
    for (auto onecnt: ones) {
        qvec.push_back((double)min(onecnt, nrefs-onecnt) /
                (double)max(onecnt, nrefs-onecnt));
    }

    return qvec;
}

unsigned int count_diff(set<string> refs, unsigned int i, unsigned int j) {
    unsigned int r = 0;
    for (auto addr: refs) {
        r += ((addr[i] - '0') ^ (addr[j] - '0'));
    }
    return r;
}

// Params: references, and length of the indexable part of each address
vector<vector<int>> calc_difftable(set<string> refs, int len) {
    vector<vector<int>> diff(len,
            vector<int>(len, 0));

    for (int i=0; i < len; i++)
        for (int j=0; j < len; j++) {
            if (i == j) continue;
            if (i > j) {
                diff[i][j] = diff[j][i];
                continue;
            }

        diff[i][j] = count_diff(refs, i, j);
        }

    return diff;
}

vector<vector<double>> calc_corr(vector<vector<int>> diff, int nrefs) {
    int len = diff.size();
    vector<vector<double>> corr(len,
            vector<double>(len, 0));

    for (int i=0; i < len; i++)
        for (int j=0; j < len; j++) {
            if (i == j) continue;
            if (i > j) {
                corr[i][j] = corr[j][i];
                continue;
            }

        corr[i][j] = ((double)min(diff[i][j], nrefs - diff[i][j]) /
                (double)max(diff[i][j], nrefs - diff[i][j]));
        }

    return corr;
}

vector<int> compute(vector<double> quality, vector<vector<double>> corr) {
    vector<int> r;
    assert(corr.size() == corr[0].size());
    assert(quality.size() == corr.size());
    int len = quality.size();

    set<int> S;
    for (int k=0; k < len; k++) {
        int best = -1;
        for (int i=len-1; i >= 0; i--) {
            if (S.count(i) == 0) {
                if (best == -1) {
                    best = i;
                    continue;
                }
                if (quality[i] > quality[best]) {
                    best = i;
                }
            }
        }
        assert(best != -1);
        S.insert(best);

        for (int j=0; j < len; j++)
            quality[j] = quality[j] * corr[best][j];

        //cout << "iteration" << k << endl;
        //print(quality);

        r.push_back(best);
    }
    return r;
}

// Params: The address, the vector of indexing bits
unsigned int toindex(string addr, vector<int> indexes) {
    unsigned int r = 0;
    for (unsigned int i=0; i < indexes.size(); i++) {
        r += (addr[indexes[i]] - '0') << i;
    }
    return r;
}

int main(int argc, char* argv[]) {
    assert(argc==4);

    // Read the file path from CLI args
    string params_path(argv[1]), refs_path(argv[2]), output_path(argv[3]);

    // Initialize output file stream
    ofstream fout;
    fout.open(output_path, ios::out);

    // Read the set of params
    auto params = parse_params(params_path);

    // Print the params to output
    print_params(fout, params);
    //print_params(cout, params);

    // Get aliases for params
    unsigned int lenaddr = params[keys[0]];
    unsigned int lenblk = params[keys[1]];
    unsigned int nsets = params[keys[2]];
    unsigned int assoc = params[keys[3]];
    unsigned int idxable = lenaddr - log2(lenblk);

    // Read the list of references
    string tcname;
    auto refs = parse_refs(refs_path, tcname, lenaddr);
    // Get unique references
    set<string> urefs(refs.begin(), refs.end());
    unsigned int nurefs = urefs.size();

    // Compute optimal set index by Givargis (2006)'s Algorithm
    auto ones = count_1(urefs, idxable); // print(urefs);
    auto quality = calc_quality(ones, nurefs); // print(quality);

    auto diff = calc_difftable(urefs, idxable); // print2d(diff);
    auto corr = calc_corr(diff, nurefs); // print2d(corr);

    auto indexes = compute(quality, corr); // print(ordered);
    indexes.resize(log2(nsets)); // truncate to the first log2(nsets)

    fout << "Indexing bit count: " << log2(nsets) << endl;
    fout << "Indexing bits:";
    for (auto b: indexes)
        fout << " " << lenaddr - b - 1;
    fout << endl;
    fout << "Offset bit count: " << log2(lenblk) << endl;
    fout << endl;

    // Start cache simulation

    // Initialize cache space
    vector<deque<string>> cache(nsets);
    fout << ".benchmark " << tcname << endl;

    unsigned int miss_count = 0;
    for (auto& ref: refs) {
        int index = toindex(ref, indexes);
        string tag = ref.substr(0, idxable);

        bool hit = 0;
        auto it = cache[index].begin();
        for (auto t: cache[index]) {
            if (t == tag) {
                hit = 1;
                cache[index].erase(it);
                cache[index].push_back(tag);
                break;
            }
            it++;
        }
        if (!hit) {
            while (cache[index].size() >= assoc) {
                cache[index].pop_front();
            }
            cache[index].push_back(tag);
            miss_count++;
        }
        fout << ref << " " << ((hit) ? "hit" : "miss") << endl;
        //print2d(cache);
    }
    fout << ".end" << endl;
    fout << endl;

    fout << "Total cache miss count: " << miss_count << endl;

    return 0;
}
