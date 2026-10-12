#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cctype>
#include <omp.h>

const char* WIRING_I   = "EKMFLGDQVZNTOWYHXUSPAIBRCJ";
const char* WIRING_II  = "AJDKSIRUXBLHWTMCQGZNPYFVOE";
const char* WIRING_III = "BDFHJLCPRTXVZNYEIWGAKMUSQO";
const char* REFLECT_B  = "YRUHQSLDPXNGOKMIEBFZCWVJAT";

const int NOTCH_I   = 16; // Q
const int NOTCH_II  = 4;  // E
const int NOTCH_III = 21; // V

struct EnigmaEngine {
    int fwd[3][26];
    int rev[3][26];
    int ref[26];
    int notch[3];

    void init() {
        auto setup = [&](int idx, const char* map, int n_val) {
            notch[idx] = n_val;
            for (int i = 0; i < 26; ++i) {
                int target = map[i] - 'A';
                fwd[idx][i] = target;
                rev[idx][target] = i;
            }
        };
        setup(0, WIRING_I, NOTCH_I);
        setup(1, WIRING_II, NOTCH_II);
        setup(2, WIRING_III, NOTCH_III);

        for (int i = 0; i < 26; ++i) {
            ref[i] = REFLECT_B[i] - 'A';
        }
    }
};

static EnigmaEngine engine;

alignas(64) float BIGRAM_SCORES[26][26];

void init_bigrams() {
    // אתחול ברירת מחדל אחידה
    for (int i = 0; i < 26; ++i)
        for (int j = 0; j < 26; ++j)
            BIGRAM_SCORES[i][j] = -4.5f;

    // ביגרמות תכופות בגרמנית עם ערכי Log מנורמלים
    auto set_bi = [](const char* bi, float val) {
        BIGRAM_SCORES[bi[0] - 'A'][bi[1] - 'A'] = val;
    };

    set_bi("ER", -1.3f); set_bi("EN", -1.4f); set_bi("CH", -1.5f); set_bi("DE", -1.6f);
    set_bi("EI", -1.6f); set_bi("TE", -1.7f); set_bi("IN", -1.7f); set_bi("ND", -1.8f);
    set_bi("IE", -1.8f); set_bi("GE", -1.9f); set_bi("ES", -2.0f); set_bi("NE", -2.0f);
    set_bi("UN", -2.0f); set_bi("ST", -2.1f); set_bi("RE", -2.1f); set_bi("HE", -2.2f);
    set_bi("AN", -2.2f); set_bi("TI", -2.3f); set_bi("DI", -2.3f); set_bi("RA", -2.4f);
    set_bi("BE", -2.4f); set_bi("IC", -2.4f); set_bi("OR", -2.5f); set_bi("SE", -2.5f);
    set_bi("SC", -2.2f); set_bi("NG", -2.3f); set_bi("LL", -2.6f); set_bi("CK", -2.7f);
    set_bi("ZU", -2.7f); set_bi("VO", -2.8f); set_bi("ZE", -2.8f); set_bi("ZW", -3.0f);
    set_bi("WE", -2.5f); set_bi("EI", -1.6f); set_bi("IX", -3.2f); set_bi("EX", -3.1f);
}

inline void decrypt_cipher(const uint8_t* in, uint8_t* out, int len,
                           int r0, int r1, int r2,
                           const uint8_t* plugboard) {
    for (int i = 0; i < len; ++i) {
        bool mid_step = (r2 == engine.notch[2]);
        bool left_step = (r1 == engine.notch[1]);

        if (mid_step || left_step) {
            r1 = (r1 + 1) % 26;
            if (left_step) {
                r0 = (r0 + 1) % 26;
            }
        }
        r2 = (r2 + 1) % 26;

        int ch = plugboard[in[i]];
        ch = (engine.fwd[2][(ch + r2) % 26] - r2 + 26) % 26;
        ch = (engine.fwd[1][(ch + r1) % 26] - r1 + 26) % 26;
        ch = (engine.fwd[0][(ch + r0) % 26] - r0 + 26) % 26;

        ch = engine.ref[ch];

        ch = (engine.rev[0][(ch + r0) % 26] - r0 + 26) % 26;
        ch = (engine.rev[1][(ch + r1) % 26] - r1 + 26) % 26;
        ch = (engine.rev[2][(ch + r2) % 26] - r2 + 26) % 26;

        out[i] = plugboard[ch];
    }
}

inline float score_text(const uint8_t* text, int len) {
    float score = 0.0f;
    for (int i = 0; i < len - 1; ++i) {
        score += BIGRAM_SCORES[text[i]][text[i + 1]];
    }
    return score;
}

inline float calculate_ioc(const uint8_t* text, int len) {
    if (len < 2) return 0.0f;
    int counts[26] = {0};
    for (int i = 0; i < len; ++i) counts[text[i]]++;
    int sum = 0;
    for (int i = 0; i < 26; ++i) {
        sum += counts[i] * (counts[i] - 1);
    }
    return static_cast<float>(sum) / static_cast<float>(len * (len - 1));
}

struct RotorCandidate {
    int r0, r1, r2;
    float score;
};

// פונקציית החלפת תקעים בטוחה ב-100% ללא דריסת זיכרון
void safe_swap_plugs(const uint8_t* src, uint8_t* dst, int a, int b) {
    for (int k = 0; k < 26; ++k) dst[k] = src[k];

    int partner_a = dst[a];
    int partner_b = dst[b];

    // ניתוק זוגות קודמים
    dst[partner_a] = partner_a;
    dst[partner_b] = partner_b;

    // חיבור הזוג החדש
    dst[a] = b;
    dst[b] = a;
}

float optimize_plugboard(int r0, int r1, int r2, const uint8_t* in, int len, uint8_t* best_plug) {
    uint8_t current_plug[26];
    for (int i = 0; i < 26; ++i) current_plug[i] = i;

    std::vector<uint8_t> out(len);
    decrypt_cipher(in, out.data(), len, r0, r1, r2, current_plug);
    float current_score = score_text(out.data(), len);

    bool improved = true;
    while (improved) {
        improved = false;
        int best_p1 = -1, best_p2 = -1;
        float best_swap_score = current_score;

        for (int i = 0; i < 26; ++i) {
            for (int j = i + 1; j < 26; ++j) {
                uint8_t temp_plug[26];
                safe_swap_plugs(current_plug, temp_plug, i, j);

                decrypt_cipher(in, out.data(), len, r0, r1, r2, temp_plug);
                float sc = score_text(out.data(), len);

                if (sc > best_swap_score) {
                    best_swap_score = sc;
                    best_p1 = i;
                    best_p2 = j;
                }
            }
        }

        if (best_swap_score > current_score + 0.05f) {
            current_score = best_swap_score;
            safe_swap_plugs(current_plug, current_plug, best_p1, best_p2);
            improved = true;
        }
    }

    for (int i = 0; i < 26; ++i) best_plug[i] = current_plug[i];
    return current_score;
}

int main() {
    engine.init();
    init_bigrams();

    std::string default_cipher = "CPKGWQFOKNPRUPGLABAROEDQDYZHSTSGFHICLIHPSNMSASOOLBVRVBOPLYXAQWHXOJCIONKVISTEDNZJRCMNYRZ";
    std::cout << "Enter ciphertext (or press ENTER to use default test cipher):\n";
    std::string user_input;
    std::getline(std::cin, user_input);

    std::string cleaned_input = "";
    for (char c : user_input) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            cleaned_input += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    if (cleaned_input.empty()) {
        cleaned_input = default_cipher;
        std::cout << "[*] Using default ciphertext (" << cleaned_input.length() << " chars)\n";
    }

    int len = cleaned_input.length();
    std::vector<uint8_t> in(len);
    for (int i = 0; i < len; ++i) in[i] = cleaned_input[i] - 'A';

    std::cout << "[*] Starting Phase 1: Exhaustive Rotor Search (17,576 configurations)...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    const int TOTAL_STATES = 26 * 26 * 26;
    std::vector<RotorCandidate> all_candidates(TOTAL_STATES);

    #pragma omp parallel for schedule(static)
    for (int state = 0; state < TOTAL_STATES; ++state) {
        int r0 = state / (26 * 26);
        int r1 = (state / 26) % 26;
        int r2 = state % 26;

        uint8_t default_plug[26];
        for (int i = 0; i < 26; ++i) default_plug[i] = i;

        std::vector<uint8_t> temp_out(len);
        decrypt_cipher(in.data(), temp_out.data(), len, r0, r1, r2, default_plug);

        float ioc = calculate_ioc(temp_out.data(), len);
        float bi_sc = score_text(temp_out.data(), len) / len;
        float total_metric = (ioc * 100.0f) + bi_sc;

        all_candidates[state] = {r0, r1, r2, total_metric};
    }

    std::sort(all_candidates.begin(), all_candidates.end(), [](const RotorCandidate& a, const RotorCandidate& b) {
        return a.score > b.score;
    });

    // 1. חישוב אוטומטי מבוסס אורך שדר
int auto_top_k = 1000;
if (len >= 500) {
    auto_top_k = 150;       // שדר ארוך מאוד - התכנסות מהירה במיוחד
} else if (len >= 300) {
    auto_top_k = 250;       // שדר ארוך
} else if (len >= 150) {
    auto_top_k = 500;       // שדר בינוני
} else {
    auto_top_k = 1200;      // שדר קצר - נדרשת רשת ביטחון רחבה
}

// 2. מתן אפשרות למשתמש לבחור או לאשר את ברירת המחדל
std::cout << "\n[?] Recommended candidate count (TOP_K) for " << len << " chars: " << auto_top_k << "\n";
std::cout << "    Press ENTER to accept or enter custom count (e.g. 100-2000): ";

std::string k_input;
std::getline(std::cin, k_input);

int top_k = auto_top_k;
if (!k_input.empty()) {
    try {
        int parsed_val = std::stoi(k_input);
        if (parsed_val >= 1 && parsed_val <= 17576) {
            top_k = parsed_val;
        } else {
            std::cout << "[!] Value out of bounds, using auto value: " << auto_top_k << "\n";
        }
    } catch (...) {
        std::cout << "[!] Invalid input, using auto value: " << auto_top_k << "\n";
    }
}

std::cout << "[*] Phase 1 finished. Running safe Hill Climbing on Top " << top_k << " candidates...\n";
    
    std::cout << "[*] Phase 1 finished. Running safe Hill Climbing on Top " << top_k << " candidates...\n";

    float global_best_score = -1e9f;
    int best_r0 = -1, best_r1 = -1, best_r2 = -1;
    uint8_t final_plug[26];

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < top_k; ++i) {
        int r0 = all_candidates[i].r0;
        int r1 = all_candidates[i].r1;
        int r2 = all_candidates[i].r2;

        uint8_t thread_plug[26];
        float score = optimize_plugboard(r0, r1, r2, in.data(), len, thread_plug);

        #pragma omp critical
        {
            if (score > global_best_score) {
                global_best_score = score;
                best_r0 = r0;
                best_r1 = r1;
                best_r2 = r2;
                for (int k = 0; k < 26; ++k) final_plug[k] = thread_plug[k];
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    std::vector<uint8_t> decrypted(len);
    decrypt_cipher(in.data(), decrypted.data(), len, best_r0, best_r1, best_r2, final_plug);

    std::cout << "\n================ CRACK RESULTS ================\n";
    std::cout << "Rotor Settings : " << static_cast<char>('A' + best_r0)
                                    << static_cast<char>('A' + best_r1)
                                    << static_cast<char>('A' + best_r2) << "\n";
    std::cout << "Plugboard Pairs: ";
    int pairs_count = 0;
    for (int i = 0; i < 26; ++i) {
        if (final_plug[i] > i) {
            std::cout << static_cast<char>('A' + i) << static_cast<char>('A' + final_plug[i]) << " ";
            pairs_count++;
        }
    }
    std::cout << "(" << pairs_count << " pairs)\n";
    std::cout << "Decrypted Text : ";
    for (int i = 0; i < len; ++i) std::cout << static_cast<char>('A' + decrypted[i]);
    std::cout << "\nElapsed Time   : " << elapsed << " seconds\n";
    std::cout << "===============================================\n";

    return 0;
}