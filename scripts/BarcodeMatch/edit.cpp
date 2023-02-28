#include "edit.h"
using namespace std;

vector<string> reverse(vector<string> s) {
    int n = s.size();
    for (int i = 0; i < n; i++) {
        reverse(s[i].begin(), s[i].end());
    }
    return(s);
}

set<string> kmer(vector<string> s, int k) {
    int n = s.size();
    set<string> out;

    for (int i = 0; i < n; i++) {
        int l = s[i].length();
        for (int j = 0; j < l - k + 1; j++) {
            out.insert(s[i].substr(j, k));
        }
    }

    return(out);
}

vector<string> kmer(string s, int k) {
    int s_len = s.length();
    vector<string> out;

    for (int i = 0; i < s_len - k + 1; i++) {
        out.push_back(s.substr(i, k));
    }
    return(out);
}

int editDist(string word1, string word2) {

    vector<vector<int>> dp = vector<vector<int>>(word1.size() + 1, vector<int>(word2.size() + 1, 0));

    for (int i = 0; i <= word1.size(); i++) {
        dp[i][0] = i;
    }

    for (int j = 1; j <= word2.size(); j++) {
        dp[0][j] = j;
    }

    for (int i = 0; i < word1.size(); i++) {
        for (int j = 0; j < word2.size(); j++) {
            if (word1[i] == word2[j]) {
                dp[i + 1][j + 1] = dp[i][j];
            }
            else {
                dp[i + 1][j + 1] = min(min(dp[i][j + 1], dp[i + 1][j]), dp[i][j]) + 1;
            }
        }
    }

    return dp[word1.size()][word2.size()];
}

pair<int, int> traceback(vector<vector<int>> dp) {
    int nrow = dp.size();
    int ncol = dp[0].size();

    int minEdit = nrow;
    int pos = 0;

    for (int i = 0; i < ncol; i++) {
        if (dp[nrow - 1][i] < minEdit) {
            minEdit = dp[nrow - 1][i];
            pos = i;
        }
    }

    int loc = nrow - 1;
    while (loc > 1) {
        int temp = min(min(dp[loc][pos-1], dp[loc-1][pos]), dp[loc-1][pos-1]);

        if (dp[loc - 1][pos] == temp) {
            loc = loc - 1;
        }
        else if (dp[loc - 1][pos - 1] == temp) {
            loc = loc - 1;
            pos = pos - 1;
        }
        else {
            pos = pos - 1;
        }
    }
    return(pair<int, int>(minEdit, pos));
}

pair<int, int> minEditDist(string seq, string barcode) {
    int bs = barcode.size();
    int ss = seq.size();

    vector<vector<int>> dp = vector<vector<int>>(bs + 1, vector<int>(ss + 1, 0));

    for (int i = 0; i <= bs; i++) {
        dp[i][0] = i;
    }

    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < ss; j++) {
            if (barcode[i] == seq[j]) {
                dp[i + 1][j + 1] = dp[i][j];
            }
            else {
                dp[i + 1][j + 1] = min(min(dp[i][j + 1], dp[i + 1][j]), dp[i][j]) + 1;
            }
        }
    }
    /*
    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < ss; j++) {
            cout << dp[i][j] << " ";
        }
        cout << endl;
    }
    */
    return traceback(dp);
}
