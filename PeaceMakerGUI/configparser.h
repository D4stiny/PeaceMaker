#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H
#include "shared.h"
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <codecvt>
#include <locale>

typedef struct FalsePositives
{
    std::vector<std::wstring> SourcePathFilter;
    std::vector<std::wstring> TargetPathFilter;
    std::vector<std::wstring> StackHistoryFilter;
} FALSE_POSITIVES;

class ConfigParser
{
    std::map<std::string, std::string> configMap;

public:
    ConfigParser(std::string ConfigFile);

    FALSE_POSITIVES GetConfigFalsePositives();
    std::vector<FILTER_INFO> GetConfigFilters();
};

#endif // CONFIGPARSER_H
