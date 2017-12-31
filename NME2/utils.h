#pragma once

#include "defs.h"

// A custom printf function that outputs red text to stderr
void perrf(const char* f, ...);

// A custom printf function that outputs yellow text to stdout
void pwarnf(const char* f, ...);

// Prints the version info to stdout
VersionInfo PrintVersionInfo(void);

// Tries to resolve the relative path to a full path, adds a backslash if the path belongs to a directory
path_t ResolveFullpath(char* buf, const char* path);

// Confirm if the user wants to overwrite a specific file
yn_response ConfirmOverwrite(const fpath path, bool multi);

// Checks if the file's signature corresponds to the given input
bool CheckFileSignature(fpath path, char sig[]);

// Get a specified command line argument
char* GetOption(char opt[], int argc, char* argv[]);

// Check if a specified command line argument exists
bool OptionExists(char opt[], int argc, char* argv[]);

// Returns the length of the longest string passed as argument
int LongestStrlen(const int n, ...);

// Formats and prints the file's options
void PrintSettingsVideo(File* file);

// Creates a full path from a fpath struct
char* MakePath(fpath path);
wchar_t* MakePathW(fpath path);

// Constructs the conversion command from a given File struct
char* ConstructCommand(File* file);

// Writes the buffer to the log, prepended with a timestamp
void WriteToLog(const char* str);

// Check if we support the given file and set the format
format GetFileFormat(const fpath path);

