#pragma once

#include <qstring.h>

#include <stdexcept>

class FormatError : public std::runtime_error {
    public:
    FormatError(const char* err) : std::runtime_error(err) { }
    FormatError(QString err) : std::runtime_error(err.toStdString()) { }
};

class DATFormatError : public FormatError {
    public:
    DATFormatError(const char* err) : FormatError(err) { }
    DATFormatError(QString err) : FormatError(err.toStdString()) { }
};

class USMFormatError : public FormatError {
    public:
    USMFormatError(const char* err) : FormatError(err) { }
    USMFormatError(QString err) : FormatError(err.toStdString()) { }
};