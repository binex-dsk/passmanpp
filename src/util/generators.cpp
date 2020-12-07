#include <ios>
#include <iterator>
#include <algorithm>
#include <sodium/randombytes.h>

#include "generators.h"

uint32_t randomChar() {
    return 0x21U + randombytes_uniform(0x7EU - 0x20U);
}

std::string genPass(int length, bool capitals, bool numbers, bool symbols) {
    std::string passw, csChoice, ssInd;
    uint32_t csInd;

    for (int i = 0; i < length; ++i) {
        csInd = randomChar();
        ssInd = std::to_string(csInd);
        while (1) {
            csInd = randomChar();
            ssInd = std::to_string(csInd);
            char* cInd = reinterpret_cast<char*>(&csInd);
            if ((i != 0 && ssInd == std::to_string(passw[i - 1])) || csInd == 0x22U || csInd == 0x5CU) {
                continue;
            }
            if (capitals && find(capital, cInd)) {
                continue;
            }
            if (numbers && find(number, cInd)) {
                continue;
            }
            if (symbols && find(symbol, cInd)) {
                continue;
            }
            break;
        }
        passw.append(reinterpret_cast<char*>(&csInd));
    }
    return passw;
}

Botan::secure_vector<uint8_t> genKey(std::string path) {
    uint8_t length = 128 + randombytes_uniform(128);

    Botan::AutoSeeded_RNG rng;
    Botan::secure_vector<uint8_t> vec = rng.random_vec(length);

    std::ofstream pkpp(path, std::ios::binary);
    pkpp << toChar(vec);
    pkpp.flush();
    pkpp.close();

    return vec;
}
