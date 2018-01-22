#pragma once

#include <cstdint>
#include <fstream>

namespace {
    uint64_t read_64_le(unsigned char b[8]) {
        uint64_t v = 0;

        for (char i = 7; i >= 0; i--) {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint64_t read_64_le(std::ifstream &s) {
        char b[8];
        s.read(b, 8);

        return read_64_le(reinterpret_cast<unsigned char*>(b));
    }

    uint32_t read_32_le(unsigned char b[4]) {
        uint32_t v = 0;

        for (char i = 3; i >= 0; i--) {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_le(std::ifstream &s) {
        char b[4];
        s.read(b, 4);

        return read_32_le(reinterpret_cast<unsigned char*>(b));
    }

    float read_float_be(unsigned char b[4]) {
        return *reinterpret_cast<float*>(read_32_be(b));
    }

    uint64_t read_64_be(unsigned char b[4]) {
        uint64_t v = 0;

        for (char i = 0; i < 8; i++) {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint64_t read_64_be(std::ifstream &s) {
        char b[8];
        s.read(b, 8);
        return read_64_be(reinterpret_cast<unsigned char*>(b));
    }

    uint32_t read_32_be(unsigned char b[4]) {
        uint32_t v = 0;

        for (char i = 0; i < 4; i++) {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_be(std::ifstream &s) {
        char b[4];
        s.read(b, 4);

        return read_32_be(reinterpret_cast<unsigned char*>(b));
    }

    uint16_t read_16_be(unsigned char b[2]) {
        uint16_t v = 0;

        for (char i = 0; i < 2; i++) {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint16_t read_16_be(std::ifstream &s) {
        char b[2];
        s.read(b, 2);

        return read_16_be(reinterpret_cast<unsigned char*>(b));
    }

}