#pragma once

#include <qstring.h>

#include <stdexcept>

class FormatError : public std::runtime_error {
    public:
    FormatError(const char* err) : std::runtime_error(err) { }
    FormatError(QString err) : std::runtime_error(err.toStdString()) { }
};

class DATFormatError : public std::runtime_error {
    public:
    DATFormatError(const char* err) : std::runtime_error(err) { }
};