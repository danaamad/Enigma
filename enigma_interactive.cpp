#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

// חיווט רוטורים Enigma I / M3
const char* ROTOR_STR[3] = {
    "EKMFLGDQVZNTOWYHXUSPAIBRCJ", // I
    "AJDKSIRUXBLHWTMCQGZNPYFVOE", // II
    "BDFHJLCPRTXVZNYEIWGAKMUSQO"  // III
};
const int ROTOR_NOTCH[3] = { 16, 4, 21 }; // Q, E, V
const char* REFL_B_STR = "YRUHQSLDPXNGOKMIEBFZCWVJAT";

struct FastEnigmaTables {
    uint8_t forward[3][26];
    uint8_t backward[3][26];
    uint8_t refl[26];

    FastEnigmaTables() {
        for (int r = 0; r < 3; ++r) {
            for (int i = 0; i < 26; ++i) {
                forward[r][i] = ROTOR_STR[r][i] - 'A';
                backward[r][ROTOR_STR[r][i] - 'A'] = i;
            }
        }
        for (int i = 0; i < 26; ++i) {
            refl[i] = REFL_B_STR[i] - 'A';
        }
    }
};

static const FastEnigmaTables TABLES;

inline void decrypt_text(
    const uint8_t* in,
    uint8_t* out,
    int len,
    int p0, int p1, int p2,
    const uint8_t* plug
) {
    const int n1 = ROTOR_NOTCH[1];
    const int n2 = ROTOR_NOTCH[2];

    for (int k = 0; k < len; ++k) {
        bool mid_step = false;
        bool left_step = false;

        if (p1 == n1) {
            mid_step = true;
            left_step = true;
        } else if (p2 == n2) {
            mid_step = true;
        }

        p2 = (p2 + 1) % 26;
        if (mid_step) p1 = (p1 + 1) % 26;
        if (left_step) p0 = (p0 + 1) % 26;

        uint8_t c = plug[in[k]];

        c = (TABLES.forward[2][(c + p2) % 26] - p2 + 26) % 26;
        c = (TABLES.forward[1][(c + p1) % 26] - p1 + 26) % 26;
        c = (TABLES.forward[0][(c + p0) % 26] - p0 + 26) % 26;

        c = TABLES.refl[c];

        c = (TABLES.backward[0][(c + p0) % 26] - p0 + 26) % 26;
        c = (TABLES.backward[1][(c + p1) % 26] - p1 + 26) % 26;
        c = (TABLES.backward[2][(c + p2) % 26] - p2 + 26) % 26;

        out[k] = plug[c];
    }
}

inline double calc_ioc(const uint8_t* text, int len) {
    if (len <= 1) return 0.0;
    int counts[26] = {0};
    for (int i = 0; i < len; ++i) counts[text[i]]++;
    int sum = 0;
    for (int i = 0; i < 26; ++i) sum += counts[i] * (counts[i] - 1);
    return (double)sum / (len * (len - 1));
}

int score_language(const uint8_t* text, int len) {
    static int bg[26][26] = {0};
    static bool init = false;
    if (!init) {
        const char* pairs[] = {
            "ER", "EN", "CH", "DE", "EI", "TE", "IN", "ND", "IE", "GE",
            "ES", "NE", "UN", "RE", "AN", "ST", "HE", "DI", "NG", "BE",
            "CK", "ZU", "HA", "RA", "AU", "SE", "IC", "HT", "SI", "VO"
        };
        const int scores[] = {
            50, 45, 35, 32, 28, 26, 24, 22, 22, 20,
            18, 18, 16, 16, 15, 15, 14, 13, 12, 11,
            10, 10, 10,  9,  9,  9, 16, 14, 12, 10
        };
        for (int i = 0; i < 30; ++i) {
            bg[pairs[i][0] - 'A'][pairs[i][1] - 'A'] = scores[i];
        }
        init = true;
    }

    int score = 0;
    for (int i = 0; i < len - 1; ++i) {
        score += bg[text[i]][text[i + 1]];
    }
    return score;
}

struct Candidate {
    double ioc;
    int p0, p1, p2;
};

int main() {
#ifdef _WIN32
    // הגדרת קידוד UTF-8 להצגה תקינה של עברית במסוף Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "====================================================\n";
    std::cout << "       מפצח שדרי אניגמה מודרני (Enigma I / M3)       \n";
    std::cout << "====================================================\n\n";

    std::cout << "הדבק את הטקסט המוצפן ולחץ Enter:\n> ";
    std::string raw_cipher;
    std::getline(std::cin, raw_cipher);

    // סינון תווים: המרה ל-0-25 והתעלמות מרווחים וסימנים
    std::vector<uint8_t> cipher;
    for (char c : raw_cipher) {
        if (c >= 'A' && c <= 'Z') cipher.push_back(c - 'A');
        else if (c >= 'a' && c <= 'z') cipher.push_back(c - 'a');
    }

    int len = cipher.size();
    if (len < 15) {
        std::cout << "\n[-] שגיאה: השדר קצר מדי לפיצוח סטטיסטי (נדרשות לפחות 15 אותיות).\n";
        return 1;
    }

    std::cout << "\n[+] נקלט שדר באורך: " << len << " תווים.\n";
    std::cout << "[+] שלב 1: סורק 17,576 מצבי רוטורים במקביל...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    uint8_t default_plug[26];
    for (int i = 0; i < 26; ++i) default_plug[i] = i;

    std::vector<Candidate> candidates;

    #pragma omp parallel
    {
        std::vector<uint8_t> out_buf(len);
        std::vector<Candidate> local_cand;

        #pragma omp for collapse(3) schedule(dynamic)
        for (int p0 = 0; p0 < 26; ++p0) {
            for (int p1 = 0; p1 < 26; ++p1) {
                for (int p2 = 0; p2 < 26; ++p2) {
                    decrypt_text(cipher.data(), out_buf.data(), len, p0, p1, p2, default_plug);
                    double ioc = calc_ioc(out_buf.data(), len);
                    if (ioc > 0.043) {
                        local_cand.push_back({ioc, p0, p1, p2});
                    }
                }
            }
        }

        #pragma omp critical
        candidates.insert(candidates.end(), local_cand.begin(), local_cand.end());
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.ioc > b.ioc;
    });

    int top_n = std::min((int)candidates.size(), 100);
    std::cout << "[+] זוהו " << candidates.size() << " מועמדים חשודים. בודק תקעים עבור " << top_n << " המובילים...\n";

    int best_score = -1;
    int best_pos[3] = {0};
    uint8_t best_plug[26];
    std::vector<uint8_t> best_decrypted(len);
    std::vector<uint8_t> out_buf(len);

    for (int r = 0; r < top_n; ++r) {
        int p0 = candidates[r].p0;
        int p1 = candidates[r].p1;
        int p2 = candidates[r].p2;

        uint8_t plug[26];
        for (int i = 0; i < 26; ++i) plug[i] = i;

        decrypt_text(cipher.data(), out_buf.data(), len, p0, p1, p2, plug);
        int cur_score = score_language(out_buf.data(), len);

        bool improved = true;
        int iters = 0;

        while (improved && iters < 12) {
            improved = false;
            iters++;

            for (int i = 0; i < 26; ++i) {
                for (int j = i + 1; j < 26; ++j) {
                    uint8_t test_plug[26];
                    std::memcpy(test_plug, plug, 26);

                    uint8_t oi = test_plug[i];
                    uint8_t oj = test_plug[j];
                    test_plug[oi] = oi;
                    test_plug[oj] = oj;
                    test_plug[i] = j;
                    test_plug[j] = i;

                    decrypt_text(cipher.data(), out_buf.data(), len, p0, p1, p2, test_plug);
                    int s = score_language(out_buf.data(), len);

                    if (s > cur_score + 8) {
                        cur_score = s;
                        std::memcpy(plug, test_plug, 26);
                        improved = true;
                        break;
                    }
                }
                if (improved) break;
            }
        }

        if (cur_score > best_score) {
            best_score = cur_score;
            best_pos[0] = p0; best_pos[1] = p1; best_pos[2] = p2;
            std::memcpy(best_plug, plug, 26);
            decrypt_text(cipher.data(), best_decrypted.data(), len, p0, p1, p2, best_plug);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "\n================ תוצאת הפיצוח המלאה ================\n";
    std::cout << "זמן ריצה כולל: " << elapsed_ms << " מילישניות (" << elapsed_ms / 1000.0 << " שניות)\n";
    std::cout << "מיקום רוטורים: " 
              << (char)('A' + best_pos[0]) 
              << (char)('A' + best_pos[1]) 
              << (char)('A' + best_pos[2]) << "\n";

    std::cout << "לוח תקעים:     ";
    bool has_plugs = false;
    for (int i = 0; i < 26; ++i) {
        if (best_plug[i] > i) {
            std::cout << (char)('A' + i) << (char)('A' + best_plug[i]) << " ";
            has_plugs = true;
        }
    }
    if (!has_plugs) std::cout << "ללא תקעים";

    std::cout << "\nציון שפה:      " << best_score << "\n";
    std::cout << "----------------------------------------------------\n";
    for (int i = 0; i < len; ++i) {
        std::cout << (char)('A' + best_decrypted[i]);
    }
    std::cout << "\n====================================================\n";

    return 0;
}