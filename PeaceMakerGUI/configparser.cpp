#include "configparser.h"

/**
 * @brief ConfigParser::ConfigParser - Read the config file.
 * @param ConfigFile - The config file to parse.
 */
ConfigParser::ConfigParser(std::string ConfigFile)
{
    std::ifstream configFile(ConfigFile);
    std::string currentLine;

    size_t findIndex;
    std::string currentPropertyName;
    std::string currentPropertyValue;

    if(configFile.is_open() == FALSE)
    {
        printf("Failed to open config file.");
        return;
    }

    while(std::getline(configFile, currentLine))
    {
        //
        // Check if the current line is a comment.
        //
        if(currentLine[0] == '#')
        {
            continue;
        }
        //
        // Check if there is anything on the current line.
        //
        if(currentLine.length() == 0)
        {
            continue;
        }

        findIndex = currentLine.find("=");
        currentPropertyName = currentLine.substr(0, findIndex);
        currentPropertyValue = currentLine.substr(findIndex + 1);
        configMap[currentPropertyName] = currentPropertyValue;
    }
}

/**
 * @brief ConfigParser::GetConfigFalsePositives - Parse false positive strings from the config.
 * @return False positives.
 */
FALSE_POSITIVES ConfigParser::GetConfigFalsePositives()
{
    FALSE_POSITIVES falsePositives;
    std::string currentCommaItem;
    std::stringstream falsePositiveStream;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    //
    // Check if there is anything for us to parse.
    //
    if(configMap.count("false_positive_sourcepath") == 0)
    {
        printf("ConfigParser!GetConfigFalsePositives: No source path false positives to parse.\n");
    }
    if(configMap.count("false_positive_targetpath") == 0)
    {
        printf("ConfigParser!GetConfigFalsePositives: No target path false positives to parse.\n");
    }
    if(configMap.count("false_positive_stackhistory") == 0)
    {
        printf("ConfigParser!GetConfigFalsePositives: No stack history false positives to parse.\n");
    }

    if(configMap.count("false_positive_sourcepath") != 0)
    {
        falsePositiveStream = std::stringstream(configMap["false_positive_sourcepath"]);
        //
        // Enumerate source path false positives.
        //
        while(falsePositiveStream.good())
        {
            std::getline(falsePositiveStream, currentCommaItem, ',');
            if(configMap.count(currentCommaItem) == 0)
            {
                printf("ConfigParser!GetConfigFalsePositives: false_positive_sourcepath had invalid false positive named %s.\n", currentCommaItem.c_str());
                continue;
            }
            falsePositives.SourcePathFilter.push_back(converter.from_bytes(configMap[currentCommaItem]));
        }
    }

    if(configMap.count("false_positive_targetpath") != 0)
    {
        falsePositiveStream = std::stringstream(configMap["false_positive_targetpath"]);
        //
        // Enumerate target path false positives.
        //
        while(falsePositiveStream.good())
        {
            std::getline(falsePositiveStream, currentCommaItem, ',');
            if(configMap.count(currentCommaItem) == 0)
            {
                printf("ConfigParser!GetConfigFalsePositives: false_positive_targetpath had invalid false positive named %s.\n", currentCommaItem.c_str());
                continue;
            }
            falsePositives.TargetPathFilter.push_back(converter.from_bytes(configMap[currentCommaItem]));
        }
    }

    if(configMap.count("false_positive_stackhistory") != 0)
    {
        falsePositiveStream = std::stringstream(configMap["false_positive_stackhistory"]);
        //
        // Enumerate source path false positives.
        //
        while(falsePositiveStream.good())
        {
            std::getline(falsePositiveStream, currentCommaItem, ',');
            if(configMap.count(currentCommaItem) == 0)
            {
                printf("ConfigParser!GetConfigFalsePositives: false_positive_stackhistory had invalid false positive named %s.\n", currentCommaItem.c_str());
                continue;
            }
            falsePositives.StackHistoryFilter.push_back(converter.from_bytes(configMap[currentCommaItem]));
        }
    }

    return falsePositives;
}

/**
 * @brief ConfigParser::GetConfigFilters - Parse filters from the config.
 * @return Parsed filter info.
 */
std::vector<FILTER_INFO> ConfigParser::GetConfigFilters()
{
    std::vector<std::string> filterNames;
    std::vector<FILTER_INFO> filters;
    FILTER_INFO currentFilter;
    std::string currentFilterName;
    std::string currentFilterType;
    std::wstring currentFilterContent;
    ULONG currentFilterFlags;
    std::stringstream filterStream;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    //
    // Check if there is anything for us to parse.
    //
    if(configMap.count("filters") == 0)
    {
        printf("ConfigParser!GetConfigFilters: No filters to parse.\n");
        return filters;
    }

    filterStream = std::stringstream(configMap["filters"]);
    //
    // Enumerate the filters.
    //
    while(filterStream.good())
    {
        std::getline(filterStream, currentFilterName, ',');
        filterNames.push_back(currentFilterName);
    }

    for(std::string filterName : filterNames)
    {
        //
        // Check for the content of the filter.
        //
        if(configMap.count(filterName + ".content") == 0)
        {
            printf("ConfigParser!GetConfigFilters: Failed to find content of filter %s.\n", filterName.c_str());
            continue;
        }
        //
        // Check for the type of the filter.
        //
        if(configMap.count(filterName + ".type") == 0)
        {
            printf("ConfigParser!GetConfigFilters: Failed to find type of filter %s.\n", filterName.c_str());
            continue;
        }
        //
        // Check for the flags of the filter.
        //
        if(configMap.count(filterName + ".flags") == 0)
        {
            printf("ConfigParser!GetConfigFilters: Failed to find flags of filter %s.\n", filterName.c_str());
            continue;
        }

        currentFilterContent = converter.from_bytes(configMap[filterName + ".content"]);
        currentFilterType = configMap[filterName + ".type"]; // f or r
        currentFilterFlags = 0;

        //
        // Parse the flags config.
        //
        if(configMap[filterName + ".flags"].find("e") != std::string::npos)
        {
            currentFilterFlags |= FILTER_FLAG_EXECUTE;
        }
        if(configMap[filterName + ".flags"].find("d") != std::string::npos)
        {
            currentFilterFlags |= FILTER_FLAG_DELETE;
        }
        if(configMap[filterName + ".flags"].find("w") != std::string::npos)
        {
            currentFilterFlags |= FILTER_FLAG_WRITE;
        }

        //
        // Parse the filter type.
        //
        if(currentFilterType[0] == 'f')
        {
            currentFilter.Type = FilesystemFilter;
        } else if(currentFilterType[0] == 'r')
        {
            currentFilter.Type = RegistryFilter;
        }
        currentFilter.Flags = currentFilterFlags;
        wcscpy_s(currentFilter.MatchString, currentFilterContent.c_str());
        currentFilter.MatchStringSize = currentFilterContent.size() + 2;
        filters.push_back(currentFilter);
    }
}

